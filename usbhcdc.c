//*****************************************************************************
//
// usbhcdc.c - USB Communications Device Class host driver.
//
//
//
//*****************************************************************************

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/usb.h"
#include "usblib/usblib.h"
#include "usblib/usblibpriv.h"
#include "usblib/host/usbhost.h"
#include "usblib/host/usbhostpriv.h"
#include "usblib/host/usbhcdc.h"
#include "usblib/host/usbhscsi.h"

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Memory.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/hal/Timer.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Queue.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/WiFi.h>


//*****************************************************************************
//
//! \addtogroup usblib_host_class
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Forward declarations for the driver open and close calls.
//
//*****************************************************************************
static void *USBHCDCOpen(tUSBHostDevice *psDevice);
static void USBHCDCClose(void *pvInstance);

//*****************************************************************************
//
// This is the structure for an instance of a USB CDC host driver.
//
//*****************************************************************************
struct tUSBHCDCInstance
{
    //
    // Save the device instance.
    //
    tUSBHostDevice *psDevice;

    //
    // Used to save the callback.
    //
    tUSBHCDCCallback pfnCallback;

    //
    // This is the control interface.
    //
    uint8_t ui8IControl;

    //
    // Interrupt IN pipe.
    //
    uint32_t ui32IntInPipe;

    //
    // Bulk IN pipe.
    //
    uint32_t ui32BulkInPipe;

    //
    // Bulk OUT pipe.
    //
    uint32_t ui32BulkOutPipe;
};


//
// Line Coding Information Structure
//
struct tLineCoding
{
    long   dwDTERate;	// Data Terminal Rate in bits per second
    short  bCharFormat;	// 0 - 1 stop bit, 1 - 1.5 stop bits, 2 - 2 stop bits
    short  bParityType;	// 0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
    short  bDataBits;	// Data bits (5, 6, 7, 8 or 16)
};

//
// Line Status Information Structure
//
struct tLineStatus
{
    long wLength;	    // Size of this structure in bytes
    long dwRingerBitmap;  // Ringer Configuration bitmap for this line.
    long dwLineState;	    // Defines current state of the line.
    long dwCallState;	    // Defines current state of first call on the line.
};

//*****************************************************************************
//
// The array of USB CDC host drivers.
//
//*****************************************************************************
static tUSBHCDCInstance g_USBHCDCDevice =
{
    0
};

//*****************************************************************************
//
//! This constant global structure defines the CDC Driver that
//! is provided with the USB library.
//
//*****************************************************************************
const tUSBHostClassDriver g_sUSBHostCDCClassDriver =
{
	USB_CLASS_CDC,
    USBHCDCOpen,
    USBHCDCClose,
    0
};

//*****************************************************************************
//
// This function handles callbacks for the Bulk IN endpoint.
//
//*****************************************************************************
unsigned char SchedulePipeInBuffer[64];
static void USBHCDCINCallback(uint32_t ulPipe, uint32_t ulEvent)
{
        uint32_t ulSize;
        //
        // Handles a request to schedule a new request on the interrupt IN
        // pipe.
        //
        if(ulEvent == USB_EVENT_SCHEDULER)
        {
            USBHCDPipeSchedule(ulPipe, SchedulePipeInBuffer, 64);
        }

        //
        // Called when new data is available on the interrupt IN pipe.
        //
        if(ulEvent == USB_EVENT_RX_AVAILABLE)
        {
              //
              // If the callback exists then call it.
              //
              if(g_USBHCDCDevice.pfnCallback != 0)
              {
                  memset(SchedulePipeInBuffer,0,64);
                  //
                  // Read the Data out.
                  //
                  ulSize = USBHCDPipeReadNonBlocking(ulPipe, SchedulePipeInBuffer,
                                                 64);
                  //
                  // Call
                  //

                  g_USBHCDCDevice.pfnCallback(&g_USBHCDCDevice,
                                              CDC_EVENT_RX_AVAILABLE,
                                              (void*)SchedulePipeInBuffer,
                                              ulSize);
                  //
                  // Schedule another read
                  //
                  USBHCDPipeSchedule(ulPipe, SchedulePipeInBuffer, 64);
              }
        }
}

