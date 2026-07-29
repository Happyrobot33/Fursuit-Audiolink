#include "audiolink_data.pb.h"
#include <string.h>

/* Protobuf wire type constants */
#define WIRE_TYPE_VARINT 0
#define WIRE_TYPE_64BIT 1
#define WIRE_TYPE_LENGTH_DELIMITED 2
#define WIRE_TYPE_32BIT 5

/* Helper functions for varint encoding/decoding */
static int encode_varint(uint8_t *buf, size_t buf_size, uint32_t value) {
    size_t written = 0;
    while (value >= 0x80 && written < buf_size) {
        buf[written++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    if (written >= buf_size) return -1;
    buf[written++] = (uint8_t)value;
    return (int)written;
}

static int decode_varint(const uint8_t *buf, size_t buf_size, uint32_t *value) {
    size_t i = 0;
    *value = 0;
    uint32_t shift = 0;
    
    while (i < buf_size) {
        uint8_t byte = buf[i++];
        *value |= ((uint32_t)(byte & 0x7F)) << shift;
        if ((byte & 0x80) == 0) {
            return (int)i;
        }
        shift += 7;
        if (shift >= 32) return -1;
    }
    return -1;
}

static int encode_tag_and_type(uint8_t *buf, size_t buf_size, int field_num, int wire_type) {
    uint32_t tag = (field_num << 3) | wire_type;
    return encode_varint(buf, buf_size, tag);
}

static int decode_tag_and_type(const uint8_t *buf, size_t buf_size, int *field_num, int *wire_type) {
    uint32_t tag;
    int n = decode_varint(buf, buf_size, &tag);
    if (n < 0) return -1;
    *field_num = tag >> 3;
    *wire_type = tag & 0x07;
    return n;
}

/* Float encoding/decoding (32-bit IEEE 754) */
static void encode_float(uint8_t *buf, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(float));
    buf[0] = (uint8_t)(bits & 0xFF);
    buf[1] = (uint8_t)((bits >> 8) & 0xFF);
    buf[2] = (uint8_t)((bits >> 16) & 0xFF);
    buf[3] = (uint8_t)((bits >> 24) & 0xFF);
}

static float decode_float(const uint8_t *buf) {
    uint32_t bits = buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(float));
    return value;
}

/* Initialization functions */
pb_istream_t pb_istream_from_buffer(const uint8_t *buf, size_t size) {
    return (pb_istream_t){.buf = buf, .pos = 0, .size = size};
}

pb_ostream_t pb_ostream_from_buffer(uint8_t *buf, size_t size) {
    return (pb_ostream_t){.buf = buf, .pos = 0, .size = size};
}

void sub_packet_init(Sub_Packet *pkt) {
    memset(pkt, 0, sizeof(Sub_Packet));
}

void audiolink_data_init(Audiolink_Data *data) {
    memset(data, 0, sizeof(Audiolink_Data));
}

