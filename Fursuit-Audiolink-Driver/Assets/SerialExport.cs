using System;
using System.Collections;
using System.Collections.Generic;
using System.IO.Ports;
using System.Threading;
using UnityEngine;
using VRCAudioLink;
using PROTO;
using Google.Protobuf;

public class SerialExport : MonoBehaviour
{
    private SerialPort serialPort;
    public AudioLink audioLink;
    public float sendInterval = 0.5f; // Send every 10 seconds
    private float timeSinceLastSend = 0f;
    
    // Reusable data structures to avoid allocations
    private Audiolink_Data audiolink_Data;
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
        audiolink_Data = new Audiolink_Data();
        
        // Open serial port COM6 at 115200 baud
        // serialPort = new SerialPort("COM6", 115200);
        const int baudRate = 921600;
        const string portName = "COM3"; // Change this to your desired COM port
        serialPort = new SerialPort(portName, baudRate);
        serialPort.Open();
        //automatically derive the write timeout based on the baud rate and data size
        int bytesToWrite = 4096 * 2; // Example value, adjust as needed
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
            if (audiolink_Data.History == null)
            {
                audiolink_Data.History = new History();
            }

            // Clear previous data (reuse lists)
            audiolink_Data.History.Bass.Clear();
            audiolink_Data.History.Lowmid.Clear();
            audiolink_Data.History.Highmid.Clear();
            audiolink_Data.History.Treble.Clear();
            
            // Collect audio bands for the current frame
            // Using 4 bands: bass, lowmid, highmid, treble
            for (int i = 0; i < 4; i++)
            {
                var targetList = i switch
                {
                    0 => audiolink_Data.History.Bass,
                    1 => audiolink_Data.History.Lowmid,
                    2 => audiolink_Data.History.Highmid,
                    3 => audiolink_Data.History.Treble,
                    _ => audiolink_Data.History.Bass
                };
                
                for (int j = 0; j < 128; j++)
                {
                    uint bandValue = floatToUInt32(getBandHistory(i, j));
                    targetList.Add(bandValue);
                }
            }

            var themeColors = new ThemeColors();
            themeColors.ThemeColor0 = getThemeColor(0);
            themeColors.ThemeColor1 = getThemeColor(1);
            themeColors.ThemeColor2 = getThemeColor(2);
            themeColors.ThemeColor3 = getThemeColor(3);
            audiolink_Data.ThemeColors = themeColors;

            audiolink_Data.Dft = getDFT();
            audiolink_Data.FilteredAudiolink = getFilteredAudiolink();

            byte[] cobsEncoded = COBS.NET.COBS.Encode(audiolink_Data.ToByteArray());

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