//*****************************************************************************
//
// This function handles callbacks for the Interrupt IN endpoint.
//
//*****************************************************************************
unsigned char SchedulePipeInterruptBuffer[100];

static void USBHCDCInterruptCallback(uint32_t ulPipe, uint32_t ulEvent)
{
        uint32_t ulSize;
        //
        // Handles a request to schedule a new request on the interrupt IN
        // pipe.
        //
        if(ulEvent == USB_EVENT_SCHEDULER)
        {
            USBHCDPipeSchedule(ulPipe, SchedulePipeInterruptBuffer, 100);
        }

        //
        // Called when new data is available on the interrupt IN pipe.
        //
        if(ulEvent == USB_EVENT_RX_AVAILABLE)
        {
              //
              // If the callback exists then call it.
              //
              if(g_USBHCDCDevice.pfnCallback != 0)
              {
                  //
                  // Read the Data out.
                  //
                  ulSize = USBHCDPipeReadNonBlocking(ulPipe, SchedulePipeInterruptBuffer,
                                                 100);
                  //
                  // Call
                  //
                  g_USBHCDCDevice.pfnCallback(&g_USBHCDCDevice,
                                              CDC_EVENT_INTERRUPT,
                                              (void*)SchedulePipeInterruptBuffer,
                                              ulSize);


                  //
                  // Schedule another read
                  //
                  USBHCDPipeSchedule(ulPipe, SchedulePipeInterruptBuffer, 100);

                  //UARTprintf("USBHCDCInterruptCallback\n\r");
              }
        }
}

//*****************************************************************************
//
// This function handles callbacks for the interrupt IN endpoint.
//
//*****************************************************************************
//static void
//HIDIntINCallback(uint32_t ui32Pipe, uint32_t ui32Event)
//{
//    int32_t i32Dev;
//
//    switch (ui32Event)
//    {
//        //
//        // Handles a request to schedule a new request on the interrupt IN
//        // pipe.
//        //
//        case USB_EVENT_SCHEDULER:
//        {
//            USBHCDPipeSchedule(ui32Pipe, 0, 1);
//            break;
//        }
//        //
//        // Called when new data is available on the interrupt IN pipe.
//        //
//        case USB_EVENT_RX_AVAILABLE:
//        {
//            //
//            // Determine which device this notification is intended for.
//            //
//            for(i32Dev = 0; i32Dev < MAX_HID_DEVICES; i32Dev++)
//            {
//                //
//                // Does this device own the pipe we have been passed?
//                //
//                if(g_psHIDDevice[i32Dev].ui32IntInPipe == ui32Pipe)
//                {
//                    //
//                    // Yes - send the report data to the USB host HID device
//                    // class driver.
//                    //
//                    g_psHIDDevice[i32Dev].pfnCallback(
//                                    g_psHIDDevice[i32Dev].pvCBData,
//                                    USB_EVENT_RX_AVAILABLE, ui32Pipe, 0);
//                }
//            }
//
//            break;
//        }
//    }
//}

