#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/queue.h"

/* Data channel is native USB CDC-ACM (TinyUSB); logs stay on the plain UART0 console */

/* Buffer sizes */
#define COBS_MAX_ENC        4096 * 2
#define COBS_MAX_DEC        4000 * 2
//This buffer size is to account for the additional data of the protobuf object that the sub packet is being encoded into.
#define ESP_NOW_BUFFER_MARGIN_SIZE 15
#define SUB_PACKET_DATA_COUNT (ESP_NOW_MAX_DATA_LEN_V2 - ESP_NOW_BUFFER_MARGIN_SIZE)

/* Queue message for passing decoded audio data to sender task */
typedef struct {
    uint8_t data[COBS_MAX_DEC];
    size_t data_len;
} QueuedAudioFrame;

/* Global audio queue (initialized in main.c) */
extern QueueHandle_t audio_queue;

/* COBS decoding is provided by espp/cobs library */

/* Task function declarations */
void espnow_sender_task(void *arg);
void serial_rx_task(void *arg);

#endif /* SHARED_H */
