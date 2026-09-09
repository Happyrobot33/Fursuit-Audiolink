/// <summary>
/// Common surface for the platform-specific USB bulk transports (WinUSB on Windows, libusb elsewhere)
/// used to talk to the audiolink_transmitter's vendor-specific data channel.
/// </summary>
public interface IUsbSerialDevice
{
    bool IsOpen { get; }
    bool Open();
    void Write(byte[] data, int offset, int length);
    void Close();
}