//*****************************************************************************
//
//! This function is used to open an instance of the CDC driver.
//!
//! \param pDevice is a pointer to the device information structure.
//!
//! This function will attempt to open an instance of the CDC driver based on
//! the information contained in the pDevice structure.  This call can fail if
//! there are not sufficient resources to open the device.  The function will
//! return a value that should be passed back into USBCDCClose() when the
//! driver is no longer needed.
//!
//! \return The function will return a pointer to a CDC driver instance.
//
//*****************************************************************************
static void *
USBHCDCOpen(tUSBHostDevice *psDevice)
{
    //printf("USBHCDCOpen: Start\n");

    int32_t i32Idx, i32Idz;
    tEndpointDescriptor *psEndpointDescriptor;
    tInterfaceDescriptor *psInterface;

    //
    // Don't allow the device to be opened without closing first.
    //
    if(g_USBHCDCDevice.psDevice)
    {
		//printf("USBHCDCOpen: For Loop: i32Idx, %d\n", i32Idx);

        return(0);
    }

    //
    // Save the device pointer.
    //
    g_USBHCDCDevice.psDevice = psDevice;

    //
    //  Loop Through the first 3 Interfaces Searching for Endpoints
    //
    for(i32Idz = 0; i32Idz < 3; i32Idz++)
    {
		//
		// Get the interface descriptor.
		//
		psInterface = USBDescGetInterface(psDevice->psConfigDescriptor, i32Idz, 0);

		//
		// Loop through the endpoints of the device.
		// bNumEndPoints returning 1, should be 3 - will set manually, as there is always 3...then it works!!!
		//
		for(i32Idx = 0; i32Idx < 3; i32Idx++)  //psInterface->bNumEndpoints; i32Idx++)
		{
			//
			// Get the first endpoint descriptor.
			//
			psEndpointDescriptor =
				USBDescGetInterfaceEndpoint(psInterface, i32Idx,
											psDevice->ui32ConfigDescriptorSize);

			//System_printf("USBHCDCOpen: psEndPointDecriptor: %d  bmAttribute: %d\n", psEndpointDescriptor, psEndpointDescriptor->bmAttributes);
			//System_flush();

			//
			// If no more endpoints then break out.
			//
			if(psEndpointDescriptor == 0)
			{
				break;
			}

			//
			// See if this is an Interrupt endpoint.
			//
			if((psEndpointDescriptor->bmAttributes & USB_EP_ATTR_TYPE_M) == USB_EP_ATTR_INT)
			{
				//
				// Interrupt IN.
				//
				if(psEndpointDescriptor->bEndpointAddress & USB_EP_DESC_IN)
				{
					g_USBHCDCDevice.ui32IntInPipe =
							USBHCDPipeAlloc(0, USBHCD_PIPE_INTR_IN,
											psDevice, USBHCDCInterruptCallback);

					USBHCDPipeConfig(g_USBHCDCDevice.ui32IntInPipe,
								psEndpointDescriptor->wMaxPacketSize,
								psEndpointDescriptor->bInterval,
								(psEndpointDescriptor->bEndpointAddress &
								 USB_EP_DESC_NUM_M));

					//System_printf("USBHCDCOpen: Allocated and Configured Interrupt In Endpoint: Interface Index: %d  bEndPointAddress: %d  ui32BulkOutPipe: %d\n", i32Idz, psEndpointDescriptor->bEndpointAddress, g_USBHCDCDevice.ui32IntInPipe);
					//System_flush();
				}
			}

			//
			// See if this is a bulk endpoint.
			//
			if((psEndpointDescriptor->bmAttributes & USB_EP_ATTR_TYPE_M) == USB_EP_ATTR_BULK)
			{
				//
				// See if this is bulk IN or bulk OUT or Interrupt
				//
				//if((psEndpointDescriptor->bmAttributes & USB_EP_ATTR_INT) == USB_EP_ATTR_INT)
				//{
	//                //
	//                // Allocate the USB Pipe for this Bulk IN Interrupt endpoint.
	//                //
	//                g_USBHCDCDevice.ui32BulkInPipe =
	//                    USBHCDPipeAllocSize(0, USBHCD_PIPE_BULK_IN,
	//                                        psDevice,
	//                                        psEndpointDescriptor->wMaxPacketSize,
	//                                        USBHCDCInterruptCallback);
	//                //
	//                // Configure the USB pipe as a Bulk IN endpoint.
	//                //
	//                USBHCDPipeConfig(g_USBHCDCDevice.ui32BulkInPipe,
	//                                 psEndpointDescriptor->wMaxPacketSize,
	//                                 1,
	//                                 (psEndpointDescriptor->bEndpointAddress &
	//                                  USB_EP_DESC_NUM_M));
	//
	//				System_printf("USBHCDCOpen: Allocated and Configured Interrupt EndPoint**: Interface Index: %d  bEndPointAddress: %d  ui32BulkOutPipe: %d\n", i32Idz, psEndpointDescriptor->bEndpointAddress, g_USBHCDCDevice.ui32BulkOutPipe);
	//				System_flush();
    //
	//			}
				if(psEndpointDescriptor->bEndpointAddress & USB_EP_DESC_IN)
				{
					//
					// Allocate the USB Pipe for this Bulk IN endpoint.
					//
					g_USBHCDCDevice.ui32BulkInPipe =
						USBHCDPipeAllocSize(0, USBHCD_PIPE_BULK_IN,
											psDevice,
											psEndpointDescriptor->wMaxPacketSize,
											USBHCDCINCallback);
					//
					// Configure the USB pipe as a Bulk IN endpoint.
					//
					USBHCDPipeConfig(g_USBHCDCDevice.ui32BulkInPipe,
									 psEndpointDescriptor->wMaxPacketSize,
									 64,
									 (psEndpointDescriptor->bEndpointAddress &
									  USB_EP_DESC_NUM_M));

					//System_printf("USBHCDCOpen: Allocated and Configured Bulk In EndPoint: Interface Index: %d  bEndPointAddress: %d  ui32BulkOutPipe: %d\n", i32Idz, psEndpointDescriptor->bEndpointAddress, g_USBHCDCDevice.ui32BulkOutPipe);
					//System_flush();

				}
				else
				{
					//
					// Allocate the USB Pipe for this Bulk OUT endpoint.
					//
					g_USBHCDCDevice.ui32BulkOutPipe =
						USBHCDPipeAllocSize(0, USBHCD_PIPE_BULK_OUT,
											psDevice,
											psEndpointDescriptor->wMaxPacketSize,
											0);
					//
					// Configure the USB pipe as a Bulk OUT endpoint.
					//
					USBHCDPipeConfig(g_USBHCDCDevice.ui32BulkOutPipe,
									 psEndpointDescriptor->wMaxPacketSize,
									 0,
									 (psEndpointDescriptor->bEndpointAddress &
									  USB_EP_DESC_NUM_M));

					//System_printf("USBHCDCOpen: Allocated and Configured Bulk Out EndPoint: Interface Index: %d  bEndPointAddress: %d  ui32BulkOutPipe: %d\n", i32Idz, psEndpointDescriptor->bEndpointAddress, g_USBHCDCDevice.ui32BulkOutPipe);
					//System_flush();
				}
			}
		}
    }


    //
    // If there is a callback function call it to inform the application that
    // the device has been enumerated.
    //
    // To be used with the RTOS callback handler example.
    //
//    if(g_USBCDCDevice.pfnCallback != 0)
//    {
//        g_psHIDDevice[i32Dev].pfnCallback(
//                            g_psHIDDevice[i32Dev].pvCBData,
//                            USB_EVENT_CONNECTED,
//                            (uint32_t)&g_psHIDDevice[i32Dev], 0);
//    }

    //
    // If the callback exists, call it with an Open event.
    //
    if(g_USBHCDCDevice.pfnCallback != 0)
    {
       // printf("USBHCDCOpen: Callback Exists: Calling\n");
       // System_flush();

        g_USBHCDCDevice.pfnCallback(&g_USBHCDCDevice,
                                    CDC_EVENT_OPEN, 0, 0);
    }

    //
    // Return the only instance of this device.
    //
    return(&g_USBHCDCDevice);
}

