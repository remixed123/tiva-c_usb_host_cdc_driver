# Tiva C USB Host CDC Driver Readme

This driver currently only supports send data out the Tiva C USB Port to a device. If you requiring receive data, then you will need to update the code, which should be relatively simple once the enumeration takes place. This code is custom, so you may need to play around with it a bit to get enumeration working, as it depends on how your CDC device has set up its interfaces and endpoints. CDC devices can vary in this respect.

### USB Background Information

If you are only starting out with USB embedded host work, I suggest the following resources.

* USB in a Nutshell - http://www.beyondlogic.org/usbnutshell/usb1.shtml
* USB Embedded Hosts - http://www.amazon.com/USB-Embedded-Hosts-Developers-Guide/dp/1931448248
* USBlyzer - http://www.usblyzer.com/ - Great tool to look at how your USB device enumarates, how it has setup its interfaces and endpoints.
* A USB Protocol Analyzer - http://www.totalphase.com/ - If you can afford it, this will help you a lot. Wish I had one!

### Code Review

Included in the respository is the following code files

* usbhcdc.c and usbhcdc.h - this is the USB Host CDC driver that has been developed
* usbhostenum.c - this is one the main usb host libraries that comes with Tivaware. I needed to make some changes to this code, as I discovered a few bugs. When I last checked on the 5/1/2014, TI had not got around to resolving this bug yet. You will need to check this your self to see whether has been resolved. The bugs are discussed here - http://e2e.ti.com/support/microcontrollers/tiva_arm/f/908/t/296947.aspx

### Code Changes Required

You will need to add the following to usbhost.h

    extern const tUSBHostClassDriver g_sUSBHostCDCClassDriver;

Here are some of the important points for the program code. I suggest starting with one of the examples provided with Tivaware or TI-RTOS for a similar device. And then altering it to your needs.

mainprogram.c

    // USB Headers
    #include "usblib/usblib.h"
    #include "usblib/host/usbhost.h"
    #include "usblib/host/usbhcdc.h"

    // A list of available Host Class Drivers, we only add CDC driver
    static tUSBHostClassDriver const * const usbHCDDriverList[] = {
        &g_sUSBHostCDCClassDriver,
        &USBCDCH_eventDriver
    };

Add this to where you initialise the your USB CDC

    void USBCDCH_init(Void)
    {
    :
    :

        // Initialize the USB stack for host mode. 
        USBStackModeSet(0, eUSBModeForceHost, NULL);
    
        // Register host class drivers 
        USBHCDRegisterDrivers(0, usbHCDDriverList, numHostClassDrivers);

        // Open an instance of the CDC host driver 
        CDCInstance = USBHCDCDriveOpen(0, CDCCallback);
    :
    :
    }


Add this to your main program loop

    // Call the USB stack to keep it running.
    USBHCDMain();

Use something like this to send data out the bulk output 

    void singlePacket(tUSBHCDCInstance *psCDCInstance)
    {
    
    :
    :
         int byteCount = 0;
         byteCount = USBHCDCWrite(psCDCInstance, packetPtr, totalSize);
    :
    :
    }
