using System;
using System.Collections;
using System.Collections.Generic;
using System.IO.Ports;
using System.Threading;
using UnityEngine;
using VRCAudioLink;
using PROTO;
using pbc = global::Google.Protobuf.Collections;

public class SerialExport : MonoBehaviour
{
    private SerialPort serialPort;
    public AudioLink audioLink;
    public float sendInterval = 0.5f; // Send every 10 seconds
    private float timeSinceLastSend = 0f;
    
    // Reusable data structures to avoid allocations
    private Audiolink_Data cachedAudioData;
    private byte[] serialBuffer = new byte[4096];
    
    // Background thread for serial writing
    private Thread serialThread;
    private Queue<byte[]> dataQueue = new Queue<byte[]>();
    private object queueLock = new object();
    private bool serialThreadRunning = false;
    private List<byte> receivedDataBuffer = new List<byte>(256);

    public bool generateNewData = true;

    void Start()
    {
        // Initialize reusable data structures
        cachedAudioData = new Audiolink_Data();
        
        // Open serial port COM6 at 115200 baud
        // serialPort = new SerialPort("COM6", 115200);
        const int baudRate = 921600;
        const string portName = "COM3"; // Change this to your desired COM port
        serialPort = new SerialPort(portName, baudRate);
        serialPort.Open();
        //automatically derive the write timeout based on the baud rate and data size
        int bytesToWrite = 4096; // Example value, adjust as needed
        int calculatedTimeoutMs = (int)((bytesToWrite * 10.0 / baudRate) * 1000.0 * 2.0);
        serialPort.WriteTimeout = calculatedTimeoutMs;
        Debug.Log("Serial port " + portName + " opened successfully");
        
        // Start background serial thread
        serialThreadRunning = true;
        serialThread = new Thread(SerialWriteThread);
        serialThread.Name = "SerialWrite";
        serialThread.Start();
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

            //make sure the lists exist
            if (cachedAudioData.History == null)
            {
                cachedAudioData.History = new History();
            }

            // Clear previous data (reuse lists)
            cachedAudioData.History.Bass.Clear();
            cachedAudioData.History.Lowmid.Clear();
            cachedAudioData.History.Highmid.Clear();
            cachedAudioData.History.Treble.Clear();
            
            // Collect audio bands for the current frame
            // Using 4 bands: bass, lowmid, highmid, treble
            for (int i = 0; i < 4; i++)
            {
                var targetList = i switch
                {
                    0 => cachedAudioData.History.Bass,
                    1 => cachedAudioData.History.Lowmid,
                    2 => cachedAudioData.History.Highmid,
                    3 => cachedAudioData.History.Treble,
                    _ => cachedAudioData.History.Bass
                };
                
                for (int j = 0; j < 128; j++)
                {
                    float bandValue = getBandHistory(i, j);
                    targetList.Add(bandValue);
                }
            }

            // Serialize to bytes (manual protobuf serialization)
            int serializedLength = SerializeAudioData(cachedAudioData, serialBuffer);

            // COBS encode using cobs.net library
            byte[] inputData = new byte[serializedLength];
            System.Array.Copy(serialBuffer, 0, inputData, 0, serializedLength);
            byte[] cobsEncoded = COBS.NET.COBS.Encode(inputData);

            // Queue the data for serial writing on background thread
            // Add 0x00 frame delimiter at the end
            byte[] dataToSend = new byte[cobsEncoded.Length + 1];
            System.Array.Copy(cobsEncoded, 0, dataToSend, 0, cobsEncoded.Length);
            dataToSend[cobsEncoded.Length] = 0x00;

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
            Debug.LogError(ex);
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
    int CalculatePackedFloatSize(pbc::RepeatedField<float> values)
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
        size += CalculatePackedFloatSize(history.Bass);
        size += CalculatePackedFloatSize(history.Lowmid);
        size += CalculatePackedFloatSize(history.Highmid);
        size += CalculatePackedFloatSize(history.Treble);
        return size;
    }

    // Manual protobuf serialization for Audiolink_Data
    int SerializeAudioData(Audiolink_Data data, byte[] buffer)
    {
        int offset = 0;

        // Write History field directly (field 1, length-delimited)
        buffer[offset++] = (byte)((1 << 3) | 2);  // Tag 1, wire type 2
        
        // Calculate history size first
        int historySize = 0;
        historySize += CalculateRepeatedFloatSize(data.History.Bass);
        historySize += CalculateRepeatedFloatSize(data.History.Lowmid);
        historySize += CalculateRepeatedFloatSize(data.History.Highmid);
        historySize += CalculateRepeatedFloatSize(data.History.Treble);
        
        // Write history length
        offset += AppendVarintLength(buffer, offset, historySize);
        
        // Write history data directly
        offset += SerializeHistory(data.History, buffer, offset);

        return offset;
    }

    // Calculate size of packed repeated float field
    int CalculateRepeatedFloatSize(pbc::RepeatedField<float> values)
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
        offset += SerializeRepeatedFloat(1, history.Bass, buffer, offset);

        // Field 2: lowmid (repeated float, packed)
        offset += SerializeRepeatedFloat(2, history.Lowmid, buffer, offset);

        // Field 3: highmid (repeated float, packed)
        offset += SerializeRepeatedFloat(3, history.Highmid, buffer, offset);

        // Field 4: treble (repeated float, packed)
        offset += SerializeRepeatedFloat(4, history.Treble, buffer, offset);

        return offset - startOffset;
    }

    // Serialize repeated float field (packed encoding)
    int SerializeRepeatedFloat(int fieldNumber, pbc::RepeatedField<float> values, byte[] buffer, int offset)
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