//*****************************************************************************
//
//! This function is used to release an instance of the CDC driver.
//!
//! \param pvInstance is an instance pointer that needs to be released.
//!
//! This function will free up any resources in use by the CDC driver instance
//! that is passed in.  The \e pvInstance pointer should be a valid value that
//! was returned from a call to USBCDCOpen().
//!
//! \return None.
//
//*****************************************************************************
static void
USBHCDCClose(void *pvInstance)
{
    //
    // Do nothing if there is not a driver open.
    //
    if(g_USBHCDCDevice.psDevice == 0)
    {
        return;
    }

    //
    // Reset the device pointer.
    //
    g_USBHCDCDevice.psDevice = 0;

    //
    // Free the Bulk IN Interrupt pipe.
    //
    if(g_USBHCDCDevice.ui32IntInPipe != 0)
    {
        USBHCDPipeFree(g_USBHCDCDevice.ui32IntInPipe);
    }

    //
    // Free the Bulk IN pipe.
    //
    if(g_USBHCDCDevice.ui32BulkInPipe != 0)
    {
        USBHCDPipeFree(g_USBHCDCDevice.ui32BulkInPipe);
    }

    //
    // Free the Bulk OUT pipe.
    //
    if(g_USBHCDCDevice.ui32BulkOutPipe != 0)
    {
        USBHCDPipeFree(g_USBHCDCDevice.ui32BulkOutPipe);
    }

    //
    // If the callback exists then call it.
    //
    if(g_USBHCDCDevice.pfnCallback != 0)
    {
        //printf("USBHCDCClose: Callback Exists: Calling\n");

        g_USBHCDCDevice.pfnCallback(&g_USBHCDCDevice,
                                    CDC_EVENT_CLOSE, 0, 0);
    }
}

