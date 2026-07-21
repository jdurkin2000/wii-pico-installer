/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define CFG_TUD_ENABLED         (1)

// Legacy RHPORT configuration
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT        (0)
#endif
// end legacy RHPORT

//------------------------
// DEVICE CONFIGURATION //
//------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  (64)
#endif

// Vendor specific class configuration
#define CFG_TUD_VENDOR           1
#define CFG_TUD_VENDOR_EP_BUFSIZE  64
#define CFG_TUD_VENDOR_RX_BUFSIZE  64
#define CFG_TUD_VENDOR_TX_BUFSIZE  64

// DFU RT does not required for this project
#define CFG_TUD_DFU_RT           0

// CDC class is not needed
#define CFG_TUD_CDC              0

// MSC class is not needed
#define CFG_TUD_MSC              0

// HID class is not needed
#define CFG_TUD_HID              0

// MIDI class is not needed
#define CFG_TUD_MIDI             0

// Audio class is not needed
#define CFG_TUD_AUDIO            0

// Video class is not needed
#define CFG_TUD_VIDEO            0

// BTH class is not needed
#define CFG_TUD_BTH              0

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
