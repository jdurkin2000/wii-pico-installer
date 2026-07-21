#include "usb_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <tusb.h>

#include <pico/stdlib.h>

static usb_job_status_t current_job;
static uint8_t response_buffer[USB_VENDOR_MAX_RESPONSE_SIZE];
static uint16_t response_length;
static bool response_pending;

static uint8_t rx_buffer[USB_VENDOR_MAX_RESPONSE_SIZE];
static uint16_t rx_length;
static bool usb_protocol_mounted;

static void usb_protocol_reset_state(void)
{
    memset(&current_job, 0, sizeof(current_job));
    current_job.state = USB_JOB_STATE_IDLE;
    current_job.error = USB_JOB_ERROR_NONE;
}

static void usb_protocol_fill_response(uint8_t override_error)
{
    memset(response_buffer, 0, sizeof(response_buffer));
    memcpy(response_buffer, &current_job, sizeof(current_job));

    usb_job_status_t *status = (usb_job_status_t *)response_buffer;
    status->state = current_job.state;
    status->error = override_error;
    if (status->target_len > USB_VENDOR_MAX_TARGET_LENGTH)
    {
        status->target_len = USB_VENDOR_MAX_TARGET_LENGTH;
    }

    response_length = sizeof(usb_job_status_t);
}

static void usb_protocol_queue_response(void)
{
    response_pending = true;
}

static void usb_protocol_send_or_queue(void)
{
    if (response_pending)
    {
        return;
    }

    if (tud_mounted() && tud_vendor_write_available() >= response_length)
    {
        tud_vendor_write(response_buffer, response_length);
        tud_vendor_write_flush();
        return;
    }

    usb_protocol_queue_response();
}

static void usb_protocol_dispatch_status(uint8_t override_error)
{
    usb_protocol_fill_response(override_error);
    usb_protocol_send_or_queue();
}

