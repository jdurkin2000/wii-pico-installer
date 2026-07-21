# wii-pico-installer

This project is a TinyUSB vendor-class device for a Pico board. It does not perform downloads itself. Instead, it accepts host requests that describe a file/job to be handled elsewhere and exposes status and progress over USB.

## Protocol Documentation

**For the complete USB protocol specification, see the
[canonical specification in wii-installer-project](https://github.com/jdurkin2000/wii-installer-project/blob/main/USB_PROTOCOL.md).**

The shared specification is the source of truth for every component and tool that communicates with the Pico device.

## Quick Overview

The device exposes one vendor-specific interface with bulk OUT for commands and bulk IN for responses.

**Key Constants:**

- **Vendor ID:** `0xCAFE`
- **Product ID:** `0x4100`
- **Protocol Version:** `1`

**Supported Commands:**

- `0x01` Submit job
- `0x02` Query status
- `0x03` Query progress
- `0x04` Cancel job
- `0x05` Reset job

**Response Size:** 47 bytes (fixed)

The submit-job command accepts metadata describing a job to be handled elsewhere. The `target` field can be a URL, path, archive name, file ID, or any other host-defined identifier. The device stores the identifier and tracks progress for the request.

**Important:** The firmware tracks state and exposes it. It does not fetch data or write file contents itself.
