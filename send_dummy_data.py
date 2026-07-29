#!/usr/bin/env python3
"""
Generate dummy Audiolink_Data protobuf messages and pipe them to UART.
Encodes with COBS framing and sends over serial.
"""

import struct
import serial
import time
import argparse
import math
import sys
from collections import deque


class ProtobufEncoder:
    """Simple protobuf encoder for Audiolink_Data and Sub_Packet messages."""
    
    WIRE_TYPE_VARINT = 0
    WIRE_TYPE_64BIT = 1
    WIRE_TYPE_LENGTH_DELIMITED = 2
    WIRE_TYPE_32BIT = 5
    
    @staticmethod
    def encode_varint(value):
        """Encode a variable-length integer."""
        result = bytearray()
        while value >= 0x80:
            result.append((value & 0x7F) | 0x80)
            value >>= 7
        result.append(value & 0x7F)
        return bytes(result)
    
    @staticmethod
    def encode_tag_and_type(field_num, wire_type):
        """Encode protobuf tag (field number + wire type)."""
        tag = (field_num << 3) | wire_type
        return ProtobufEncoder.encode_varint(tag)
    
    @staticmethod
    def encode_float(value):
        """Encode IEEE 754 float (32-bit)."""
        return struct.pack('<f', value)
    
    @staticmethod
    def encode_repeated_floats(field_num, values):
        """Encode a repeated float field."""
        if not values:
            return b''
        
        # Encode all floats into a byte buffer
        float_data = b''.join(ProtobufEncoder.encode_float(v) for v in values)
        
        # Encode tag and wire type (length-delimited)
        result = ProtobufEncoder.encode_tag_and_type(field_num, ProtobufEncoder.WIRE_TYPE_LENGTH_DELIMITED)
        
        # Encode length
        result += ProtobufEncoder.encode_varint(len(float_data))
        
        # Append float data
        result += float_data
        
        return result
    
    @staticmethod
    def encode_audiolink_data(bass, lowmid, highmid, treble):
        """Encode an Audiolink_Data message containing a History."""
        history_data = b''
        
        # Encode each frequency band (fields 1-4 in History message)
        history_data += ProtobufEncoder.encode_repeated_floats(1, bass)
        history_data += ProtobufEncoder.encode_repeated_floats(2, lowmid)
        history_data += ProtobufEncoder.encode_repeated_floats(3, highmid)
        history_data += ProtobufEncoder.encode_repeated_floats(4, treble)
        
        if not history_data:
            return b''
        
        # Wrap History in Audiolink_Data (field 1)
        result = ProtobufEncoder.encode_tag_and_type(1, ProtobufEncoder.WIRE_TYPE_LENGTH_DELIMITED)
        result += ProtobufEncoder.encode_varint(len(history_data))
        result += history_data
        
        return result


class COBSEncoder:
    """Consistent Overhead Byte Stuffing (COBS) encoder."""
    
    @staticmethod
    def encode(data):
        """Encode data using COBS. Returns frame without trailing 0x00."""
        if not data:
            return b'\x01'
        
        result = bytearray()
        code_pos = 0
        result.append(0)  # Placeholder for code byte
        
        for i, byte in enumerate(data):
            if byte == 0x00:
                # Found a zero, write code and reset
                result[code_pos] = i - code_pos + 1
                code_pos = len(result)
                result.append(0)  # New code placeholder
            else:
                result.append(byte)
        
        # Write final code
        result[code_pos] = len(result) - code_pos
        
        return bytes(result)


def send_to_uart(port, baudrate=115200, num_packets=10, interval=0.0):
    """Send dummy Audiolink_Data packets to UART with animation.
    
    Mimics the ESP32 test mode: gradually builds up from 1 to 128 samples per band,
    with a growing sin wave animation. Runs as fast as possible (no delays by default).
    """
    try:
        ser = serial.Serial(port, baudrate, timeout=1.0)
        time.sleep(0.5)  # Wait for device to be ready
    except serial.SerialException as e:
        print(f"Error opening serial port {port}: {e}")
        return False
    
    try:
        # Use deques for efficient rotation (O(1) instead of O(n) for shift operations)
        bass_buffer = deque([0.0] * 128, maxlen=128)
        lowmid_buffer = deque([0.0] * 128, maxlen=128)
        highmid_buffer = deque([0.0] * 128, maxlen=128)
        treble_buffer = deque([0.0] * 128, maxlen=128)
        buffer_count = 0
        
        packet_num = 0
        last_sleep_time = time.time()
        
        while True:
            # Generate next sin value with 1-second period
            phase = (2.0 * math.pi * buffer_count) / 128.0
            sin_value = (math.sin(phase) + 1.0) / 2.0
            
            # Rotate buffers (deque handles this efficiently with O(1))
            bass_buffer.appendleft(sin_value)
            lowmid_buffer.appendleft(sin_value)
            highmid_buffer.appendleft(sin_value)
            treble_buffer.appendleft(sin_value)
            
            # Increment buffer count (max 128)
            if buffer_count < 128:
                buffer_count += 1
            
            # Create audio data with current buffer_count samples
            bass = list(bass_buffer)[:buffer_count]
            lowmid = list(lowmid_buffer)[:buffer_count]
            highmid = list(highmid_buffer)[:buffer_count]
            treble = list(treble_buffer)[:buffer_count]

            #force bass to 0
            bass = [1.0] * buffer_count
            lowmid = [1.0] * buffer_count
            highmid = [1.0] * buffer_count
            treble = [1.0] * buffer_count
            
            # Encode as protobuf
            encoded_data = ProtobufEncoder.encode_audiolink_data(bass, lowmid, highmid, treble)
            
            # Encode with COBS
            cobs_data = COBSEncoder.encode(encoded_data)

            #print the first 80 bytes of the cobs data
            print(f"Packet {packet_num + 1}: COBS data length: {len(cobs_data)} bytes, first 80 bytes: {cobs_data[:].hex()}")
            
            # Send frame: COBS data + 0x00 delimiter
            frame = cobs_data + b'\x00'
            ser.write(frame)
            ser.flush()
            
            # Stop after num_packets (or use Ctrl+C for infinite animation)
            packet_num += 1
            if packet_num >= num_packets:
                break
            
            # Optional throttling (only if interval > 0)
            if interval > 0:
                now = time.time()
                elapsed = now - last_sleep_time
                if elapsed < interval:
                    time.sleep(interval - elapsed)
                last_sleep_time = time.time()
        
        return True
        
    except KeyboardInterrupt:
        return True
    except Exception as e:
        print(f"Error sending data: {e}")
        return False
    finally:
        ser.close()


def main():
    parser = argparse.ArgumentParser(
        description='Generate animated Audiolink_Data and send over UART (matches ESP32 test mode)'
    )
    parser.add_argument('port', nargs='?', default="COM6", help='Serial port (e.g., COM6 or /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--count', type=int, default=0, help='Number of packets to send before stopping (default: 0 for infinite)')
    parser.add_argument('--interval', type=float, default=0.0, help='Interval between packets in seconds (default: 0.0 for maximum speed)')
    
    args = parser.parse_args()
    
    # If count is 0, use a large number to effectively run forever (Ctrl+C to stop)
    count = args.count if args.count > 0 else 999999
    
    success = send_to_uart(args.port, args.baud, count, args.interval)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