static uint32_t usb_protocol_read_u32_le(uint8_t const *buffer)
{
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static void usb_protocol_handle_submit(uint8_t const *buffer, uint16_t length)
{
    if (length < 9)
    {
        usb_protocol_dispatch_status(USB_JOB_ERROR_BAD_LENGTH);
        return;
    }

    uint32_t request_id = usb_protocol_read_u32_le(&buffer[0]);
    uint32_t expected_bytes = usb_protocol_read_u32_le(&buffer[4]);
    uint8_t target_len = buffer[8];

    if ((uint16_t)(9 + target_len) != length)
    {
        usb_protocol_dispatch_status(USB_JOB_ERROR_BAD_LENGTH);
        return;
    }

    if (target_len > USB_VENDOR_MAX_TARGET_LENGTH)
    {
        target_len = USB_VENDOR_MAX_TARGET_LENGTH;
    }

    usb_protocol_reset_state();
    current_job.request_id = request_id;
    current_job.expected_bytes = expected_bytes;
    current_job.progress_bytes = 0;
    current_job.state = USB_JOB_STATE_QUEUED;
    current_job.error = USB_JOB_ERROR_NONE;
    current_job.target_len = target_len;
    if (target_len > 0)
    {
        memcpy(current_job.target, &buffer[9], target_len);
    }

    usb_protocol_dispatch_status(USB_JOB_ERROR_NONE);
}

static void usb_protocol_handle_command(uint8_t const *buffer, uint16_t length)
{
    if (length < USB_VENDOR_COMMAND_HEADER_SIZE)
    {
        usb_protocol_dispatch_status(USB_JOB_ERROR_BAD_LENGTH);
        return;
    }

    uint8_t command = buffer[0];
    uint8_t protocol = buffer[1];
    uint16_t payload_length = (uint16_t)buffer[2] | ((uint16_t)buffer[3] << 8);

    if (protocol != USB_PROTOCOL_VERSION)
    {
        usb_protocol_dispatch_status(USB_JOB_ERROR_BAD_PROTOCOL);
        return;
    }

    if ((uint16_t)(USB_VENDOR_COMMAND_HEADER_SIZE + payload_length) != length)
    {
        usb_protocol_dispatch_status(USB_JOB_ERROR_BAD_LENGTH);
        return;
    }

    switch (command)
    {
    case USB_CMD_SUBMIT_JOB:
        usb_protocol_handle_submit(&buffer[USB_VENDOR_COMMAND_HEADER_SIZE], payload_length);
        break;

    case USB_CMD_QUERY_STATUS:
    case USB_CMD_QUERY_PROGRESS:
        usb_protocol_dispatch_status(USB_JOB_ERROR_NONE);
        break;

    case USB_CMD_CANCEL_JOB:
        current_job.state = USB_JOB_STATE_CANCELED;
        current_job.error = USB_JOB_ERROR_NONE;
        usb_protocol_dispatch_status(USB_JOB_ERROR_NONE);
        break;

    case USB_CMD_RESET_JOB:
        usb_protocol_reset_state();
        usb_protocol_dispatch_status(USB_JOB_ERROR_NONE);
        break;

    default:
        usb_protocol_dispatch_status(USB_JOB_ERROR_INVALID_COMMAND);
        break;
    }
}

void usb_protocol_init(void)
{
    usb_protocol_reset_state();
    response_length = 0;
    response_pending = false;
    rx_length = 0;
    usb_protocol_mounted = false;
}

void usb_protocol_task(void)
{
    if (!response_pending)
    {
        return;
    }

    if (!tud_mounted())
    {
        return;
    }

    if (tud_vendor_write_available() < response_length)
    {
        return;
    }

    tud_vendor_write(response_buffer, response_length);
    tud_vendor_write_flush();
    response_pending = false;
}

void usb_protocol_clear_request(void)
{
    usb_protocol_reset_state();
}

void usb_protocol_set_job_state(usb_job_state_t state, usb_job_error_t error)
{
    current_job.state = (uint8_t)state;
    current_job.error = (uint8_t)error;
}

void usb_protocol_set_job_progress(uint32_t progress_bytes, uint32_t expected_bytes)
{
    current_job.progress_bytes = progress_bytes;
    current_job.expected_bytes = expected_bytes;
}

bool usb_protocol_set_job_request(uint32_t request_id,
                                  uint32_t expected_bytes,
                                  const char *target,
                                  uint8_t target_len)
{
    if (target_len > USB_VENDOR_MAX_TARGET_LENGTH)
    {
        target_len = USB_VENDOR_MAX_TARGET_LENGTH;
    }

    usb_protocol_reset_state();
    current_job.request_id = request_id;
    current_job.expected_bytes = expected_bytes;
    current_job.progress_bytes = 0;
    current_job.state = USB_JOB_STATE_QUEUED;
    current_job.error = USB_JOB_ERROR_NONE;
    current_job.target_len = target_len;

    if (target_len > 0 && target != NULL)
    {
        memcpy(current_job.target, target, target_len);
    }

    return true;
}

void tud_mount_cb(void)
{
    printf("tud_mount_cb\n");
    usb_protocol_mounted = true;
    usb_protocol_init();
}

void tud_umount_cb(void)
{
    printf("tud_umount_cb\n");
    usb_protocol_mounted = false;
    usb_protocol_init();
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    printf("tud_suspend_cb\n");
    usb_protocol_mounted = false;
    usb_protocol_init();
}

void tud_resume_cb(void)
{
    printf("tud_resume_cb\n");
    usb_protocol_mounted = true;
    usb_protocol_init();
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const *buffer, uint16_t bufsize)
{
    if (itf != USB_VENDOR_INTERFACE)
    {
        return;
    }

    printf("tud_vendor_rx_cb: itf=%u size=%u rx_length=%u\n", itf, bufsize, rx_length);
    for (uint16_t i = 0; i < bufsize; ++i)
    {
        printf("rx[%u]=0x%02x\n", (unsigned)(rx_length + i), buffer[i]);
    }

    if ((uint16_t)(rx_length + bufsize) > sizeof(rx_buffer))
    {
        printf("rx buffer overflow\n");
        usb_protocol_dispatch_status(USB_JOB_ERROR_PAYLOAD_TOO_LARGE);
        rx_length = 0;
        return;
    }

    memcpy(&rx_buffer[rx_length], buffer, bufsize);
    rx_length = (uint16_t)(rx_length + bufsize);

    if (rx_length < USB_VENDOR_COMMAND_HEADER_SIZE)
    {
        return;
    }

    uint16_t expected_length = USB_VENDOR_COMMAND_HEADER_SIZE +
                               ((uint16_t)rx_buffer[2] | ((uint16_t)rx_buffer[3] << 8));

    printf("rx header: cmd=0x%02x proto=0x%02x payload_len=%u expected_length=%u\n",
           rx_buffer[0], rx_buffer[1], ((uint16_t)rx_buffer[2] | ((uint16_t)rx_buffer[3] << 8)), expected_length);

    if (expected_length > sizeof(rx_buffer))
    {
        printf("rx expected length too large\n");
        usb_protocol_dispatch_status(USB_JOB_ERROR_PAYLOAD_TOO_LARGE);
        rx_length = 0;
        return;
    }

    if (rx_length < expected_length)
    {
        return;
    }

    usb_protocol_handle_command(rx_buffer, expected_length);

    if (rx_length > expected_length)
    {
        memmove(rx_buffer, &rx_buffer[expected_length], rx_length - expected_length);
        rx_length = (uint16_t)(rx_length - expected_length);
    }
    else
    {
        rx_length = 0;
    }

#if CFG_TUD_VENDOR_RX_BUFSIZE > 0
    tud_vendor_read_flush();
#endif
}

void tud_vendor_tx_cb(uint8_t itf, uint32_t sent_bytes)
{
    (void)itf;
    (void)sent_bytes;
}
