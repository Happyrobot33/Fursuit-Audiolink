#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pb_decode.h"
#include "audiolink_data.pb.h"
#include "cobs.h"
#include "receiver.h"
#include "shared.h"

static const char *TAG = "serial_rx";

void serial_rx_task(void *arg)
{
    uart_config_t uart_cfg{};
    uart_cfg.baud_rate  = SERIAL_BAUD_RATE;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    ESP_ERROR_CHECK(uart_param_config(SERIAL_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(SERIAL_UART_NUM,
                                 SERIAL_UART_TX_PIN, SERIAL_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(SERIAL_UART_NUM, COBS_MAX_ENC * 3, 0, 10, &uart_queue, 0));

    ESP_LOGI(TAG, "UART initialized: num=%d, baud=%d, rx_pin=%d, tx_pin=%d",
             SERIAL_UART_NUM, SERIAL_BAUD_RATE, SERIAL_UART_RX_PIN, SERIAL_UART_TX_PIN);

    static uint8_t enc_buf[COBS_MAX_ENC];
    static uint8_t dec_buf[COBS_MAX_DEC];
    static uint8_t read_buf[512];
    int     enc_len = 0;
    uart_event_t event;

    while (true) {
        /* Wait for UART events */
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
            case UART_DATA:
                if (event.size > 0) {
                    size_t read_size = (event.size > sizeof(read_buf)) ? sizeof(read_buf) : event.size;
                    int n = uart_read_bytes(SERIAL_UART_NUM, read_buf, read_size, 0);
                    
                    for (int i = 0; i < n; i++) {
                        uint8_t byte = read_buf[i];
                        ESP_LOGD(TAG, "UART RX byte: 0x%02x", byte);

                        if (byte == 0x00) {
                            /* End-of-frame delimiter: decode and queue Audiolink_Data */
                            if (enc_len > 0) {
                                ESP_LOGI(TAG, "Frame received, size=%d bytes", enc_len);
                                cobs_decode_result decode_result = cobs_decode(dec_buf, sizeof(dec_buf), enc_buf, enc_len);
                                if (decode_result.status == COBS_DECODE_OK) {
                                    size_t dec_len = decode_result.out_len;
                                    PROTO_Audiolink_Data audio_data = PROTO_Audiolink_Data_init_zero;
                                    pb_istream_t stream = pb_istream_from_buffer(dec_buf, dec_len);
                                    bool status = pb_decode(&stream, PROTO_Audiolink_Data_fields, &audio_data);
                                    
                                    if (status) {
                                        ESP_LOGI(TAG, "Audiolink_Data decoded successfully");
                                        
                                        QueuedAudioFrame queued_frame;
                                        queued_frame.data_len = dec_len;
                                        memcpy(queued_frame.data, dec_buf, dec_len);
                                        
                                        if (xQueueSend(audio_queue, &queued_frame, 0) != pdTRUE) {
                                            ESP_LOGW(TAG, "Audio queue full, frame dropped");
                                        }
                                    } else {
                                        ESP_LOGW(TAG, "Decode failed: %s", PB_GET_ERROR(&stream));
                                    }
                                } else {
                                    ESP_LOGW(TAG, "COBS decode error: status=%d", decode_result.status);
                                }
                            }
                            enc_len = 0;
                        } else {
                            if (enc_len < COBS_MAX_ENC) {
                                enc_buf[enc_len++] = byte;
                            } else {
                                ESP_LOGW(TAG, "Frame too large, discarding");
                                enc_len = 0;
                            }
                        }
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART RX FIFO overflow");
                uart_flush_input(SERIAL_UART_NUM);
                enc_len = 0;
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART RX buffer full");
                uart_flush_input(SERIAL_UART_NUM);
                enc_len = 0;
                break;

            case UART_BREAK:
                ESP_LOGD(TAG, "UART break signal");
                break;

            case UART_PARITY_ERR:
            case UART_FRAME_ERR:
                ESP_LOGW(TAG, "UART error: parity or frame error");
                break;

            default:
                break;
        }
    }
}
