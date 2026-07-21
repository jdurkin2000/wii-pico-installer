# AGENTS.md

## Project Summary

This repository contains the `wii-pico-installer` firmware for a Raspberry Pi Pico and a small host-side USB test tool.

The device is a single vendor-specific USB interface with bulk IN/OUT endpoints. It does not download files itself; it only tracks request metadata, status, and progress.

## Working Rules

- Keep firmware changes focused and minimal.
- Treat `main.c` as startup and polling glue only.
- Keep USB protocol state and TinyUSB callbacks in `usb_protocol.c` / `usb_protocol.h`.
- Keep USB descriptors conservative. Do not reintroduce a device qualifier callback for the RP2040 unless there is a concrete reason.
- Preserve the current vendor VID/PID unless a change is required for the host or target platform.
- Do not edit build outputs under `build/`.

## Firmware Layout

- `main.c`: board init, LED heartbeat, `tud_task()`, protocol polling.
- `usb_protocol.c`: TinyUSB callbacks and protocol state machine.
- `usb_descriptors.c`: device/config/string descriptors.
- `tusb_config.h`: TinyUSB feature toggles and buffer sizes.

## Host Test Layout

- `tools/test_device.py` is the host-side smoke test.
- Use `--simulate` when you want to exercise the protocol on PC without hardware.
- Use real-device mode only when the Pico is connected and enumerating correctly.
- On Windows, real-device mode requires a libusb-compatible backend such as WinUSB plus `libusb-package` in the Python environment.

## Build and Test

- Build the firmware with the Pico Ninja task or the equivalent `ninja -C build` flow.
- Flash/load the firmware with the existing picotool task.
- Run the host smoke test with:

  ```powershell
  .\.venv\Scripts\python.exe tools\test_device.py --simulate
  ```

- Run real-device testing with:

  ```powershell
  .\.venv\Scripts\python.exe tools\test_device.py
  ```

## Protocol Notes

- The canonical protocol specification is
  [USB_PROTOCOL.md in wii-installer-project](https://github.com/jdurkin2000/wii-installer-project/blob/main/USB_PROTOCOL.md).
- Command packets use a 4-byte header: command, protocol version, payload length.
- The current protocol version is `1`.
- Supported commands are submit job, query status, query progress, cancel job, and reset job.
- Status packets are fixed-size, packed 47-byte records.
- If you change the protocol, update `usb_protocol.h`, `tools/test_device.py`, `README.md`, and the canonical specification together, then coordinate the corresponding host changes.

## Editing Guidance

- Prefer `apply_patch` for edits.
- Keep changes small and verifiable.
- After firmware changes, rebuild before assuming the USB behavior changed.
- If USB enumeration fails, check descriptors first before changing host-side code.
