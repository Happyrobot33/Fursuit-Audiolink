using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TMPro;

public class AudioInput : MonoBehaviour
{
    public string deviceName;
    public TMP_Dropdown dropdown;
    AudioSource audioSource;

    // Start is called before the first frame update
    void Start()
    {
        audioSource = GetComponent<AudioSource>();
        initMic();

        // Populate dropdown with available devices
        dropdown.ClearOptions();
        List<string> options = new List<string>();
        foreach (string device in Microphone.devices)
        {
            options.Add(device);
        }
        dropdown.AddOptions(options);

        changeAudioVolume(1f);
    }

    void initMic()
    {
        try
        {
            audioSource.Stop();
            int deviceMaxFreq,
                deviceMinFreq;
            Microphone.GetDeviceCaps(deviceName, out deviceMinFreq, out deviceMaxFreq);
            audioSource.clip = Microphone.Start(deviceName, true, 10, deviceMaxFreq);
            audioSource.loop = true;
            audioSource.Play();
        }
        catch (System.Exception e)
        {
            Debug.LogError(e);
        }
    }

    public void changeMicInput(int deviceIndex)
    {
        //end all recording
        foreach (var device in Microphone.devices)
        {
            Microphone.End(device);
        }
        //disable audio source
        StartCoroutine(enableAudioSource());
        deviceName = Microphone.devices[deviceIndex];
        initMic();
    }

    public void changeAudioVolume(float volume)
    {
        //remap 0 to 1 to -80 to 0
        volume = (volume * 80) - 80;
        audioSource.outputAudioMixerGroup.audioMixer.SetFloat("Audio Input Loopback", volume);
    }

    // coroutine to enable and disable audio source
    IEnumerator enableAudioSource()
    {
        audioSource.enabled = false;
        yield return new WaitForSeconds(0.1f);
        audioSource.enabled = true;
    }
}