/* Audiolink_Data encoding */
int audiolink_data_encode(uint8_t *buf, size_t buf_size, const Audiolink_Data *msg) {
    size_t pos = 0;
    
    /* Encode History (field 1) */
    if (msg->history.bass_count > 0 || msg->history.lowmid_count > 0 ||
        msg->history.highmid_count > 0 || msg->history.treble_count > 0) {
        
        /* Calculate History message size */
        size_t history_size = 0;
        
        /* bass field (field 1, packed repeated float) */
        if (msg->history.bass_count > 0) {
            history_size += 2; /* tag + length (packed format) */
            history_size += msg->history.bass_count * 4; /* 4 bytes per float */
        }
        
        /* lowmid field (field 2, packed repeated float) */
        if (msg->history.lowmid_count > 0) {
            history_size += 2; /* tag + length (packed format) */
            history_size += msg->history.lowmid_count * 4;
        }
        
        /* highmid field (field 3, packed repeated float) */
        if (msg->history.highmid_count > 0) {
            history_size += 2; /* tag + length (packed format) */
            history_size += msg->history.highmid_count * 4;
        }
        
        /* treble field (field 4, packed repeated float) */
        if (msg->history.treble_count > 0) {
            history_size += 2; /* tag + length (packed format) */
            history_size += msg->history.treble_count * 4;
        }
        
        /* Encode History tag and length */
        if (pos + 2 > buf_size) return -1;
        int n = encode_tag_and_type(&buf[pos], buf_size - pos, 1, WIRE_TYPE_LENGTH_DELIMITED);
        if (n < 0) return -1;
        pos += n;
        
        n = encode_varint(&buf[pos], buf_size - pos, (uint32_t)history_size);
        if (n < 0) return -1;
        pos += n;
        
        /* Encode bass array (packed format - all values in one length-delimited field) */
        if (msg->history.bass_count > 0) {
            n = encode_tag_and_type(&buf[pos], buf_size - pos, 1, WIRE_TYPE_LENGTH_DELIMITED);
            if (n < 0) return -1;
            pos += n;
            
            n = encode_varint(&buf[pos], buf_size - pos, (uint32_t)(msg->history.bass_count * 4));
            if (n < 0) return -1;
            pos += n;
            
            for (size_t i = 0; i < msg->history.bass_count; i++) {
                if (pos + 4 > buf_size) return -1;
                encode_float(&buf[pos], msg->history.bass[i]);
                pos += 4;
            }
        }
        
        /* Encode lowmid array (packed format - all values in one length-delimited field) */
        if (msg->history.lowmid_count > 0) {
            n = encode_tag_and_type(&buf[pos], buf_size - pos, 2, WIRE_TYPE_LENGTH_DELIMITED);
            if (n < 0) return -1;
            pos += n;
            
            n = encode_varint(&buf[pos], buf_size - pos, (uint32_t)(msg->history.lowmid_count * 4));
            if (n < 0) return -1;
            pos += n;
            
            for (size_t i = 0; i < msg->history.lowmid_count; i++) {
                if (pos + 4 > buf_size) return -1;
                encode_float(&buf[pos], msg->history.lowmid[i]);
                pos += 4;
            }
        }
        
        /* Encode highmid array (packed format - all values in one length-delimited field) */
        if (msg->history.highmid_count > 0) {
            n = encode_tag_and_type(&buf[pos], buf_size - pos, 3, WIRE_TYPE_LENGTH_DELIMITED);
            if (n < 0) return -1;
            pos += n;
            
            n = encode_varint(&buf[pos], buf_size - pos, (uint32_t)(msg->history.highmid_count * 4));
            if (n < 0) return -1;
            pos += n;
            
            for (size_t i = 0; i < msg->history.highmid_count; i++) {
                if (pos + 4 > buf_size) return -1;
                encode_float(&buf[pos], msg->history.highmid[i]);
                pos += 4;
            }
        }
        
        /* Encode treble array (packed format - all values in one length-delimited field) */
        if (msg->history.treble_count > 0) {
            n = encode_tag_and_type(&buf[pos], buf_size - pos, 4, WIRE_TYPE_LENGTH_DELIMITED);
            if (n < 0) return -1;
            pos += n;
            
            n = encode_varint(&buf[pos], buf_size - pos, (uint32_t)(msg->history.treble_count * 4));
            if (n < 0) return -1;
            pos += n;
            
            for (size_t i = 0; i < msg->history.treble_count; i++) {
                if (pos + 4 > buf_size) return -1;
                encode_float(&buf[pos], msg->history.treble[i]);
                pos += 4;
            }
        }
    }
    
    return (int)pos;
}

