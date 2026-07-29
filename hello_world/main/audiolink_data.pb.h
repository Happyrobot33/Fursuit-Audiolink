#ifndef AUDIOLINK_DATA_PB_H
#define AUDIOLINK_DATA_PB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of elements in repeated fields */
#define HISTORY_BASS_COUNT 128
#define HISTORY_LOWMID_COUNT 128
#define HISTORY_HIGHMID_COUNT 128
#define HISTORY_TREBLE_COUNT 128
#define SUB_PACKET_DATA_COUNT 1024  /* ESP-NOW v2 large packet support */

/* Message definitions */
typedef struct {
    float bass[HISTORY_BASS_COUNT];
    size_t bass_count;
    
    float lowmid[HISTORY_LOWMID_COUNT];
    size_t lowmid_count;
    
    float highmid[HISTORY_HIGHMID_COUNT];
    size_t highmid_count;
    
    float treble[HISTORY_TREBLE_COUNT];
    size_t treble_count;
} History;

typedef struct {
    History history;
} Audiolink_Data;

typedef struct {
    int32_t packet_index;
    uint8_t data[SUB_PACKET_DATA_COUNT];
    size_t data_count;
} Sub_Packet;

/* Stream types for advanced I/O */
typedef struct {
    const uint8_t *buf;
    size_t pos;
    size_t size;
} pb_istream_t;

typedef struct {
    uint8_t *buf;
    size_t pos;
    size_t size;
} pb_ostream_t;

/* Function declarations */
pb_istream_t pb_istream_from_buffer(const uint8_t *buf, size_t size);
pb_ostream_t pb_ostream_from_buffer(uint8_t *buf, size_t size);

void sub_packet_init(Sub_Packet *pkt);
void audiolink_data_init(Audiolink_Data *data);

int audiolink_data_encode(uint8_t *buf, size_t buf_size, const Audiolink_Data *msg);
int audiolink_data_decode(const uint8_t *buf, size_t buf_size, Audiolink_Data *msg);

int sub_packet_encode(uint8_t *buf, size_t buf_size, const Sub_Packet *msg);
int sub_packet_decode(const uint8_t *buf, size_t buf_size, Sub_Packet *msg);

#ifdef __cplusplus
}
#endif

#endif