    PROTO.FilteredAudiolink getFilteredAudiolink()
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return new PROTO.FilteredAudiolink();
        }

        // Validate audioData exists
        if (audioLink.audioData == null)
        {
            Debug.LogWarning("AudioLink.audioData is not initialized!");
            return new PROTO.FilteredAudiolink();
        }

        PROTO.FilteredAudiolink filteredAudiolink = new PROTO.FilteredAudiolink();

        Vector2Int startPos = new Vector2Int(0, 28); // Assuming the filtered data starts at row 32

        // Assuming the filtered data starts at index 4096 and has 128 values for each band
        for (int band = 0; band < 4; band++)
        {
            for (int i = 0; i < 16; i++)
            {
                int dataIndex = getIndexFromXY(startPos.x + i, startPos.y + band);

                // Validate index is within bounds
                if (dataIndex < 0 || dataIndex >= audioLink.audioData.Length)
                {
                    Debug.LogWarning($"AudioLink data index {dataIndex} out of bounds (length: {audioLink.audioData.Length})");
                    continue;
                }

                uint bandValue = floatToUInt32(audioLink.audioData[dataIndex].grayscale);

                switch (band)
                {
                    case 0:
                        filteredAudiolink.Bass.Add(bandValue);
                        break;
                    case 1:
                        filteredAudiolink.Lowmid.Add(bandValue);
                        break;
                    case 2:
                        filteredAudiolink.Highmid.Add(bandValue);
                        break;
                    case 3:
                        filteredAudiolink.Treble.Add(bandValue);
                        break;
                }
            }
        }

        return filteredAudiolink;
    }

    PROTO.DFT getDFT()
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return new PROTO.DFT();
        }

        // Validate audioData exists
        if (audioLink.audioData == null)
        {
            Debug.LogWarning("AudioLink.audioData is not initialized!");
            return new PROTO.DFT();
        }

        //dft starts at 0,4
        Vector2Int startPos = new Vector2Int(0, 4);
        const int totalDFT = 128 * 2;
        //split into 4 float arrays
        float[] mag = new float[totalDFT];
        float[] magEQ = new float[totalDFT];
        float[] magFilt = new float[totalDFT];
        float[] magPhase = new float[totalDFT];
        for (int i = 0; i < totalDFT; i++)
        {
            var col = audioLink.audioData[interpretMultiline(startPos, i)];
            mag[i] = col.r;
            magEQ[i] = col.g;
            magFilt[i] = col.b;
            magPhase[i] = col.a;
        }
        PROTO.DFT dft = new PROTO.DFT
        {
            Mag = { mag },
            MagEQ = { magEQ },
            Magfilt = { magFilt },
            MagPhase = { magPhase }
        };

        return dft;
    }

    private int interpretMultiline(Vector2Int startPos, int index)
    {
        //For data groups that are multiline, all data is represented as left-to-right (increasing X) then incrementing Y and scanning X from left to right on the next line.
        int width = 128; // Assuming the texture width is 128
        int x = startPos.x + (index % width);
        int y = startPos.y + (index / width);
        return getIndexFromXY(x, y);
    }

    PROTO.Color getThemeColor(int index)
    {
        // return new PROTO.Color
        // {
        //     R = 0,
        //     G = 1,
        //     B = 0
        // };
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return new PROTO.Color { R = 0, G = 0, B = 0 };
        }

        int dataIndex = getIndexFromXY(index, 23); // Assuming the theme colors are in the first row (y=0)
        return convertUnityColorToProtoColor(audioLink.audioData[dataIndex]);
    }

    //helper function to convert from XY coords in the texture to the index in the audioData array
    int getIndexFromXY(int x, int y)
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return -1;
        }

        // Validate audioData exists
        if (audioLink.audioData == null)
        {
            Debug.LogWarning("AudioLink.audioData is not initialized!");
            return -1;
        }

        int width = 128; // Assuming the texture width is 128
        int height = 64; // Assuming the texture height is 64

        // Validate x and y are within bounds
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            Debug.LogWarning($"XY coordinates ({x}, {y}) out of bounds (width: {width}, height: {height})");
            return -1;
        }

        int index = (y * width) + x;

        // Validate index is within bounds
        if (index < 0 || index >= audioLink.audioData.Length)
        {
            Debug.LogWarning($"Calculated index {index} out of bounds (length: {audioLink.audioData.Length})");
            return -1;
        }

        return index;
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
            //     if (serialPort != null && serialPort.IsOpen && serialPort.BytesToRead > 0)
            //     {
            //         byte[] buffer = new byte[Mathf.Min(serialPort.BytesToRead, 4096)];
            //         int bytesRead = serialPort.Read(buffer, 0, buffer.Length);
                    
            //         if (bytesRead > 0)
            //         {
            //             lock (queueLock)
            //             {
            //                 for (int i = 0; i < bytesRead; i++)
            //                 {
            //                     receivedDataBuffer.Add(buffer[i]);
            //                 }
                            
            //                 // Log batched received data
            //                 string dataString = System.Text.Encoding.ASCII.GetString(receivedDataBuffer.ToArray());
            //                 Debug.Log($"Received {receivedDataBuffer.Count} bytes: {dataString}");
            //                 receivedDataBuffer.Clear();
            //             }
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

    private PROTO.Color convertUnityColorToProtoColor(UnityEngine.Color unityColor)
    {
        return new PROTO.Color
        {
            R = floatToUInt32(unityColor.r),
            G = floatToUInt32(unityColor.g),
            B = floatToUInt32(unityColor.b)
        };
    }

    private uint[] floatArrayToUInt32Array(float[] floatArray)
    {
        uint[] uintArray = new uint[floatArray.Length];
        for (int i = 0; i < floatArray.Length; i++)
        {
            uintArray[i] = floatToUInt32(floatArray[i]);
        }
        return uintArray;
    }

    private uint floatToUInt32(float value)
    {
        // Clamp the value to [0, 1] range
        value = Mathf.Clamp01(value);
        return (uint)(value * 255.0f);
    }
}