/* Audiolink_Data decoding */
int audiolink_data_decode(const uint8_t *buf, size_t buf_size, Audiolink_Data *msg) {
    memset(msg, 0, sizeof(Audiolink_Data));
    size_t pos = 0;
    
    while (pos < buf_size) {
        int field_num, wire_type;
        int n = decode_tag_and_type(&buf[pos], buf_size - pos, &field_num, &wire_type);
        if (n < 0) break;
        pos += n;
        
        if (field_num == 1 && wire_type == WIRE_TYPE_LENGTH_DELIMITED) {
            /* History message */
            uint32_t len;
            n = decode_varint(&buf[pos], buf_size - pos, &len);
            if (n < 0) break;
            pos += n;
            
            size_t history_end = pos + len;
            if (history_end > buf_size) return -1;
            
            while (pos < history_end) {
                int sub_field, sub_type;
                n = decode_tag_and_type(&buf[pos], buf_size - pos, &sub_field, &sub_type);
                if (n < 0) break;
                pos += n;
                
                if (sub_type == WIRE_TYPE_LENGTH_DELIMITED) {
                    uint32_t arr_len;
                    n = decode_varint(&buf[pos], buf_size - pos, &arr_len);
                    if (n < 0) break;
                    pos += n;
                    
                    size_t arr_end = pos + arr_len;
                    if (arr_end > buf_size) return -1;
                    
                    if (sub_field == 1) { /* bass (packed format) */
                        msg->history.bass_count = 0;
                        while (pos < arr_end && msg->history.bass_count < HISTORY_BASS_COUNT) {
                            if (pos + 4 > buf_size) return -1;
                            msg->history.bass[msg->history.bass_count++] = decode_float(&buf[pos]);
                            pos += 4;
                        }
                    } else if (sub_field == 2) { /* lowmid (packed format) */
                        msg->history.lowmid_count = 0;
                        while (pos < arr_end && msg->history.lowmid_count < HISTORY_LOWMID_COUNT) {
                            if (pos + 4 > buf_size) return -1;
                            msg->history.lowmid[msg->history.lowmid_count++] = decode_float(&buf[pos]);
                            pos += 4;
                        }
                    } else if (sub_field == 3) { /* highmid (packed format) */
                        msg->history.highmid_count = 0;
                        while (pos < arr_end && msg->history.highmid_count < HISTORY_HIGHMID_COUNT) {
                            if (pos + 4 > buf_size) return -1;
                            msg->history.highmid[msg->history.highmid_count++] = decode_float(&buf[pos]);
                            pos += 4;
                        }
                    } else if (sub_field == 4) { /* treble (packed format) */
                        msg->history.treble_count = 0;
                        while (pos < arr_end && msg->history.treble_count < HISTORY_TREBLE_COUNT) {
                            if (pos + 4 > buf_size) return -1;
                            msg->history.treble[msg->history.treble_count++] = decode_float(&buf[pos]);
                            pos += 4;
                        }
                    } else {
                        pos = arr_end;
                    }
                }
            }
        }
    }
    
    return (int)pos;
}

/* Sub_Packet encoding */
int sub_packet_encode(uint8_t *buf, size_t buf_size, const Sub_Packet *msg) {
    size_t pos = 0;
    
    /* Encode packet_index (field 1, sint32) */
    if (msg->packet_index != 0) {
        int n = encode_tag_and_type(&buf[pos], buf_size - pos, 1, WIRE_TYPE_VARINT);
        if (n < 0) return -1;
        pos += n;
        
        /* Zigzag encode the signed integer */
        uint32_t zz_value = (msg->packet_index << 1) ^ (msg->packet_index >> 31);
        n = encode_varint(&buf[pos], buf_size - pos, zz_value);
        if (n < 0) return -1;
        pos += n;
    }
    
    /* Encode data array (field 2, repeated byte) */
    if (msg->data_count > 0) {
        int n = encode_tag_and_type(&buf[pos], buf_size - pos, 2, WIRE_TYPE_LENGTH_DELIMITED);
        if (n < 0) return -1;
        pos += n;
        
        n = encode_varint(&buf[pos], buf_size - pos, (uint32_t)msg->data_count);
        if (n < 0) return -1;
        pos += n;
        
        if (pos + msg->data_count > buf_size) return -1;
        memcpy(&buf[pos], msg->data, msg->data_count);
        pos += msg->data_count;
    }
    
    return (int)pos;
}

/* Sub_Packet decoding */
int sub_packet_decode(const uint8_t *buf, size_t buf_size, Sub_Packet *msg) {
    memset(msg, 0, sizeof(Sub_Packet));
    size_t pos = 0;
    
    while (pos < buf_size) {
        int field_num, wire_type;
        int n = decode_tag_and_type(&buf[pos], buf_size - pos, &field_num, &wire_type);
        if (n < 0) break;
        pos += n;
        
        if (field_num == 1 && wire_type == WIRE_TYPE_VARINT) {
            /* packet_index */
            uint32_t value;
            n = decode_varint(&buf[pos], buf_size - pos, &value);
            if (n < 0) break;
            pos += n;
            
            /* Zigzag decode */
            msg->packet_index = (int32_t)((value >> 1) ^ (-(int32_t)(value & 1)));
        } else if (field_num == 2 && wire_type == WIRE_TYPE_LENGTH_DELIMITED) {
            /* data array */
            uint32_t len;
            n = decode_varint(&buf[pos], buf_size - pos, &len);
            if (n < 0) break;
            pos += n;
            
            if (pos + len > buf_size) return -1;
            if (len > SUB_PACKET_DATA_COUNT) return -1;
            
            msg->data_count = len;
            memcpy(msg->data, &buf[pos], len);
            pos += len;
        }
    }
    
    return (int)pos;
}