//*****************************************************************************
//
//! This function checks if a drive is ready to be accessed.
//!
//! \param ulInstance is the device instance to use for this read.
//!
//! This function checks if the current device is ready to be accessed.
//! It uses the \e ulInstance parameter to determine which device to check and
//! will return zero when the device is ready.  Any non-zero return code
//! indicates that the device was not ready.
//!
//! \return This function will return zero if the device is ready and it will
//! return a other value if the device is not ready or if an error occurred.
//
//*****************************************************************************
unsigned char TestPipeOut[] = "AT\n\r";
int32_t
USBHCDCReady(tUSBHCDCInstance *psCDCInstance)
{

	//System_printf("USBHCDCReady: Start\n");
	//System_flush();

    struct tLineCoding	lc;
    struct tLineStatus	ls;

    //
    // If there is no device present then return an error.
    //
    if(psCDCInstance->psDevice == 0)
    {
    	//System_printf("USBHCDCReady: No Device Present");
    	//System_flush();

        return(-1);
    }

    //
    //  Open an instance of the CDC driver.
    //
    //USBHCDCOpen(psCDCInstance->psDevice);
   // USBHCDCOpen(psCDCInstance->psDevice);
    //
    // Set Control Line
    //
    ACMSetControlLineState(psCDCInstance,2);  //ACMSetControlLineState(g_USBHCDCDevice.pDevice, g_USBHCDCDevice.pDevice->ulAddress,1);

    //
    // Set Control Line
    //
    ACMSetControlLineState(psCDCInstance, 0); //    ACMSetControlLineState(g_USBHCDCDevice.pDevice, g_USBHCDCDevice.pDevice->ulAddress,3);

    //
    // Get Line Status - we will just set for now, as we know what is required
    //
    //memset(&lc,0,sizeof(ls));
    //ACMGetLineStatus(g_USBHCDCDevice.psDevice, &ls);

    //
    // Set line coding
    //
    lc.dwDTERate = 19200;
    lc.bCharFormat = 0;
    lc.bParityType = 0;
    lc.bDataBits = 8;

    //
    // Get line coding
    //
    //memset(&lc,0,sizeof(lc));  // this reset everything in lc to 0
    //ACMGetLineCoding(g_USBHCDCDevice.psDevice, &lc);  - We will just set for now, as we know what is required.

    ACMSetLineCoding(psCDCInstance); //, &lc);

    //USBHCDPipeWrite(pCDCDevice->ulBulkOutPipe,
    //                          (unsigned char*)TestPipeOut, sizeof(TestPipeOut)-1);

    //
    // Success.
    //
    return(0);
}

//*****************************************************************************
//
//! This function is used to set control line
//!
//! \param State is a line state
//!
//!
//! \return None.
//
//*****************************************************************************
//void USBHCDCSetControlLine(int State)
//{
//        //
//        // Set Control Line
//        //
//        ACMSetControlLineState(g_USBHCDCDevice, State);
//}

