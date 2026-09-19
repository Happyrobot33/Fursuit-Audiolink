using System;
using System.Linq;
using LibUsbDotNet;
using LibUsbDotNet.LibUsb;
using LibUsbDotNet.Main;
using UnityEngine;

/// <summary>
/// Cross-platform backend for the audiolink_transmitter's vendor-specific bulk data channel,
/// using libusb-1.0 via LibUsbDotNet (which supports WinUSB on Windows and libusb-1.0 directly
/// on Linux/macOS). On Windows this rides the same WinUsb.sys binding that the firmware's
/// MS OS 2.0 descriptor auto-negotiates; on Linux, non-root access typically requires a udev
/// rule granting permission for this VID/PID.
/// </summary>
public class LibUsbSerialDevice : IUsbSerialDevice
{
    private const int VendorId = 0x303A;  // Espressif
    private const int ProductId = 0x4020; // Must match idProduct in receiver.cpp's desc_device

    private UsbContext context;
    private IUsbDevice device;
    private UsbEndpointWriter writeEndpoint;

    public bool IsOpen { get; private set; }

    public bool Open()
    {
        Close();

        try
        {
            context = new UsbContext();
        }
        catch (DllNotFoundException ex)
        {
            Debug.LogError($"libusb native library was not found. Add the x64 libusb-1.0.dll to the Windows player directory: {ex.Message}");
            Close();
            return false;
        }
        catch (Exception ex)
        {
            Debug.LogError($"Failed to initialize libusb: {ex.Message}");
            Close();
            return false;
        }

        using (var deviceCollection = context.List())
        {
            var found = deviceCollection.FirstOrDefault(d => d.VendorId == VendorId && d.ProductId == ProductId);
            // Clone the device so it stays valid after this collection is disposed (per library docs).
            device = found?.Clone();
        }

        if (device == null)
        {
            Debug.LogWarning($"libusb: no device found with VID=0x{VendorId:X4} PID=0x{ProductId:X4}");
            Close();
            return false;
        }

        try
        {
            device.Open();
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"libusb: failed to open device (check udev permissions / try running with elevated privileges): {ex.Message}");
            Close();
            return false;
        }

        // Unlike Windows (where WinUSB/MS-OS-descriptors only expose the vendor interface),
        // libusb on Linux/macOS enumerates every interface of the composite device, so interface
        // index 0 isn't guaranteed to be the vendor bulk interface. Find the interface that
        // actually owns endpoint 0x01 OUT instead of assuming index 0.
        const byte bulkOutEndpointAddress = 0x01;
        var interfaces = device.Configs[0].Interfaces;
        var targetInterface = interfaces.FirstOrDefault(
            i => i.Endpoints.Any(e => e.EndpointAddress == bulkOutEndpointAddress)
        ) ?? interfaces[0];

        Debug.Log(
            $"libusb: found {interfaces.Count} interface(s); using interface {targetInterface.Number} "
            + $"(class 0x{targetInterface.Class:X2}) for endpoint 0x{bulkOutEndpointAddress:X2}"
        );

        try
        {
            // Best-effort: if a generic kernel driver has bound this interface (common on Linux
            // for composite USB devices), detach it before claiming so the write doesn't silently
            // go nowhere. No-op on platforms/backends that don't support it (e.g. Windows/WinUSB).
            if (device.SupportsDetachKernelDriver() && device.IsKernelDriverActive(targetInterface.Number))
            {
                device.SetAutoDetachKernelDriver(true);
            }
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"libusb: kernel driver check/detach failed (continuing anyway): {ex.Message}");
        }

        try
        {
            device.ClaimInterface(targetInterface.Number);
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"libusb: failed to claim interface {targetInterface.Number}: {ex.Message}");
            Close();
            return false;
        }

        writeEndpoint = device.OpenEndpointWriter(WriteEndpointID.Ep01);
        if (writeEndpoint == null)
        {
            Debug.LogWarning("libusb: failed to open bulk OUT endpoint");
            Close();
            return false;
        }

        IsOpen = true;
        return true;
    }

    public void Write(byte[] data, int offset, int length)
    {
        if (!IsOpen)
        {
            throw new InvalidOperationException("libusb device is not open");
        }

        byte[] toWrite = data;
        if (offset != 0)
        {
            toWrite = new byte[length];
            Array.Copy(data, offset, toWrite, 0, length);
        }

        // Kept short so ShutdownSerialPort's thread-join timeout can safely exceed the worst case,
        // avoiding a race where native USB handles get disposed while a write is still in flight.
        Error error = writeEndpoint.Write(toWrite, 300, out int bytesWritten);
        if (error != Error.Success)
        {
            throw new System.IO.IOException($"libusb write failed: {error}");
        }
        if (bytesWritten != length)
        {
            throw new System.IO.IOException($"libusb short write: {bytesWritten}/{length} bytes");
        }
    }

    public void Close()
    {
        IsOpen = false;
        writeEndpoint = null;

        // Don't call device.Close() here: UsbContext.Dispose() already disposes every device
        // still tracked in its OpenDevices list, so doing both double-releases the native
        // libusb device handle and crashes (double libusb_unref_device).
        device = null;

        context?.Dispose();
        context = null;
    }

    public void Dispose()
    {
        Close();
    }
}
