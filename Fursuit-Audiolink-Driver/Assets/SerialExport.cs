using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.IO.Ports;
using System.Text;
using System.Threading;
using UnityEngine;
using TMPro;
using VRCAudioLink;
using PROTO;
using Google.Protobuf;

public class SerialExport : MonoBehaviour
{
    private SerialPort serialPort;
    public AudioLink audioLink;
    [SerializeField] private string portName = "COM3";
    [SerializeField] private int baudRate = 921600;
    public float sendInterval = 0.5f; // Send every 10 seconds
    public bool useZlibCompression = true;
    public bool logPacketSizeDifference = true;
    [SerializeField] private TMP_Text packetSizeText;
    public bool logReceivedSerialData = false;
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
    private bool serialConfigDirty = false;
    public bool generateNewData = true;

    //flags enum for different features
    [Flags]
    public enum FeaturesEnum
    {
        Waveform = 1 << 0,
        ThemeColors = 1 << 1,
        History = 1 << 2,
        DFT = 1 << 3,
        FilteredAudiolink = 1 << 4,
        Colorchord = 1 << 5,
        GeneralVU = 1 << 6,
        GlobalStrings = 1 << 7,
        AutoCorrelator = 1 << 8,
        Chronotensity = 1 << 9
    }

    public FeaturesEnum Features;

    void OnValidate()
    {
        serialConfigDirty = true;
    }

    void Start()
    {
        // Initialize reusable data structures
        audiolink_Data = new Audiolink_Data();

        InitializeSerialPort();
        serialConfigDirty = false;
    }

    void Update()
    {
        if (serialConfigDirty)
        {
            serialConfigDirty = false;
            ReinitializeSerialPort();
        }

        timeSinceLastSend += Time.deltaTime;

        if (timeSinceLastSend >= sendInterval && generateNewData)
        {
            SendAudioData();
            timeSinceLastSend = 0f;
        }
    }

    private void InitializeSerialPort()
    {
        string trimmedPortName = portName == null ? string.Empty : portName.Trim();
        if (string.IsNullOrEmpty(trimmedPortName))
        {
            Debug.LogWarning("Serial port name is empty. Serial export is disabled until a valid port name is set.");
            return;
        }

        string[] availablePorts = SerialPort.GetPortNames();
        bool portExists = false;
        for (int i = 0; i < availablePorts.Length; i++)
        {
            if (string.Equals(availablePorts[i], trimmedPortName, StringComparison.OrdinalIgnoreCase))
            {
                portExists = true;
                break;
            }
        }

        if (!portExists)
        {
            Debug.LogWarning($"Serial port {trimmedPortName} was not found. Serial export is disabled. Available ports: {string.Join(", ", availablePorts)}");
        }

        if (portExists)
        {
            try
            {
                serialPort = new SerialPort(trimmedPortName, baudRate);
                serialPort.Open();
                //automatically derive the write timeout based on the baud rate and data size
                int bytesToWrite = 4096 * 4; // Example value, adjust as needed
                int calculatedTimeoutMs = (int)((bytesToWrite * 10.0 / baudRate) * 1000.0 * 2.0);
                serialPort.WriteTimeout = calculatedTimeoutMs;
                Debug.Log("Serial port " + trimmedPortName + " opened successfully");
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"Failed to open serial port {trimmedPortName}. Serial export is disabled. Reason: {ex.Message}");
                serialPort = null;
            }
        }