//*****************************************************************************
//
//! This function is used to set appropriate line coding
//!
//! \param None
//!
//!
//! \return None.
//
//*****************************************************************************
//void USBHCDCSetLineCoding(void)
//{
//    struct tLineCoding	lc;
//    //
//    // Set line coding
//    // (need to set to what the device responds with in the USBHCDCSetLineCoding
//    // For now we will set this manually to what the mini controller responds with
//    // But this should be changed in the future to ensure support for other devices
//    // that may have different requirements.
//    //
//    lc.dwDTERate = 19200;  // This is the baud rate
//    lc.bCharFormat = 0; // 0 is equal to 1 stop bit
//    lc.bParityType = 0;  // 0 is equal to parity of none
//    lc.bDataBits = 8; // Number of data bits
//    ACMSetLineCoding(g_USBHCDCDevice, &lc);
//}

//*****************************************************************************
//
//! This function sends out data to connected CDC device.
//!
//! \param ulInstance is the device instance
//! \param pucData is the pointer to data buffer to be sent out
//! \param ulSize is the size of data to be sent out in bytes
//!
//! \return This function will return number of data in bytes scheduled to
//! 		sent out.
//
//*****************************************************************************
int32_t
USBHCDCWrite(tUSBHCDCInstance *psCDCInstance, unsigned char *pucData, int32_t ulSize)
{
	//System_printf("USBHCDCWrite: Start\n");
	//System_flush();

    //
    // If there is no device present then return an error.
    //
    if(psCDCInstance->psDevice == 0)
    {
        return(0);
    }

    //
    // Send data through OUT pipe  (MAY NEED TO LOOK AT psCDCInstance)
    //
    return (USBHCDPipeWrite(psCDCInstance->ui32BulkOutPipe, pucData, ulSize));
    //return (USBHCDPipeWrite(2, pucData, ulSize));

}


//*****************************************************************************
//
//! This function is used to send chars to Bulk Out Pipe
//!
//! \param *pcData is pointer to data
//! \param Size is number of bytes
//!
//!
//! \return None.
//
//*****************************************************************************
void USBHCDCSendChars(unsigned char *pucData, uint32_t ulSize)
{
    USBHCDPipeWrite(g_USBHCDCDevice.ui32BulkOutPipe, pucData, ulSize);
}


//*****************************************************************************
//
//! This function is used to send chars to Bulk Out Pipe
//!
//! \param *pcData is pointer to data
//! \param Size is number of bytes
//!
//!
//! \return None.
//
//*****************************************************************************
void USBHCDCSendString(unsigned char *pucData)
{
    int Size = 0, Page = 0;

    while(pucData[Size] != 0)
    {
        Size ++;
        if(Size % 64)
        {
            //
            // Send out full pipe
            //
            USBHCDPipeWrite(g_USBHCDCDevice.ui32BulkOutPipe,&pucData[Page], Size);

            //
            // Increment Page
            //
            Page++;
        }
    }

    //
    // If anything left, send out
    //
    if(Size)
    	USBHCDCWrite(0, pucData, Size);

    	//USBHCDPipeWrite(g_USBHCDCDevice.ulBulkOutPipe,&pucData[Page], Size);
}

//*****************************************************************************
//
//! This function is used to get line status
//!
//! \param *pcData is pointer to data
//! \param Size is number of bytes
//!
//!
//! \return None.
//
//*****************************************************************************
//void USBHCDCGetLineStatus(int *LineState, int *CallState)
//{
//    struct tLineStatus	ls;
//    //
//    // Get Line Status
//    //
//    memset(&ls,0,sizeof(ls));
//    ACMGetLineStatus(g_USBHCDCDevice, &ls);
//    *LineState = ls.dwLineState;
//    *CallState = ls.dwCallState;
//}

