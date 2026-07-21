#ifndef USB_PROTOCOL_H_
#define USB_PROTOCOL_H_

#include <stdbool.h>
#include <stdint.h>

#define USB_PROTOCOL_VERSION 1
#define USB_VENDOR_INTERFACE 0
#define USB_VENDOR_COMMAND_HEADER_SIZE 4
#define USB_VENDOR_MAX_TARGET_LENGTH 32
#define USB_VENDOR_MAX_RESPONSE_SIZE 64

// On-wire command header:
// byte 0: command id
// byte 1: protocol version
// byte 2: payload length, little-endian low byte
// byte 3: payload length, little-endian high byte
typedef struct __attribute__((packed))
{
    uint8_t command;
    uint8_t protocol;
    uint16_t payload_length;
} usb_vendor_command_header_t;

// Payload for USB_CMD_SUBMIT_JOB.
// The target field is not NUL-terminated on the wire; target_len is explicit.
typedef struct __attribute__((packed))
{
    uint32_t request_id;
    uint32_t expected_bytes;
    uint8_t target_len;
    char target[USB_VENDOR_MAX_TARGET_LENGTH];
} usb_vendor_submit_job_t;

#define USB_CMD_SUBMIT_JOB 0x01
#define USB_CMD_QUERY_STATUS 0x02
#define USB_CMD_QUERY_PROGRESS 0x03
#define USB_CMD_CANCEL_JOB 0x04
#define USB_CMD_RESET_JOB 0x05

typedef enum
{
    USB_JOB_STATE_IDLE = 0,
    USB_JOB_STATE_QUEUED = 1,
    USB_JOB_STATE_ACTIVE = 2,
    USB_JOB_STATE_COMPLETE = 3,
    USB_JOB_STATE_FAILED = 4,
    USB_JOB_STATE_CANCELED = 5,
} usb_job_state_t;

typedef enum
{
    USB_JOB_ERROR_NONE = 0,
    USB_JOB_ERROR_BAD_PROTOCOL = 1,
    USB_JOB_ERROR_BAD_LENGTH = 2,
    USB_JOB_ERROR_INVALID_COMMAND = 3,
    USB_JOB_ERROR_PAYLOAD_TOO_LARGE = 4,
} usb_job_error_t;

typedef struct __attribute__((packed))
{
    uint32_t request_id;
    uint32_t expected_bytes;
    uint32_t progress_bytes;
    uint8_t state;
    uint8_t error;
    uint8_t target_len;
    char target[USB_VENDOR_MAX_TARGET_LENGTH];
} usb_job_status_t;

// usb_job_status_t is the host-visible response packet.
// State values:
// - IDLE: no job active
// - QUEUED: request accepted, not started yet
// - ACTIVE: job in progress
// - COMPLETE: job finished successfully
// - FAILED: job ended with error
// - CANCELED: job canceled by host

void usb_protocol_init(void);
void usb_protocol_task(void);
void usb_protocol_clear_request(void);
void usb_protocol_set_job_state(usb_job_state_t state, usb_job_error_t error);
void usb_protocol_set_job_progress(uint32_t progress_bytes, uint32_t expected_bytes);
bool usb_protocol_set_job_request(uint32_t request_id,
                                  uint32_t expected_bytes,
                                  const char *target,
                                  uint8_t target_len);

#endif
