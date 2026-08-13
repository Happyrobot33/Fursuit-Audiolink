using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using VRCAudioLink;

public class StringsManager : MonoBehaviour
{
    public AudioLink audioLink;
    public string custom1;
    public string custom2;
    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        audioLink.customString1 = custom1;
        audioLink.customString2 = custom2;
        audioLink.UpdateCustomStrings();
    }
}