//*****************************************************************************
//
//! This function should be called before any devices are present to enable
//! the CDC class driver.
//!
//! \param ulDrive is the drive number to open.
//! \param pfnCallback is the driver callback for any CDC events.
//!
//! This function is called to open an instance of a CDC device.  It
//! should be called before any devices are connected to allow for proper
//! notification of drive connection and disconnection.  The \e ulDrive
//! parameter is a zero based index of the drives present in the system.
//! There are a constant number of drives, and this number should only
//! be greater than 0 if there is a USB hub present in the system.  The
//! application should also provide the \e pfnCallback to be notified of mass
//! storage related events like device enumeration and device removal.
//!
//! \return This function will return the driver instance to use for the other
//! CDC functions.  If there is no driver available at the time of
//! this call, this function will return zero.
//
//*****************************************************************************
tUSBHCDCInstance *
USBHCDCDriveOpen(uint32_t ui32Drive, tUSBHCDCCallback pfnCallback)
{
	//System_printf("USBCDriveOpen: Start\n");
	//System_flush();

    //
    // Only the first drive is supported and only one callback is supported.
    //
    if((ui32Drive != 0) || (g_USBHCDCDevice.pfnCallback))
    {
        return(0);
    }

    //
    // Save the callback.
    //
    g_USBHCDCDevice.pfnCallback = pfnCallback;

    //
    // Return the requested device instance.
    //
    return(&g_USBHCDCDevice);
}


//*****************************************************************************
//
//! This function sets ACM baud rate
//!
//! \param ulAddress is the device address on the USB bus.
//! \param *dataptr is pointer to line coding structure
//!
//!
//! \return None.
//
//*****************************************************************************
int ACMSetLineCoding(tUSBHCDCInstance *psCDCInstance)  //, struct tLineCoding *dataptr)
{
	//System_printf("ACMSetLineCoding: Start\n");
	//System_flush();

    tUSBRequest SetupPacket;

    //
    // This is a Class specific interface OUT request.
    //
    SetupPacket.bmRequestType = bmREQ_CDCOUT;

    //
    // Request a set baud rate
    //
    SetupPacket.bRequest = CDC_SET_LINE_CODING;
    SetupPacket.wValue = 0;

    //
    // Indicate the index
    //
    SetupPacket.wIndex = 0;

    //
    // Zero bytes request
    //
    SetupPacket.wLength = 7;  //sizeof(struct tLineCoding);


    // This is 19200, 8, N, 1
	unsigned char byteLC[] = { 0x00, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x08 };

	unsigned char *byteLCPtr;
	byteLCPtr = byteLC;

    //
    // Put the setup packet in the buffer and send the command.
    //
    if(USBHCDControlTransfer(0, &SetupPacket, psCDCInstance->psDevice, byteLCPtr, 7, MAX_PACKET_SIZE_EP0))
    {
		return 1;
    }

    return 0;
}

//*****************************************************************************
//
//! This function gets ACM line coding
//!
//! \param ulAddress is the device address on the USB bus.
//! \param *dataptr is pointer to line coding structure
//!
//! \return None.
//
//*****************************************************************************
int ACMGetLineCoding(tUSBHCDCInstance *psCDCInstance)   //, struct tLineCoding *dataptr)
{
	//System_printf("ACMGetLineCoding: Start\n");
	//System_flush();

    tUSBRequest SetupPacket;

    //
    // This is a Class specific interface OUT request.
    //
    SetupPacket.bmRequestType = bmREQ_CDCIN;

    //
    // Request a set baud rate
    //
    SetupPacket.bRequest = CDC_GET_LINE_CODING;
    SetupPacket.wValue = 0;

    //
    // Indicate the index
    //
    SetupPacket.wIndex = 0;

    //
    // Zero bytes request
    //
    SetupPacket.wLength = 7; // sizeof(struct tLineCoding);

    // This is 19200, 8, N, 1
	unsigned char byteLC[] = { 0x00, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x08 };

	unsigned char *byteLCPtr;
	byteLCPtr = byteLC;

    //
    // Put the setup packet in the buffer and send the command.
    //
    if(USBHCDControlTransfer(0, &SetupPacket, psCDCInstance->psDevice, byteLCPtr, 7, MAX_PACKET_SIZE_EP0))
    {
		return 1;
    }

    return 0;
}

