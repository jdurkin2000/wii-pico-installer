# USB Protocol Specification

**Source of Truth for wii-pico-installer USB Protocol**

This document is the canonical reference for the USB protocol implemented in the wii-pico-installer firmware. All agents and developers should reference this file when working with the Pico device or host-side code.

## Device Identifiers

- **Vendor ID (VID):** `0xCAFE`
- **Product ID (PID):** `0x4100`
- **Interface Class:** `0xFF` (Vendor-specific)
- **Protocol Version:** `1`

## USB Interface

The device exposes one vendor-specific interface with:

- **Bulk OUT:** Host → Device commands
- **Bulk IN:** Device → Host responses

## Command Packet Format

All host commands begin with a 4-byte header followed by a variable-length payload:

| Byte | Field            | Type   | Description                  |
| ---- | ---------------- | ------ | ---------------------------- |
| 0    | Command ID       | u8     | Command to execute           |
| 1    | Protocol Version | u8     | Protocol version (must be 1) |
| 2-3  | Payload Length   | u16-LE | Length of payload in bytes   |
| 4+   | Payload          | bytes  | Command-specific data        |

### Supported Commands

#### 0x01: Submit Job

Submits a job request with metadata for processing.

**Payload Layout:**

| Field            | Type      | Offset | Description                            |
| ---------------- | --------- | ------ | -------------------------------------- |
| `request_id`     | u32       | 0      | Unique request identifier              |
| `expected_bytes` | u32       | 4      | Total expected work in bytes           |
| `target_len`     | u8        | 8      | Length of target field (0-32)          |
| `target`         | bytes[32] | 9      | Target identifier (not NUL-terminated) |

**Payload Length:** 41 bytes

**Constraints:**

- `target_len` must not exceed 32
- `target` field is not NUL-terminated on the wire
- Must clamp and validate `target_len` before copying

#### 0x02: Query Status

Queries the current job status.

**Payload:** None (payload_length = 0)

#### 0x03: Query Progress

Queries the current job progress.

**Payload:** None (payload_length = 0)

#### 0x04: Cancel Job

Cancels the currently active job.

**Payload:** None (payload_length = 0)

#### 0x05: Reset Job

Resets the job state to idle.

**Payload:** None (payload_length = 0)

## Response Packet Format

Responses are a fixed-size packed status record (43 bytes).

| Field            | Type      | Offset | Description                                 |
| ---------------- | --------- | ------ | ------------------------------------------- |
| `request_id`     | u32       | 0      | Current job ID                              |
| `expected_bytes` | u32       | 4      | Total expected work in bytes (0 if unknown) |
| `progress_bytes` | u32       | 8      | Current progress in bytes                   |
| `state`          | u8        | 12     | Current job state (see State Values below)  |
| `error`          | u8        | 13     | Error code (see Error Values below)         |
| `target_len`     | u8        | 14     | Length of target field (0-32)               |
| `target`         | bytes[32] | 15     | Target identifier (not NUL-terminated)      |

**Total Response Size:** 43 bytes

**Important Constraints:**

- `target` is a fixed-size wire field and is **not NUL-terminated** on the device side
- Always clamp and validate `target_len` before copying in host code
- Treat non-matching request IDs, short reads, or unexpected lengths as protocol failures

## State Values

| State    | Value | Description                |
| -------- | ----- | -------------------------- |
| Idle     | 0     | No job active              |
| Queued   | 1     | Job queued, awaiting start |
| Active   | 2     | Job currently processing   |
| Complete | 3     | Job completed successfully |
| Failed   | 4     | Job failed                 |
| Canceled | 5     | Job canceled by host       |

## Error Values

| Error             | Value | Description                                   |
| ----------------- | ----- | --------------------------------------------- |
| None              | 0     | No error                                      |
| Bad protocol      | 1     | Protocol version mismatch or malformed header |
| Bad length        | 2     | Invalid payload length                        |
| Invalid command   | 3     | Unknown command ID                            |
| Payload too large | 4     | Payload exceeds buffer                        |

## Host Communication Flow

1. **Submit Job:** Send `0x01` with request metadata
2. **Query Status/Progress:** Send `0x02` or `0x03` as needed to check job state
3. **Cancel or Reset:** Send `0x04` (cancel) or `0x05` (reset) to stop or clear the request

The firmware tracks state and exposes it. It **does not** fetch data or write file contents itself.

## Implementation Notes

- **Protocol version:** Always set byte 1 to `1` in all commands
- **Byte order:** All multi-byte fields use little-endian byte order
- **Buffer alignment:** USB transfer buffers must be aligned (e.g., 32-byte alignment for libogc bulk I/O) to avoid transfer failures
- **Request ID matching:** Host must validate that response `request_id` matches the submitted `request_id`
- **No NUL termination:** The `target` field on the wire is never NUL-terminated; use `target_len` to determine valid bytes

## Versioning

**Current Version:** 1

If the protocol changes, this file must be updated along with:

- Firmware implementation (`usb_protocol.c`, `usb_protocol.h`)
- Host test tool (`tools/test_device.py`)
- Wii installer (`include/wii_pico_protocol.hpp`, `source/wii_pico_protocol.cpp`)
- Any agent documentation that references the protocol
