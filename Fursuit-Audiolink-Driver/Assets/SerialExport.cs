using System;
using System.Collections;
using System.Collections.Generic;
using System.IO.Ports;
using System.Threading;
using UnityEngine;
using VRCAudioLink;

// Protobuf message definitions
public class History
{
    public List<float> bass = new List<float>();
    public List<float> lowmid = new List<float>();
    public List<float> highmid = new List<float>();
    public List<float> treble = new List<float>();
}

public class AudiolinkData
{
    public History history = new History();
}

public class SubPacket
{
    public int packet_index;
    public List<byte> data = new List<byte>();
}

public class SerialExport : MonoBehaviour
{
    private SerialPort serialPort;
    public AudioLink audioLink;
    public float sendInterval = 0.5f; // Send every 10 seconds
    private float timeSinceLastSend = 0f;
    
    // Reusable data structures to avoid allocations
    private AudiolinkData cachedAudioData;
    private byte[] serialBuffer = new byte[4096];
    private byte[] cobsBuffer = new byte[4096];
    
    // Background thread for serial writing
    private Thread serialThread;
    private Queue<byte[]> dataQueue = new Queue<byte[]>();
    private object queueLock = new object();
    private bool serialThreadRunning = false;
    private List<byte> receivedDataBuffer = new List<byte>(256);

    public bool generateNewData = true;

    void Start()
    {
        try
        {
            // Initialize reusable data structures
            cachedAudioData = new AudiolinkData();
            cachedAudioData.history.bass.Capacity = 128;
            cachedAudioData.history.lowmid.Capacity = 128;
            cachedAudioData.history.highmid.Capacity = 128;
            cachedAudioData.history.treble.Capacity = 128;
            
            // Open serial port COM6 at 115200 baud
            // serialPort = new SerialPort("COM6", 115200);
            const int baudRate = 921600;
            serialPort = new SerialPort("COM3", baudRate);
            serialPort.Open();
            //automatically derive the write timeout based on the baud rate and data size
            int bytesToWrite = 4096; // Example value, adjust as needed
            int calculatedTimeoutMs = (int)((bytesToWrite * 10.0 / baudRate) * 1000.0 * 2.0);
            serialPort.WriteTimeout = calculatedTimeoutMs;
            Debug.Log("Serial port COM6 opened successfully");
            
            // Start background serial thread
            serialThreadRunning = true;
            serialThread = new Thread(SerialWriteThread);
            serialThread.Name = "SerialWrite";
            serialThread.Start();
        }
        catch (System.Exception ex)
        {
            Debug.LogError("Failed to open serial port COM6: " + ex.Message);
        }
    }

    void Update()
    {
        if (serialPort == null || !serialPort.IsOpen)
            return;

        timeSinceLastSend += Time.deltaTime;

        if (timeSinceLastSend >= sendInterval && generateNewData)
        {
            SendAudioData();
            timeSinceLastSend = 0f;
        }
    }

    void SendAudioData()
    {
        try
        {
            // Validate audioLink once before the loop
            if (audioLink == null || audioLink.audioData == null)
                return;

            // Clear previous data (reuse lists)
            cachedAudioData.history.bass.Clear();
            cachedAudioData.history.lowmid.Clear();
            cachedAudioData.history.highmid.Clear();
            cachedAudioData.history.treble.Clear();
            
            // Collect audio bands for the current frame
            // Using 4 bands: bass, lowmid, highmid, treble
            for (int i = 0; i < 4; i++)
            {
                List<float> targetList = i switch
                {
                    0 => cachedAudioData.history.bass,
                    1 => cachedAudioData.history.lowmid,
                    2 => cachedAudioData.history.highmid,
                    3 => cachedAudioData.history.treble,
                    _ => cachedAudioData.history.bass
                };
                
                for (int j = 0; j < 128; j++)
                {
                    float bandValue = getBandHistory(i, j);
                    targetList.Add(bandValue);
                }
            }

            // Serialize to bytes (manual protobuf serialization)
            int serializedLength = SerializeAudioData(cachedAudioData, serialBuffer);

            // COBS encode
            int cobsLength = COBSEncode(serialBuffer, serializedLength, cobsBuffer);

            // Queue the data for serial writing on background thread
            // Add 0x00 frame delimiter at the end
            byte[] dataToSend = new byte[cobsLength + 1];
            System.Array.Copy(cobsBuffer, 0, dataToSend, 0, cobsLength);
            dataToSend[cobsLength] = 0x00;

            //print the first 10 bytes of the data to send for debugging
            // string debugOutput = "Data to send (first 10 bytes): ";
            // for (int i = 0; i < Mathf.Min(10, dataToSend.Length); i++)
            // {
            //     debugOutput += dataToSend[i].ToString("X2") + " ";
            // }
            // Debug.Log(debugOutput);

            lock (queueLock)
            {
                //only add to the que if there is not already 2 items in the queue, to avoid flooding the serial port
                if (dataQueue.Count < 2)
                {
                    dataQueue.Enqueue(dataToSend);
                }
            }
        }
        catch (System.Exception ex)
        {
            Debug.LogError("Error preparing audio data: " + ex.Message);
        }
    }