//*****************************************************************************
//
//! This function sets ACM Control Line
//!
//! \param ulAddress is the device address on the USB bus.
//! \param State is the State of Control Line
//!
//! \return None.
//
//*****************************************************************************
int ACMSetControlLineState(tUSBHCDCInstance *psCDCInstance,  int State)
{
	//System_printf("ACMSetControlLineState: Start\n");
	//System_flush();

	tUSBRequest SetupPacket;

    //
    // This is a Class specific interface OUT request.
    //
    SetupPacket.bmRequestType = bmREQ_CDCOUT;  // 0 - host to

    //
    // Request a set baud rate
    //
    SetupPacket.bRequest = CDC_SET_CONTROL_LINE_STATE;
    SetupPacket.wValue = State;

    //
    // Indicate the index
    //
    SetupPacket.wIndex = State; // 2; //This depends on wValue - should be the same the wValue

    //
    // Zero bytes request
    //
    SetupPacket.wLength = 0;

    //
    // Put the setup packet in the buffer and send the command.
    //
    if(USBHCDControlTransfer(0, &SetupPacket, psCDCInstance->psDevice, 0, 0, MAX_PACKET_SIZE_EP0))
    {
		return 1;
    }

    return 0;
}

//*****************************************************************************
//
//! This function gets ACM Control Line
//!
//! \param ulAddress is the device address on the USB bus.
//! \param State is the State of Control Line
//!
//!
//! \return None.
//
//*****************************************************************************
int ACMGetLineStatus(tUSBHCDCInstance *psCDCInstance)  //, struct tLineStatus *dataptr)
{
	//System_printf("ACMGetLineState: Start\n");
	//System_flush();

    tUSBRequest SetupPacket;

    //
    // This is a Class specific interface IN request.
    //
    SetupPacket.bmRequestType = bmREQ_CDCIN;

    //
    // Request a set baud rate
    //
    SetupPacket.bRequest = CDC_GET_LINE_PARMS;
    SetupPacket.wValue = 0;

    //
    // Indicate the index
    //
    SetupPacket.wIndex = 0;

    //
    // Size of Structure
    //
    SetupPacket.wLength = 7; //sizeof(struct tLineStatus);

    //
    // Put the setup packet in the buffer and send the command.
    //
    if(USBHCDControlTransfer(0, &SetupPacket, psCDCInstance->psDevice, 0, 0, MAX_PACKET_SIZE_EP0))
    {
		return 1;
    }

    return 0;
}

//! \param ui32Index is the controller index to use for this transfer.
//! \param psSetupPacket is the setup request to be sent.
//! \param psDevice is the device instance pointer for this request.
//! \param pui8Data is the data to send for OUT requests or the receive buffer
//! for IN requests.
//! \param ui32Size is the size of the buffer in pui8Data.
//! \param ui32MaxPacketSize is the maximum packet size for the device for this
//! request.
//    ulRet = USBHCDControlTransfer(0, &sReq, pFTDIDevice->pDevice, ucData, 0, MAX_PACKET_SIZE_EP0);


////*****************************************************************************
////
////! This function gets Comm Feature
////!
////! \param ulAddress is the device address on the USB bus.
////! \param State is the State of Control Line
////!
////!
////! \return None.
////
////*****************************************************************************
//int ACMGetCommFeature(tUSBHostDevice *psDevice, struct tCommFeature *dataptr)
//{
//    tUSBRequest SetupPacket;
//
//    //
//    // This is a Class specific interface IN request.
//    //
//    SetupPacket.bmRequestType = bmREQ_CDCIN;
//
//    //
//    // Request a set baud rate
//    //
//    SetupPacket.bRequest = CDC_GET_COMM_FEATURE;
//    SetupPacket.wValue = 1;
//
//    //
//    // Indicate the index
//    //
//    SetupPacket.wIndex = 0;
//
//    //
//    // Size of Structure
//    //
//    SetupPacket.wLength = sizeof(struct tLineStatus);;
//
//    //
//    // Put the setup packet in the buffer and send the command.
//    //
//    if(USBHCDControlTransfer(0,
//                             &SetupPacket,
//                             psDevice,
//                             (unsigned char *)dataptr,
//                             sizeof(struct tLineStatus),
//                             MAX_PACKET_SIZE_EP0))
//    {
//		return 1;
//    }
//
//    return 0;
//}


//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
