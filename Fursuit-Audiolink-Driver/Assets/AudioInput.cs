using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TMPro; // FIX: Restored to TMPro

public class AudioInput : MonoBehaviour
{
    public string deviceName;
    public TMP_Dropdown dropdown;
    public Slider slider;
    AudioSource audioSource;
    
    private Coroutine retryInitCoroutine;

    void Start()
    {
        audioSource = GetComponent<AudioSource>();
        
        // Start trying to initialize continuously if it fails or has no device yet
        StartInitRoutine();

        // Populate dropdown with available devices
        dropdown.ClearOptions();
        List<string> options = new List<string>();
        foreach (string device in Microphone.devices)
        {
            options.Add(device);
        }
        dropdown.AddOptions(options);

        changeAudioVolume(slider.value);
    }

    void StartInitRoutine()
    {
        if (retryInitCoroutine != null)
        {
            StopCoroutine(retryInitCoroutine);
        }
        retryInitCoroutine = StartCoroutine(TryInitMicRoutine());
    }

    IEnumerator TryInitMicRoutine()
    {
        while (true)
        {
            if (Microphone.devices.Length > 0)
            {
                if (string.IsNullOrEmpty(deviceName))
                {
                    deviceName = Microphone.devices[0]; 
                }

                // Try to spin up the mic hardware
                yield return StartCoroutine(InitMicOnceRoutine());
                
                // If it successfully created a clip and started recording, exit the loop
                if (audioSource.clip != null && Microphone.IsRecording(deviceName))
                {
                    Debug.Log("🎉 Microphone initialized successfully: " + deviceName);
                    retryInitCoroutine = null;
                    yield break; 
                }
            }
            else
            {
                Debug.LogWarning("⚠️ No microphone devices detected on the system. Retrying...");
            }

            // Wait 1 second before trying again
            yield return new WaitForSeconds(1.0f);
        }
    }

    IEnumerator InitMicOnceRoutine()
    {
        bool setupSuccessful = false;

        try
        {
            audioSource.Stop();
            Microphone.End(deviceName); // Ensure any hanging record instances are wiped

            int deviceMaxFreq, deviceMinFreq;
            Microphone.GetDeviceCaps(deviceName, out deviceMinFreq, out deviceMaxFreq);
            
            int sampleRate = (deviceMinFreq == 0 && deviceMaxFreq == 0) ? 44100 : deviceMaxFreq;
            
            // Assign the microphone record stream
            audioSource.clip = Microphone.Start(deviceName, true, 10, sampleRate);
            audioSource.loop = true;
            
            setupSuccessful = true;
        }
        catch (System.Exception e)
        {
            Debug.LogWarning("❌ Mic startup failed, retrying... Error: " + e.Message);
        }

        if (setupSuccessful && audioSource.clip != null)
        {
            float timer = 0;
            while (!(Microphone.GetPosition(deviceName) > 0) && timer < 1.0f)
            {
                timer += Time.deltaTime;
                yield return null;
            }

            audioSource.Play();
        }
    }

    public void changeMicInput(int deviceIndex)
    {
        // End all recording
        foreach (var device in Microphone.devices)
        {
            Microphone.End(device);
        }
        
        StartCoroutine(enableAudioSource());
        
        if (deviceIndex >= 0 && deviceIndex < Microphone.devices.Length)
        {
            deviceName = Microphone.devices[deviceIndex];
        }
        
        // Restart the retry routine for the newly selected device
        StartInitRoutine();
    }

    public void changeAudioVolume(float volume)
    {
        volume = (volume * 80) - 80;
        if (audioSource.outputAudioMixerGroup != null && audioSource.outputAudioMixerGroup.audioMixer != null)
        {
            audioSource.outputAudioMixerGroup.audioMixer.SetFloat("Audio Input Loopback", volume);
        }
    }

    IEnumerator enableAudioSource()
    {
        audioSource.enabled = false;
        yield return new WaitForSeconds(0.1f);
        audioSource.enabled = true;
    }
}

