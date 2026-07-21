# wii-pico-installer

This project is a TinyUSB vendor-class device for a Pico board. It does not perform downloads itself. Instead, it accepts host requests that describe a file/job to be handled elsewhere and exposes status and progress over USB.

## USB model

The device exposes one vendor-specific interface with bulk OUT for commands and bulk IN for responses.

## Command packet

All host commands begin with a 4-byte header:

| Byte | Meaning                       |
| ---- | ----------------------------- |
| 0    | Command id                    |
| 1    | Protocol version              |
| 2-3  | Payload length, little-endian |

Supported command ids:

| Command        | Value | Payload                                                                 |
| -------------- | ----- | ----------------------------------------------------------------------- |
| Submit job     | 0x01  | `request_id:u32`, `expected_bytes:u32`, `target_len:u8`, `target:bytes` |
| Query status   | 0x02  | none                                                                    |
| Query progress | 0x03  | none                                                                    |
| Cancel job     | 0x04  | none                                                                    |
| Reset job      | 0x05  | none                                                                    |

The submit-job payload is metadata only. The `target` field can be a URL, path, archive name, file ID, or any other host-defined identifier. The device stores the identifier and tracks progress for the request.

## Response packet

Responses are a packed status record:

| Field            | Type      | Meaning                                           |
| ---------------- | --------- | ------------------------------------------------- |
| `request_id`     | u32       | Current job id                                    |
| `expected_bytes` | u32       | Total expected work, if known                     |
| `progress_bytes` | u32       | Current progress                                  |
| `state`          | u8        | Idle, queued, active, complete, failed, canceled  |
| `error`          | u8        | Error code                                        |
| `target_len`     | u8        | Length of the target field                        |
| `target`         | bytes[32] | Target identifier, not NUL-terminated on the wire |

## State values

| State    | Value |
| -------- | ----- |
| Idle     | 0     |
| Queued   | 1     |
| Active   | 2     |
| Complete | 3     |
| Failed   | 4     |
| Canceled | 5     |

## Error values

| Error             | Value |
| ----------------- | ----- |
| None              | 0     |
| Bad protocol      | 1     |
| Bad length        | 2     |
| Invalid command   | 3     |
| Payload too large | 4     |

## Host flow

1. Send `Submit job` with metadata for the request.
2. Query `Status` or `Progress` as needed.
3. Use `Cancel job` or `Reset job` to stop or clear the current request.

The firmware currently tracks state and exposes it. It does not fetch data or write the file contents itself.
