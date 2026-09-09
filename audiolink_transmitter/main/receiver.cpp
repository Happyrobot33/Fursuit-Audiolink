#include <string.h>
#include <string>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb.h"
#include "cobs.h"
#include "receiver.h"
#include "shared.h"
#include "magic_enum/magic_enum.hpp"

static const char *TAG = "serial_rx";

/* Sized to match CFG_TUD_VENDOR_RX_BUFSIZE so one drain call empties the whole ring buffer,
 * instead of looping in small pieces and waking serial_rx_task once per piece */
typedef struct {
    uint8_t data[2048];
    size_t  len;
} UsbRxChunk;

static QueueHandle_t usb_rx_queue;

/* --- Custom descriptors: vendor-only device + MS OS 2.0 so Windows auto-binds WinUSB (no Zadig) --- */

#define MS_VENDOR_REQUEST_CODE  1

static tusb_desc_device_t const desc_device = {
    .bLength         = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB          = 0x0201, /* required for BOS/MS OS 2.0 descriptor to be queried */
    .bDeviceClass    = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor        = USB_ESPRESSIF_VID,
    .idProduct       = 0x4020,
    .bcdDevice       = 0x0100,
    .iManufacturer   = 0x01,
    .iProduct        = 0x02,
    .iSerialNumber   = 0x03,
    .bNumConfigurations = 0x01,
};

#define MS_OS_20_DESC_LEN  0xA2
#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static uint8_t const desc_bos[] = {
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, MS_VENDOR_REQUEST_CODE),
};

extern "C" uint8_t const *tud_descriptor_bos_cb(void)
{
    return desc_bos;
}

/* Single-function (non-composite) device: no configuration/function subset headers, just
 * header -> compatible ID -> registry property, per TinyUSB's "simple device" MS OS 2.0 example */
static uint8_t const desc_ms_os_20[] = {
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
    U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A),
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
    'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
    U16_TO_U8S_LE(0x0050),
    /* {975F44D9-0D08-43FD-8B3E-127CA8AFFF9D} - device interface GUID, used to open the device from Windows */
    '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
    '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
    '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
    '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,
};

static_assert(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "MS OS 2.0 descriptor size mismatch");

static char const *usb_string_descriptor[] = {
    (const char[]){0x09, 0x04}, /* 0: English (US) langid, not a C string */
    "Happyrobot33",             /* 1: Manufacturer */
    "AudioLink Transmitter",    /* 2: Product */
    "000001",                   /* 3: Serial */
};

extern "C" bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    if (stage != CONTROL_STAGE_SETUP) {
        return true;
    }
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR &&
        request->bRequest == MS_VENDOR_REQUEST_CODE && request->wIndex == 7) {
        uint16_t total_len;
        memcpy(&total_len, desc_ms_os_20 + 8, 2);
        return tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_ms_os_20, total_len);
    }
    return false;
}

extern "C" void tud_vendor_rx_cb(uint8_t itf, const uint8_t *buffer, uint16_t bufsize)
{
    (void)itf;
    (void)buffer;
    (void)bufsize;
    UsbRxChunk chunk;
    uint32_t n = tud_vendor_read(chunk.data, sizeof(chunk.data));
    if (n > 0) {
        chunk.len = n;
        if (xQueueSend(usb_rx_queue, &chunk, 0) != pdTRUE) {
            ESP_LOGW(TAG, "USB rx queue full, chunk dropped");
        }
    }
}

void serial_rx_task(void *arg)
{
    usb_rx_queue = xQueueCreate(8, sizeof(UsbRxChunk));

    tinyusb_config_t tusb_cfg{};
    tusb_cfg.device_descriptor = &desc_device;
    tusb_cfg.string_descriptor = usb_string_descriptor;
    tusb_cfg.string_descriptor_count = sizeof(usb_string_descriptor) / sizeof(usb_string_descriptor[0]);
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB WinUSB vendor data channel initialized");

    static uint8_t enc_buf[COBS_MAX_ENC];
    static uint8_t dec_buf[COBS_MAX_DEC];
    int enc_len = 0;
    UsbRxChunk chunk;

    uint32_t rx_frames = 0;
    uint64_t last_rate_log_us = esp_timer_get_time();

    while (true) {
        if (xQueueReceive(usb_rx_queue, &chunk, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        uint8_t *pos = chunk.data;
        uint8_t *chunk_end = chunk.data + chunk.len;
        while (pos < chunk_end) {
            uint8_t *delim = (uint8_t *)memchr(pos, 0x00, chunk_end - pos);
            uint8_t *seg_end = delim ? delim : chunk_end;
            size_t seg_len = seg_end - pos;

            /* Append this segment to enc_buf, capped to its capacity */
            size_t space = COBS_MAX_ENC - enc_len;
            if (seg_len > space) {
                ESP_LOGW(TAG, "Frame too large, discarding");
                memcpy(enc_buf + enc_len, pos, space);
                enc_len = COBS_MAX_ENC;
            } else {
                memcpy(enc_buf + enc_len, pos, seg_len);
                enc_len += seg_len;
            }

            if (delim) {
                /* End-of-frame delimiter: decode COBS and queue raw bytes */
                if (enc_len > 0 && enc_len <= COBS_MAX_ENC) {
                    cobs_decode_result decode_result = cobs_decode(dec_buf, sizeof(dec_buf), enc_buf, enc_len);
                    if (decode_result.status == COBS_DECODE_OK) {
                        size_t dec_len = decode_result.out_len;

                        QueuedAudioFrame queued_frame;
                        queued_frame.data_len = dec_len;
                        memcpy(queued_frame.data, dec_buf, dec_len);
                        rx_frames++;

                        if (xQueueSend(audio_queue, &queued_frame, 0) != pdTRUE) {
                            ESP_LOGW(TAG, "Audio queue full, frame dropped");
                        }
                    } else {
                        ESP_LOGW(TAG, "COBS decode error: status=%s, enc_len=%d, dec_len=%d",
                                 std::string(magic_enum::enum_name(decode_result.status)).c_str(),
                                 enc_len, (int)decode_result.out_len);
                    }
                }
                enc_len = 0;
                pos = delim + 1;
            } else {
                pos = seg_end;
            }
        }

        /* Log decoded-frame arrival rate every second, to isolate whether the bottleneck is
         * upstream of the USB rx path (host/cable) vs downstream (ESP-NOW send) */
        uint64_t now_us = esp_timer_get_time();
        if (now_us - last_rate_log_us >= 1000000ULL) {
            float elapsed_sec = (float)(now_us - last_rate_log_us) / 1000000.0f;
            ESP_LOGI(TAG, "Decoded frame rate: %.1f FPS", (float)rx_frames / elapsed_sec);
            rx_frames = 0;
            last_rate_log_us = now_us;
        }
    }
}
