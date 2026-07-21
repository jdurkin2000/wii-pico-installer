# wii-pico-installer

This project is a TinyUSB vendor-class device for a Pico board. It does not perform downloads itself. Instead, it accepts host requests that describe a file/job to be handled elsewhere and exposes status and progress over USB.

## Protocol Documentation

**For complete USB protocol specification, see [USB_PROTOCOL.md](USB_PROTOCOL.md)**

This is the canonical source of truth for the USB protocol and should be referenced by all agents, developers, and tooling that interact with the Pico device.

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

**Response Size:** 43 bytes (fixed)

The submit-job command accepts metadata describing a job to be handled elsewhere. The `target` field can be a URL, path, archive name, file ID, or any other host-defined identifier. The device stores the identifier and tracks progress for the request.

**Important:** The firmware tracks state and exposes it. It does not fetch data or write file contents itself.