    float getBandHistory(int band, int index)
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return 0f;
        }

        // Validate audioData exists
        if (audioLink.audioData == null)
        {
            Debug.LogWarning("AudioLink.audioData is not initialized!");
            return 0f;
        }

        // Calculate the data index for the history
        int dataIndex = (band * 128) + index;

        // Validate index is within bounds
        if (dataIndex < 0 || dataIndex >= audioLink.audioData.Length)
        {
            Debug.LogWarning($"AudioLink data index {dataIndex} out of bounds (length: {audioLink.audioData.Length})");
            return 0f;
        }

        float bandValue = audioLink.audioData[dataIndex].grayscale;
        return bandValue;
    }

    //smoothed bands start at index 3584
    //3584 is the smoothest and 3599 is the most recent
    //each band is offset by 128
    float getBand(int band, int smoothing = 0)
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return 0f;
        }

        // Validate audioData exists
        if (audioLink.audioData == null)
        {
            Debug.LogWarning("AudioLink.audioData is not initialized!");
            return 0f;
        }

        //invert smoothing so it makes sense
        smoothing = 15 - smoothing;
        //cap smoothing
        if (smoothing < 0)
            smoothing = 0;
        if (smoothing > 15)
            smoothing = 15;
        int dataIndex = 3584 + (band * 128) + smoothing;
        
        // Validate index is within bounds
        if (dataIndex < 0 || dataIndex >= audioLink.audioData.Length)
        {
            Debug.LogWarning($"AudioLink data index {dataIndex} out of bounds (length: {audioLink.audioData.Length})");
            return 0f;
        }

        float bandValue = audioLink.audioData[dataIndex].grayscale;
        return bandValue;
    }

    // Calculate varint size
    int CalculateVarintSize(int value)
    {
        if (value < 0x80) return 1;
        if (value < 0x4000) return 2;
        if (value < 0x200000) return 3;
        if (value < 0x10000000) return 4;
        return 5;
    }

    // Calculate size of packed repeated float field
    int CalculatePackedFloatSize(List<float> values)
    {
        if (values.Count == 0) return 0;
        int floatDataSize = values.Count * 4;
        int tagSize = 1;
        int lengthSize = CalculateVarintSize(floatDataSize);
        return tagSize + lengthSize + floatDataSize;
    }

    // Calculate total size of History message
    int CalculateHistorySize(History history)
    {
        int size = 0;
        size += CalculatePackedFloatSize(history.bass);
        size += CalculatePackedFloatSize(history.lowmid);
        size += CalculatePackedFloatSize(history.highmid);
        size += CalculatePackedFloatSize(history.treble);
        return size;
    }

    // Manual protobuf serialization for Audiolink_Data
    int SerializeAudioData(AudiolinkData data, byte[] buffer)
    {
        int offset = 0;

        // Write History field directly (field 1, length-delimited)
        buffer[offset++] = (byte)((1 << 3) | 2);  // Tag 1, wire type 2
        
        // Calculate history size first
        int historySize = 0;
        historySize += CalculateRepeatedFloatSize(data.history.bass);
        historySize += CalculateRepeatedFloatSize(data.history.lowmid);
        historySize += CalculateRepeatedFloatSize(data.history.highmid);
        historySize += CalculateRepeatedFloatSize(data.history.treble);
        
        // Write history length
        offset += AppendVarintLength(buffer, offset, historySize);
        
        // Write history data directly
        offset += SerializeHistory(data.history, buffer, offset);

        return offset;
    }

    // Calculate size of packed repeated float field
    int CalculateRepeatedFloatSize(List<float> values)
    {
        if (values.Count == 0) return 0;
        
        int floatDataSize = values.Count * 4;
        int tagSize = 1;
        int lengthSize = CalculateVarintSize(floatDataSize);
        return tagSize + lengthSize + floatDataSize;
    }

    // Manual protobuf serialization for History
    int SerializeHistory(History history, byte[] buffer, int offset)
    {
        int startOffset = offset;

        // Field 1: bass (repeated float, packed)
        offset += SerializeRepeatedFloat(1, history.bass, buffer, offset);

        // Field 2: lowmid (repeated float, packed)
        offset += SerializeRepeatedFloat(2, history.lowmid, buffer, offset);

        // Field 3: highmid (repeated float, packed)
        offset += SerializeRepeatedFloat(3, history.highmid, buffer, offset);

        // Field 4: treble (repeated float, packed)
        offset += SerializeRepeatedFloat(4, history.treble, buffer, offset);

        return offset - startOffset;
    }

    // Serialize repeated float field (packed encoding)
    int SerializeRepeatedFloat(int fieldNumber, List<float> values, byte[] buffer, int offset)
    {
        int startOffset = offset;
        
        if (values.Count == 0)
            return 0;
        
        // Calculate packed data size (4 bytes per float)
        int packedDataSize = values.Count * 4;
        
        // Write tag with wire type 2 (length-delimited)
        buffer[offset++] = (byte)((fieldNumber << 3) | 2);
        
        // Write length as varint
        offset += AppendVarintLength(buffer, offset, packedDataSize);
        
        // Write all float values back-to-back
        foreach (float value in values)
        {
            System.BitConverter.GetBytes(value).CopyTo(buffer, offset);
            offset += 4;
        }

        return offset - startOffset;
    }

    // Append varint-encoded length to buffer, returns bytes written
    int AppendVarintLength(byte[] buffer, int offset, int length)
    {
        int startOffset = offset;
        
        while (length > 127)
        {
            buffer[offset++] = (byte)((length & 0x7F) | 0x80);
            length >>= 7;
        }
        buffer[offset++] = (byte)(length & 0x7F);
        
        return offset - startOffset;
    }

    // COBS (Consistent Overhead Byte Stuffing) encode - matches Python implementation
    int COBSEncode(byte[] input, int inputLength, byte[] output)
    {
        if (inputLength == 0)
        {
            output[0] = 0x01;
            return 1;
        }

        int writePos = 0;
        int codePos = 0;
        output[writePos++] = 0;  // Placeholder for first code byte
        
        for (int i = 0; i < inputLength; i++)
        {
            if (input[i] == 0x00)
            {
                // Found a zero, write code and reset
                output[codePos] = (byte)(i - codePos + 1);
                codePos = writePos;
                output[writePos++] = 0;  // New code placeholder
            }
            else
            {
                output[writePos++] = input[i];
            }
        }
        
        // Write final code
        output[codePos] = (byte)(writePos - codePos);
        
        return writePos;
    }

    // Background thread for serial port writing and reading
    void SerialWriteThread()
    {
        while (serialThreadRunning)
        {
            byte[] dataToWrite = null;
            
            lock (queueLock)
            {
                if (dataQueue.Count > 0)
                {
                    dataToWrite = dataQueue.Dequeue();
                }
            }
            
            if (dataToWrite != null && serialPort != null && serialPort.IsOpen)
            {
                try
                {
                    serialPort.Write(dataToWrite, 0, dataToWrite.Length);
                }
                catch (System.Exception ex)
                {
                    Debug.LogError("Serial write error: " + ex.Message);
                }
            }
            
            // Read incoming data
            // try
            // {
            //     while (serialPort != null && serialPort.IsOpen && serialPort.BytesToRead > 0)
            //     {
            //         int byteRead = serialPort.ReadByte();
            //         lock (queueLock)
            //         {
            //             receivedDataBuffer.Add((byte)byteRead);
            //         }
            //     }
                
            //     // Log batched received data if any
            //     if (receivedDataBuffer.Count > 0)
            //     {
            //         lock (queueLock)
            //         {
            //             string dataString = System.Text.Encoding.ASCII.GetString(receivedDataBuffer.ToArray());
            //             Debug.Log($"Received {receivedDataBuffer.Count} bytes: {dataString}");
            //             receivedDataBuffer.Clear();
            //         }
            //     }
            // }
            // catch (System.Exception ex)
            // {
            //     Debug.LogError("Serial read error: " + ex.Message);
            // }
            
            if (dataToWrite == null)
            {
                // Small sleep to avoid busy-waiting when queue is empty
                Thread.Sleep(0);
            }
        }
    }

    void OnDestroy()
    {
        // Stop the serial thread
        serialThreadRunning = false;
        if (serialThread != null && serialThread.IsAlive)
        {
            serialThread.Join(1000); // Wait up to 1 second
        }
        
        if (serialPort != null && serialPort.IsOpen)
        {
            serialPort.Close();
            Debug.Log("Serial port closed");
        }
    }
}