        if (serialPort != null && serialPort.IsOpen)
        {
            // Start background serial thread only when serial output is available
            serialThreadRunning = true;
            serialThread = new Thread(SerialWriteThread);
            serialThread.Name = "SerialWrite";
            serialThread.Start();
        }
    }

    private void ReinitializeSerialPort()
    {
        ShutdownSerialPort(false);
        InitializeSerialPort();
    }

    private void ShutdownSerialPort(bool logClose)
    {
        serialThreadRunning = false;
        if (serialThread != null && serialThread.IsAlive)
        {
            serialThread.Join(1000);
        }

        serialThread = null;

        bool wasOpen = serialPort != null && serialPort.IsOpen;
        if (serialPort != null)
        {
            try
            {
                if (serialPort.IsOpen)
                {
                    serialPort.Close();
                }
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"Failed closing serial port: {ex.Message}");
            }

            serialPort = null;
        }

        if (logClose && wasOpen)
        {
            Debug.Log("Serial port closed");
        }
    }

    void SendAudioData()
    {
        try
        {
            // Validate audioLink once before the loop
            if (audioLink == null || !audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
                return;

            //hard clear the audiolink_Data object to avoid sending stale data
            audiolink_Data = new Audiolink_Data();

            if (Features.HasFlag(FeaturesEnum.Waveform))
            {
                audiolink_Data.Waveform = getWaveform();
            }

            if (Features.HasFlag(FeaturesEnum.History))
            {
                audiolink_Data.History = getHistory();
            }

            if (Features.HasFlag(FeaturesEnum.ThemeColors))
            {            
                audiolink_Data.ThemeColors = getThemeColors();
            }

            if (Features.HasFlag(FeaturesEnum.DFT))
            {
                audiolink_Data.Dft = getDFT();
            }

            if (Features.HasFlag(FeaturesEnum.FilteredAudiolink))
            {
                audiolink_Data.FilteredAudiolink = getFilteredAudiolink();
            }

            if (Features.HasFlag(FeaturesEnum.Colorchord))
            {
                audiolink_Data.Colorchord = getColorChord();
            }

            if (Features.HasFlag(FeaturesEnum.GeneralVU))
            {
                audiolink_Data.GeneralVu = getGeneralVU();
            }

            if (Features.HasFlag(FeaturesEnum.GlobalStrings))
            {
                audiolink_Data.GlobalStrings = getGlobalStrings();
            }

            if (Features.HasFlag(FeaturesEnum.AutoCorrelator))
            {
                audiolink_Data.Autocorrelator = getAutoCorrelator();
            }

            if (Features.HasFlag(FeaturesEnum.Chronotensity))
            {
                audiolink_Data.Chronotensity = getChronotensity();
            }
            
            

            byte[] protobufPacket = audiolink_Data.ToByteArray();
            byte[] payloadToFrame = useZlibCompression ? CompressZlibPayload(protobufPacket) : protobufPacket;
            byte[] cobsEncoded = COBS.NET.COBS.Encode(payloadToFrame);

            // Queue the data for serial writing on background thread
            // Add 0x00 frame delimiter at the end
            byte[] dataToSend = new byte[cobsEncoded.Length + 1];
            System.Array.Copy(cobsEncoded, 0, dataToSend, 0, cobsEncoded.Length);
            dataToSend[cobsEncoded.Length] = 0x00;

            if (logPacketSizeDifference)
            {
                int originalSize = protobufPacket.Length;
                int compressedSize = payloadToFrame.Length;
                int framedSize = dataToSend.Length;
                float savingsPercent = originalSize > 0
                    ? (1f - (compressedSize / (float)originalSize)) * 100f
                    : 0f;
                
                const int packetSize = 1470;
                int idealPackets = Mathf.CeilToInt(framedSize / (float)packetSize);

                string packetSizeMessage = $"Packet size bytes - protobuf: {originalSize}, zlib: {compressedSize}, savings: {savingsPercent:F1}%, framed: {framedSize}, ideal packets: {idealPackets}";

                if (packetSizeText != null)
                {
                    packetSizeText.text = packetSizeMessage;
                }
                else
                {
                    Debug.Log(packetSizeMessage);
                }
            }

            //print the first 10 bytes of the data to send for debugging
            // string debugOutput = "Data to send (first 10 bytes): ";
            // for (int i = 0; i < Mathf.Min(10, dataToSend.Length); i++)
            // {
            //     debugOutput += dataToSend[i].ToString("X2") + " ";
            // }
            // Debug.Log(debugOutput);

            lock (queueLock)
            {
                // Only queue if serial output is active; generation still runs even without a serial device.
                if (serialPort != null && serialPort.IsOpen && dataQueue.Count < 2)
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

    private uint ReadGlobalStringCodePoint(int stringNum, int charIndex)
    {
        if (audioLink == null || !audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
            return 0;

        Vector2Int globalStringsPos = new Vector2Int(40, 28);
        int pixelIndex = charIndex / 4;
        int dataIndex = getIndexFromXY(globalStringsPos.x + pixelIndex, globalStringsPos.y + stringNum);
        if (dataIndex < 0)
            return 0;

        Vector4 col = audioLink.rawAudioData[dataIndex];
        float channelValue = charIndex % 4 switch
        {
            0 => col.x,
            1 => col.y,
            2 => col.z,
            _ => col.w
        };

        uint bits = BitConverter.ToUInt32(BitConverter.GetBytes(channelValue), 0);
        return bits & 0x007FFFFF;
    }

    private static uint FloatToIntBits24Bit(float value)
    {
        return (uint)Math.Round(
            (value / 1.1754944e-38F) * 8388608F
        ) & 0x007FFFFF;
    }

    private string getGlobalString(int num)
    {
        Vector2Int globalStringsPos = new Vector2Int(40, 28);
        Vector4[] vecs = new Vector4[8];
        for (int i = 0; i < vecs.Length; i++)
        {
            int pixelIndex = i;
            int dataIndex = getIndexFromXY(globalStringsPos.x + pixelIndex, globalStringsPos.y + num);
            
            //convert to Vector4
            vecs[i] = audioLink.rawAudioData[dataIndex];
        }
        System.Text.StringBuilder sb = new System.Text.StringBuilder();

        for (int i = 0; i < vecs.Length; i++)
        {
            uint[] codePoints =
            {
                FloatToIntBits24Bit(vecs[i].x),
                FloatToIntBits24Bit(vecs[i].y),
                FloatToIntBits24Bit(vecs[i].z),
                FloatToIntBits24Bit(vecs[i].w)
            };

            for (int j = 0; j < codePoints.Length; j++)
            {
                // 0 represents unused/padding entries
                // if (codePoints[j] == 0)
                //     return sb.ToString();

                sb.Append(char.ConvertFromUtf32((int)codePoints[j]));
            }
        }

        return sb.ToString();
    }

    PROTO.GlobalStrings getGlobalStrings()
    {
        PROTO.GlobalStrings globalStrings = new GlobalStrings()
        {
            PlayerName = getGlobalString(0),
            MasterName = getGlobalString(1),
            CustomString1 = getGlobalString(2),
            CustomString2 = getGlobalString(3)
        };
        // string test = "";
        // test += getGlobalString(0) + "\n";
        // test += getGlobalString(1) + "\n";
        // test += getGlobalString(2) + "\n";
        // test += getGlobalString(3) + "\n";
        // Debug.Log(test);
        //interpret global string 2 in hex
        // Debug.Log("Global String 3: " + getGlobalString(3));
        // Debug.Log("Global String 3 Hex: " + BitConverter.ToString(Encoding.UTF8.GetBytes(getGlobalString(3))).Replace("-", " "));
        return globalStrings;
    }

    double AudioLinkDecodeDataAsDouble(Vector4 raw)
    {
        return raw.x + raw.y*1024 + raw.z * 1048576 + raw.w * 1073741824;
    }

    uint AudioLinkDecodeDataAsUInt(Vector4 raw)
    {
        return (uint)(raw.x + raw.y*1024 + raw.z * 1048576 + raw.w * 1073741824);
    }

    PROTO.PlayerData getPlayerData()
    {
        Vector2Int playerDataPos = new Vector2Int(6, 22);
        uint numberOfPlayers = (uint)audioLink.rawAudioData[getIndexFromXY(playerDataPos.x, playerDataPos.y)].x;
        bool isMaster = audioLink.rawAudioData[getIndexFromXY(playerDataPos.x, playerDataPos.y)].y > 0.5;
        bool isOwner = audioLink.rawAudioData[getIndexFromXY(playerDataPos.x, playerDataPos.y)].z > 0.5;

        PROTO.PlayerData playerData = new PROTO.PlayerData
        {
            NumberOfPlayers = numberOfPlayers,
            IsMaster = isMaster,
            IsOwner = isOwner
        };
        return playerData;
    }

    PROTO.Intensity getIntensity(Vector2Int pos)
    {
        float rmsleft = audioLink.rawAudioData[getIndexFromXY(pos.x, pos.y)].x;
        float peakLeft = audioLink.rawAudioData[getIndexFromXY(pos.x, pos.y)].y;
        float rmsRight = audioLink.rawAudioData[getIndexFromXY(pos.x, pos.y)].z;
        float peakRight = audioLink.rawAudioData[getIndexFromXY(pos.x, pos.y)].w;

        PROTO.Intensity protoIntensity = new PROTO.Intensity
        {
            RMSLeft = rmsleft,
            PeakLeft = peakLeft,
            RMSRight = rmsRight,
            PeakRight = peakRight
        };
        return protoIntensity;
    }

    PROTO.Autogain getAutogain()
    {
        Vector2Int autogainPos = new Vector2Int(11, 22);
        float assymetricfiltered = audioLink.rawAudioData[getIndexFromXY(autogainPos.x, autogainPos.y)].x;
        float symmetricfiltered = audioLink.rawAudioData[getIndexFromXY(autogainPos.x, autogainPos.y)].y;

        PROTO.Autogain protoAutogain = new PROTO.Autogain
        {
            AsymmetricGain = assymetricfiltered,
            SymmetricGain = symmetricfiltered
        };
        return protoAutogain;
    }

    PROTO.ChronotensityBand getChronotensityBand(int bandIndex)
    {
        Vector2Int chronotensityPos = new Vector2Int(16, 28);
        Vector2Int chronotensitySize = new Vector2Int(8, 1);

        Vector2Int bandPos = new Vector2Int(chronotensityPos.x, chronotensityPos.y + bandIndex);

        uint increasing = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x, bandPos.y)]);
        uint filtered_increasing = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 1, bandPos.y)]);
        uint bounce = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 2, bandPos.y)]);
        uint filtered_bounce = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 3, bandPos.y)]);
        uint intensity_pause = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 4, bandPos.y)]);
        uint filtered_intensity_pause = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 5, bandPos.y)]);
        uint bounce_pause = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 6, bandPos.y)]);
        uint filtered_bounce_pause = AudioLinkDecodeDataAsUInt(audioLink.rawAudioData[getIndexFromXY(bandPos.x + 7, bandPos.y)]);

        PROTO.ChronotensityBand protoChronotensityBand = new PROTO.ChronotensityBand
        {
            Increasing = increasing,
            FilteredIncreasing = filtered_increasing,
            Bounce = bounce,
            FilteredBounce = filtered_bounce,
            IntensityPause = intensity_pause,
            FilteredIntensityPause = filtered_intensity_pause,
            BouncePause = bounce_pause,
            FilteredBouncePause = filtered_bounce_pause
        };
        return protoChronotensityBand;
    }

    PROTO.Chronotensity getChronotensity()
    {
        PROTO.Chronotensity protoChronotensity = new PROTO.Chronotensity
        {
            Bass = getChronotensityBand(0),
            Lowmid = getChronotensityBand(1),
            Highmid = getChronotensityBand(2),
            Treble = getChronotensityBand(3)
        };
        // printProtoDefinition(protoChronotensity);
        return protoChronotensity;
    }

    PROTO.AutoCorrelator getAutoCorrelator()
    {
        Vector2Int autoCorrelatorPos = new Vector2Int(0, 27);
        Vector2Int autoCorrelatorSize = new Vector2Int(128, 1);
        float[] autocorrelatorData = new float[autoCorrelatorSize.x];
        float[] uncorrelatedData = new float[autoCorrelatorSize.x];

        for (int i = 0; i < autoCorrelatorSize.x; i++)
        {
            int dataIndex = getIndexFromXY(autoCorrelatorPos.x + i, autoCorrelatorPos.y);
            autocorrelatorData[i] = audioLink.rawAudioData[dataIndex].x;
            uncorrelatedData[i] = audioLink.rawAudioData[dataIndex].y;
        }
        PROTO.AutoCorrelator protoAutoCorrelator = new PROTO.AutoCorrelator
        {
            Autocorrelation = { autocorrelatorData },
            Uncorrelated = { uncorrelatedData }
        };
        return protoAutoCorrelator;
    }

    PROTO.GeneralVU getGeneralVU()
    {
        //bunch of data we need to collect beforehand. Yipee
        Vector2Int versionPos = new Vector2Int(0, 22);
        //x is deprecated single float version number. Not sent because its already deprecated so it shouldnt be used?
        float versionMinor = audioLink.rawAudioData[getIndexFromXY(versionPos.x, versionPos.y)].y;
        float systemFPS = audioLink.rawAudioData[getIndexFromXY(versionPos.x, versionPos.y)].z;
        float versionMajor = audioLink.rawAudioData[getIndexFromXY(versionPos.x, versionPos.y)].w;

        Vector2Int frameratePos = new Vector2Int(1, 22);
        float frameCount = audioLink.rawAudioData[getIndexFromXY(frameratePos.x, frameratePos.y)].x;

        Vector2Int msSinceInstanceStart = new Vector2Int(2, 22);
        double msSinceStart = AudioLinkDecodeDataAsDouble(audioLink.rawAudioData[getIndexFromXY(msSinceInstanceStart.x, msSinceInstanceStart.y)]);

        Vector2Int msSinceMidnightLocal = new Vector2Int(3, 22);
        double msSinceMidnight = AudioLinkDecodeDataAsDouble(audioLink.rawAudioData[getIndexFromXY(msSinceMidnightLocal.x, msSinceMidnightLocal.y)]);

        //the fuck is network time in this context
        Vector2Int msInNetworkTime = new Vector2Int(4, 22);
        double msNetworkTime = AudioLinkDecodeDataAsDouble(audioLink.rawAudioData[getIndexFromXY(msInNetworkTime.x, msInNetworkTime.y)]);

        Vector2Int UTCDaysSinceEpochpos = new Vector2Int(5, 23);
        double UTCDaysSinceEpoch = AudioLinkDecodeDataAsDouble(audioLink.rawAudioData[getIndexFromXY(UTCDaysSinceEpochpos.x, UTCDaysSinceEpochpos.y)]);

        Vector2Int msSinceUTCDayStartpos = new Vector2Int(6, 23);
        double msSinceUTCDayStart = AudioLinkDecodeDataAsDouble(audioLink.rawAudioData[getIndexFromXY(msSinceUTCDayStartpos.x, msSinceUTCDayStartpos.y)]);

        Vector2Int mediaPosPos = new Vector2Int(7, 23);
        Vector4 mediaPos = audioLink.rawAudioData[getIndexFromXY(mediaPosPos.x, mediaPosPos.y)];

        PROTO.GeneralVU generalVU = new PROTO.GeneralVU
        {
            VersionMajor = versionMajor,
            VersionMinor = versionMinor,
            SystemFPS = systemFPS,
            FrameCount = frameCount,
            MsSinceInstanceStart = msSinceStart,
            MsSinceMidnightLocal = msSinceMidnight,
            MsInNetworkTime = msNetworkTime,
            UTCDaysSinceEpoch = UTCDaysSinceEpoch,
            MsSinceUTCDayStart = msSinceUTCDayStart,
            Position = new PROTO.Position
            {
                Lat = mediaPos.x,
                Lon = mediaPos.y,
            }
        };
        generalVU.MediaState = getMediaState();
        generalVU.PlayerData = getPlayerData();
        generalVU.CurrentIntensity = getIntensity(new Vector2Int(8, 22));
        generalVU.MarkerValue = getIntensity(new Vector2Int(9, 22));
        generalVU.MarkerTimes = getIntensity(new Vector2Int(10, 22));
        generalVU.Autogain = getAutogain();
        // printProtoDefinition(generalVU);
        return generalVU;
    }

    //helper function to print out a proto definition
    void printProtoDefinition(IMessage protoMessage)
    {
        string protoDefinition = protoMessage.ToString();
        Debug.Log(protoDefinition);
    }

    PROTO.MediaState getMediaState()
    {
        Vector2Int mediaStatePos = new Vector2Int(5, 22);
        //single pixel control
        var col = RawToColor(audioLink.rawAudioData[getIndexFromXY(mediaStatePos.x, mediaStatePos.y)]);
        float volume = col.r;
        float time = col.g;
        PROTO.PlaybackState playbackState;
        switch (Mathf.RoundToInt(col.b))
        {
            case 0:
                playbackState = PROTO.PlaybackState.None;
                break;
            case 1:
                playbackState = PROTO.PlaybackState.Playing;
                break;
            case 2:
                playbackState = PROTO.PlaybackState.Paused;
                break;
            case 3:
                playbackState = PROTO.PlaybackState.Stopped;
                break;
            case 4:
                playbackState = PROTO.PlaybackState.Loading;
                break;
            case 5:
                playbackState = PROTO.PlaybackState.Streaming;
                break;
            case 6:
                playbackState = PROTO.PlaybackState.Error;
                break;
            default:
                playbackState = PROTO.PlaybackState.None;
                break;
        }

        PROTO.LoopOrRandom loopOrRandom;
        switch (Mathf.RoundToInt(col.a))
        {
            case 0:
                loopOrRandom = PROTO.LoopOrRandom.None;
                break;
            case 1:
                loopOrRandom = PROTO.LoopOrRandom.Loop;
                break;
            case 2:
                loopOrRandom = PROTO.LoopOrRandom.LoopOne;
                break;
            case 3:
                loopOrRandom = PROTO.LoopOrRandom.Random;
                break;
            case 4:
                loopOrRandom = PROTO.LoopOrRandom.RandomAndLoop;
                break;
            default:
                loopOrRandom = PROTO.LoopOrRandom.None;
                break;
        }

        PROTO.MediaState mediaState = new PROTO.MediaState
        {
            MediaVolume = volume,
            MediaTime = time,
            MediaPlayback = playbackState,
            MediaLoop = loopOrRandom
        };
        return mediaState;
    }

    PROTO.History getHistory()
    {
        PROTO.History history = new PROTO.History();
        for (int i = 0; i < 4; i++)
        {
            var targetList = i switch
            {
                0 => history.Bass,
                1 => history.Lowmid,
                2 => history.Highmid,
                3 => history.Treble,
                _ => history.Bass
            };

            for (int j = 0; j < 128; j++)
            {
                float bandValue = getBandHistory(i, j);
                targetList.Add(bandValue);
            }
        }
        return history;
    }

    PROTO.ThemeColors getThemeColors()
    {
        PROTO.ThemeColors themeColors = new PROTO.ThemeColors();
        themeColors.ThemeColor0 = getThemeColor(0);
        themeColors.ThemeColor1 = getThemeColor(1);
        themeColors.ThemeColor2 = getThemeColor(2);
        themeColors.ThemeColor3 = getThemeColor(3);
        return themeColors;
    }

    float getBandHistory(int band, int index)
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return 0f;
        }

        // Validate rawAudioData exists
        if (!audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
        {
            Debug.LogWarning("AudioLink.rawAudioData is not initialized!");
            return 0f;
        }

        // Calculate the data index for the history
        int dataIndex = (band * 128) + index;

        // Validate index is within bounds
        if (dataIndex < 0 || dataIndex >= audioLink.rawAudioData.Length)
        {
            Debug.LogWarning($"AudioLink data index {dataIndex} out of bounds (length: {audioLink.rawAudioData.Length})");
            return 0f;
        }

        UnityEngine.Color sampleColor = RawToColor(audioLink.rawAudioData[dataIndex]);
        return sampleColor.grayscale;
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

        // Validate rawAudioData exists
        if (!audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
        {
            Debug.LogWarning("AudioLink.rawAudioData is not initialized!");
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
        if (dataIndex < 0 || dataIndex >= audioLink.rawAudioData.Length)
        {
            Debug.LogWarning($"AudioLink data index {dataIndex} out of bounds (length: {audioLink.rawAudioData.Length})");
            return 0f;
        }

        UnityEngine.Color sampleColor = RawToColor(audioLink.rawAudioData[dataIndex]);
        return sampleColor.grayscale;
    }

    PROTO.FilteredAudiolink getFilteredAudiolink()
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return new PROTO.FilteredAudiolink();
        }

        // Validate rawAudioData exists
        if (!audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
        {
            Debug.LogWarning("AudioLink.rawAudioData is not initialized!");
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
                if (dataIndex < 0 || dataIndex >= audioLink.rawAudioData.Length)
                {
                    Debug.LogWarning($"AudioLink data index {dataIndex} out of bounds (length: {audioLink.rawAudioData.Length})");
                    continue;
                }

                float bandValue = RawToColor(audioLink.rawAudioData[dataIndex]).grayscale;

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

    PROTO.WaveForm getWaveform()
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return new PROTO.WaveForm();
        }

        // Validate rawAudioData exists
        if (!audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
        {
            Debug.LogWarning("AudioLink.rawAudioData is not initialized!");
            return new PROTO.WaveForm();
        }

        Vector2Int startPos = new Vector2Int(0, 6);
        //size is 128 x 16
        const int totalWaveform = 128 * 16;
        float[] wav1 = new float[totalWaveform];
        float[] wav2 = new float[totalWaveform];
        float[] wav3 = new float[totalWaveform];
        float[] wav1diff = new float[totalWaveform];

        for (int index = 0; index < totalWaveform; index++)
        {
            var col = audioLink.rawAudioData[interpretMultiline(startPos, index)];
            wav1[index] = col.x;
            wav2[index] = col.y;
            wav3[index] = col.z;
            wav1diff[index] = col.w;
        }

        PROTO.WaveForm waveform = new PROTO.WaveForm()
        {
            Wav1 = { wav1 },
            Wav2 = { wav2 },
            Wav3 = { wav3 },
            Wav1Diff = { wav1diff }
        };


        return waveform;
    }

    PROTO.DFT getDFT()
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return new PROTO.DFT();
        }

        // Validate rawAudioData exists
        if (!audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
        {
            Debug.LogWarning("AudioLink.rawAudioData is not initialized!");
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
            var col = audioLink.rawAudioData[interpretMultiline(startPos, i)];
            mag[i] = col.x;
            magEQ[i] = col.y;
            magFilt[i] = col.z;
            magPhase[i] = col.w;
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

    PROTO.ColorChord getColorChord()
    {
        // #define ALPASS_CCCOLORS                 uint2(25,22) //Size: 11, 1
        Vector2Int colorsStartPos = new Vector2Int(25, 22);
        int colorsCount = 11;
        // #define ALPASS_CCSTRIP                  uint2(0,24)  //Size: 128, 1
        Vector2Int stripStartPos = new Vector2Int(0, 24);
        int stripCount = 128;
        // #define ALPASS_CCLIGHTS                 uint2(0,25)  //Size: 128, 2
        Vector2Int lightsStartPos = new Vector2Int(0, 25);
        int lightsCount = 128;

        //get the color sets from the data
        PROTO.Color[] colors = new PROTO.Color[colorsCount];
        for (int i = 0; i < colorsCount; i++)
        {
            colors[i] = convertRawVectorToProtoColor(audioLink.rawAudioData[interpretMultiline(colorsStartPos, i)]);
        }

        PROTO.Color[] strip = new PROTO.Color[stripCount];
        for (int i = 0; i < stripCount; i++)
        {
            strip[i] = convertRawVectorToProtoColor(audioLink.rawAudioData[interpretMultiline(stripStartPos, i)]);
        }

        //lights are represented in two discrete chunks in the actual proto
        //first line is the user facing, second line is internal
        PROTO.Color[] lights = new PROTO.Color[lightsCount];
        PROTO.Color[] lightsInternal = new PROTO.Color[lightsCount];
        for (int i = 0; i < lightsCount; i++)
        {
            lights[i] = convertRawVectorToProtoColor(audioLink.rawAudioData[interpretMultiline(lightsStartPos, i)]);
            lightsInternal[i] = convertRawVectorToProtoColor(audioLink.rawAudioData[interpretMultiline(new Vector2Int(lightsStartPos.x, lightsStartPos.y + 1), i)]);
        }

        PROTO.ColorChord chord = new PROTO.ColorChord
        {
            Colors = { colors },
            Strip = { strip },
            Lights = { lights },
            LightsInternal = { lightsInternal }
        };

        return chord;
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
        return convertRawVectorToProtoColor(audioLink.rawAudioData[dataIndex]);
    }

    //helper function to convert from XY coords in the texture to the index in the rawAudioData array
    int getIndexFromXY(int x, int y)
    {
        // Validate audioLink is assigned
        if (audioLink == null)
        {
            Debug.LogWarning("AudioLink is not assigned!");
            return -1;
        }

        // Validate rawAudioData exists
        if (!audioLink.rawAudioData.IsCreated || audioLink.rawAudioData.Length == 0)
        {
            Debug.LogWarning("AudioLink.rawAudioData is not initialized!");
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
        if (index < 0 || index >= audioLink.rawAudioData.Length)
        {
            Debug.LogWarning($"Calculated index {index} out of bounds (length: {audioLink.rawAudioData.Length})");
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
            if (logReceivedSerialData)
            {
                try
                {
                    if (serialPort != null && serialPort.IsOpen && serialPort.BytesToRead > 0)
                    {
                        byte[] buffer = new byte[Mathf.Min(serialPort.BytesToRead, 4096)];
                        int bytesRead = serialPort.Read(buffer, 0, buffer.Length);
                        
                        if (bytesRead > 0)
                        {
                            lock (queueLock)
                            {
                                for (int i = 0; i < bytesRead; i++)
                                {
                                    receivedDataBuffer.Add(buffer[i]);
                                }
                                
                                string dataString = System.Text.Encoding.ASCII.GetString(receivedDataBuffer.ToArray());
                                Debug.Log($"Received {receivedDataBuffer.Count} bytes: {dataString}");
                                receivedDataBuffer.Clear();
                            }
                        }
                    }
                }
                catch (System.Exception ex)
                {
                    Debug.LogError("Serial read error: " + ex.Message);
                }
            }
            
            if (dataToWrite == null)
            {
                // Small sleep to avoid busy-waiting when queue is empty
                Thread.Sleep(0);
            }
        }
    }

    void OnDestroy()
    {
        ShutdownSerialPort(true);
    }

    private static UnityEngine.Color RawToColor(Vector4 rawColor)
    {
        return new UnityEngine.Color(rawColor.x, rawColor.y, rawColor.z, rawColor.w);
    }

    private PROTO.Color convertRawVectorToProtoColor(Vector4 rawColor)
    {
        UnityEngine.Color color = RawToColor(rawColor);
        return new PROTO.Color
        {
            R = color.r,
            G = color.g,
            B = color.b
        };
        // //testing, represent as uint32 instead of float, to save space
        // return new PROTO.Color
        // {
        //     R = (uint)(color.r * 255.0f),
        //     G = (uint)(color.g * 255.0f),
        //     B = (uint)(color.b * 255.0f)
        // };
    }

    // private uint[] floatArrayToScaledUInt32Array(float[] floatArray)
    // {
    //     uint[] uintArray = new uint[floatArray.Length];
    //     for (int i = 0; i < floatArray.Length; i++)
    //     {
    //         uintArray[i] = floatToScaledUInt32(floatArray[i]);
    //     }
    //     return uintArray;
    // }

    // private uint floatToScaledUInt32(float value)
    // {
    //     // Clamp the value to [0, 1] range
    //     value = Mathf.Clamp01(value);
    //     const uint scaleFactor = uint.MaxValue;
    //     return (uint)(value * scaleFactor);
    // }

    private byte[] CompressZlibPayload(byte[] input)
    {
        if (input == null || input.Length == 0)
            return input;

        using (var output = new MemoryStream())
        {
            using (var compressor = new DeflateStream(output, System.IO.Compression.CompressionLevel.Fastest, true))
            {
                compressor.Write(input, 0, input.Length);
            }

            return output.ToArray();
        }
    }
}
