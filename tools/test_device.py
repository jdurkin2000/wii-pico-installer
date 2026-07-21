#!/usr/bin/env python3
"""Simple host-side smoke test for the wii-pico-installer vendor protocol.

This script checks that the Pico device is plugged in, accepts a vendor
command, and returns a valid packed status response.

Dependencies:
    pip install pyusb

On Windows, the device must be bound to a libusb-compatible driver such as
WinUSB for the vendor interface.

Use --simulate to run the same protocol checks against an in-process mock
device when you want to test host-side code without plugging in hardware.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from typing import Protocol

import usb.core
import usb.util
from libusb_package import get_libusb1_backend

VID_DEFAULT = 0xCafe
PID_DEFAULT = 0x4100
INTERFACE_NUMBER = 0
PROTOCOL_VERSION = 1
RESPONSE_SIZE = struct.calcsize("<III BBB 32s")

USB_CMD_SUBMIT_JOB = 0x01
USB_CMD_QUERY_STATUS = 0x02
USB_CMD_QUERY_PROGRESS = 0x03
USB_CMD_CANCEL_JOB = 0x04
USB_CMD_RESET_JOB = 0x05

STATE_NAMES = {
    0: "idle",
    1: "queued",
    2: "active",
    3: "complete",
    4: "failed",
    5: "canceled",
}

ERROR_NAMES = {
    0: "none",
    1: "bad_protocol",
    2: "bad_length",
    3: "invalid_command",
    4: "payload_too_large",
}


@dataclass
class Status:
    request_id: int
    expected_bytes: int
    progress_bytes: int
    state: int
    error: int
    target_len: int
    target: str

    @property
    def state_name(self) -> str:
        return STATE_NAMES.get(self.state, f"unknown({self.state})")

    @property
    def error_name(self) -> str:
        return ERROR_NAMES.get(self.error, f"unknown({self.error})")


class DeviceError(RuntimeError):
    pass


class Transport(Protocol):
    def transact(self, command: int, payload: bytes = b"") -> Status:
        ...


class UsbTransport:
    def __init__(self, vid: int, pid: int):
        self.vid = vid
        self.pid = pid
        self.device = None
        self.endpoint_out = None
        self.endpoint_in = None

    def open(self) -> None:
        backend = get_libusb1_backend()
        if backend is None:
            raise DeviceError(
                "PyUSB could not load a libusb backend. On Windows, install a libusb-compatible backend "
                "such as libusb-package or provide libusb-1.0.dll on PATH."
            )

        self.device = usb.core.find(idVendor=self.vid, idProduct=self.pid, backend=backend)
        if self.device is None:
            raise DeviceError(f"device not found (VID=0x{self.vid:04x}, PID=0x{self.pid:04x})")

    def configure(self) -> None:
        assert self.device is not None

        try:
            self.device.set_configuration()
        except usb.core.USBError:
            # The device may already be configured on some systems.
            pass

        try:
            if hasattr(self.device, "is_kernel_driver_active") and self.device.is_kernel_driver_active(INTERFACE_NUMBER):
                try:
                    self.device.detach_kernel_driver(INTERFACE_NUMBER)
                except usb.core.USBError:
                    pass
        except (NotImplementedError, AttributeError):
            pass

        usb.util.claim_interface(self.device, INTERFACE_NUMBER)
        self._discover_endpoints()

    def release(self) -> None:
        if self.device is None:
            return

        try:
            usb.util.release_interface(self.device, INTERFACE_NUMBER)
        except Exception:
            pass
        try:
            usb.util.dispose_resources(self.device)
        except Exception:
            pass

    def _discover_endpoints(self) -> None:
        assert self.device is not None

        self.endpoint_out = None
        self.endpoint_in = None

        cfg = self.device.get_active_configuration()
        if cfg is None:
            raise DeviceError("device is not configured")

        interfaces = list(cfg)
        if not interfaces:
            raise DeviceError("configuration has no interfaces")

        for interface in interfaces:
            if getattr(interface, "bInterfaceNumber", None) != INTERFACE_NUMBER:
                continue

            for endpoint in interface:
                endpoint_address = getattr(endpoint, "bEndpointAddress", None)
                endpoint_attributes = getattr(endpoint, "bmAttributes", None)
                if endpoint_address is None or endpoint_attributes != 0x02:
                    continue

                if endpoint_address & 0x80:
                    self.endpoint_in = endpoint_address
                else:
                    self.endpoint_out = endpoint_address

        if self.endpoint_out is None or self.endpoint_in is None:
            raise DeviceError(
                f"could not discover bulk endpoints from descriptors for interface {INTERFACE_NUMBER}"
            )

    def transact(self, command: int, payload: bytes = b"") -> Status:
        assert self.device is not None
        if self.endpoint_out is None or self.endpoint_in is None:
            self._discover_endpoints()

        packet = pack_command(command, payload)
        written = self.device.write(self.endpoint_out, packet, timeout=1000)
        if written != len(packet):
            raise DeviceError(f"short write: expected {len(packet)} bytes, wrote {written}")

        raw = bytes(self.device.read(self.endpoint_in, RESPONSE_SIZE, timeout=1000))
        return parse_status(raw)


class SimulatedTransport:
    def __init__(self):
        self.status = Status(
            request_id=0,
            expected_bytes=0,
            progress_bytes=0,
            state=0,
            error=0,
            target_len=0,
            target="",
        )

    def transact(self, command: int, payload: bytes = b"") -> Status:
        self._handle_command(command, payload)
        return self.status

    def _handle_command(self, command: int, payload: bytes) -> None:
        if command == USB_CMD_RESET_JOB:
            self.status = Status(0, 0, 0, 0, 0, 0, "")
            return

        if command == USB_CMD_SUBMIT_JOB:
            if len(payload) < 9:
                raise DeviceError("simulate: bad submit payload")

            request_id, expected_bytes, target_len = struct.unpack_from("<IIB", payload, 0)
            target = payload[9:9 + target_len].decode("utf-8", errors="replace")
            self.status = Status(request_id, expected_bytes, 0, 1, 0, target_len, target)
            return

        if command == USB_CMD_QUERY_STATUS or command == USB_CMD_QUERY_PROGRESS:
            return

        if command == USB_CMD_CANCEL_JOB:
            self.status.state = 5
            self.status.error = 0
            return

        raise DeviceError(f"simulate: unsupported command 0x{command:02x}")


def pack_command(command: int, payload: bytes = b"") -> bytes:
    if len(payload) > 0xFFFF:
        raise ValueError("payload too large")
    return struct.pack("<BBH", command, PROTOCOL_VERSION, len(payload)) + payload


def build_submit_payload(request_id: int, expected_bytes: int, target: str) -> bytes:
    target_bytes = target.encode("utf-8")
    if len(target_bytes) > 32:
        raise ValueError("target must be 32 bytes or less after UTF-8 encoding")
    return struct.pack("<II B", request_id, expected_bytes, len(target_bytes)) + target_bytes


def parse_status(raw: bytes) -> Status:
    if len(raw) < RESPONSE_SIZE:
        raise DeviceError(f"short response: expected {RESPONSE_SIZE} bytes, got {len(raw)}")

    request_id, expected_bytes, progress_bytes, state, error, target_len, target_raw = struct.unpack(
        "<III BBB 32s", raw[:RESPONSE_SIZE]
    )
    target = target_raw[:target_len].decode("utf-8", errors="replace")
    return Status(
        request_id=request_id,
        expected_bytes=expected_bytes,
        progress_bytes=progress_bytes,
        state=state,
        error=error,
        target_len=target_len,
        target=target,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise DeviceError(message)


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke test the wii-pico-installer USB protocol")
    parser.add_argument("--vid", type=lambda value: int(value, 0), default=VID_DEFAULT)
    parser.add_argument("--pid", type=lambda value: int(value, 0), default=PID_DEFAULT)
    parser.add_argument("--request-id", type=lambda value: int(value, 0), default=1)
    parser.add_argument("--expected-bytes", type=lambda value: int(value, 0), default=123456)
    parser.add_argument("--target", default="smoke-test-target")
    parser.add_argument("--simulate", action="store_true", help="run against an in-process mock device")
    args = parser.parse_args()

    if args.simulate:
        transport: Transport = SimulatedTransport()
        print("Running in simulation mode")
    else:
        transport = UsbTransport(args.vid, args.pid)
        transport.open()
        assert isinstance(transport, UsbTransport)
        transport.configure()

    try:
        if not args.simulate:
            print(f"Found device VID=0x{args.vid:04x} PID=0x{args.pid:04x}")

        status = transport.transact(USB_CMD_RESET_JOB)
        require(status.error == 0, f"reset failed: {status.error_name}")

        payload = build_submit_payload(args.request_id, args.expected_bytes, args.target)
        status = transport.transact(USB_CMD_SUBMIT_JOB, payload)
        print(
            "submit -> state={state} error={error} request_id={request_id} target={target!r}".format(
                state=status.state_name,
                error=status.error_name,
                request_id=status.request_id,
                target=status.target,
            )
        )
        require(status.state == 1, f"expected queued state after submit, got {status.state_name}")
        require(status.request_id == args.request_id, "request id mismatch after submit")
        require(status.expected_bytes == args.expected_bytes, "expected-bytes mismatch after submit")
        require(status.target == args.target, "target mismatch after submit")

        status = transport.transact(USB_CMD_QUERY_STATUS)
        print(
            "status -> state={state} error={error} progress={progress}/{expected} target={target!r}".format(
                state=status.state_name,
                error=status.error_name,
                progress=status.progress_bytes,
                expected=status.expected_bytes,
                target=status.target,
            )
        )
        require(status.request_id == args.request_id, "request id mismatch on query")
        require(status.state in (1, 2, 3, 4, 5), "unexpected state value")

        status = transport.transact(USB_CMD_CANCEL_JOB)
        print(f"cancel -> state={status.state_name} error={status.error_name}")
        require(status.state == 5, f"expected canceled state after cancel, got {status.state_name}")

        status = transport.transact(USB_CMD_RESET_JOB)
        print(f"reset -> state={status.state_name} error={status.error_name}")
        require(status.state == 0, f"expected idle state after reset, got {status.state_name}")

        print("PASS: device responded to all protocol checks")
        return 0
    except (usb.core.USBError, DeviceError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    finally:
        if isinstance(transport, UsbTransport):
            transport.release()


if __name__ == "__main__":
    raise SystemExit(main())
