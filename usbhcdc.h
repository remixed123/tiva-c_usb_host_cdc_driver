//*****************************************************************************
//
// usbhcdc.h - Definitions for the USB cdc host driver.
//
//
//*****************************************************************************

#ifndef __USBHCDC_H__
#define __USBHCDC_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! \addtogroup usblib_host_class
//! @{
//
//*****************************************************************************
  
typedef struct tUSBHCDCInstance tUSBHCDCInstance;

//*****************************************************************************
//
// These defines are the the events that will be passed in the \e ulEvent
// parameter of the callback from the driver.
//
//*****************************************************************************
#define CDC_EVENT_OPEN          1
#define CDC_EVENT_CLOSE         2
#define CDC_EVENT_RX_AVAILABLE  3
#define CDC_EVENT_INTERRUPT     4


#define USB_SETUP_HOST_TO_DEVICE                0x00    // Device Request bmRequestType transfer direction - host to device transfer
#define USB_SETUP_DEVICE_TO_HOST                0x80    // Device Request bmRequestType transfer direction - device to host transfer
#define USB_SETUP_TYPE_STANDARD                 0x00    // Device Request bmRequestType type - standard
#define USB_SETUP_TYPE_CLASS                    0x20    // Device Request bmRequestType type - class
#define USB_SETUP_TYPE_VENDOR                   0x40    // Device Request bmRequestType type - vendor
#define USB_SETUP_RECIPIENT_DEVICE              0x00    // Device Request bmRequestType recipient - device
#define USB_SETUP_RECIPIENT_INTERFACE           0x01    // Device Request bmRequestType recipient - interface
#define USB_SETUP_RECIPIENT_ENDPOINT            0x02    // Device Request bmRequestType recipient - endpoint
#define USB_SETUP_RECIPIENT_OTHER               0x03    // Device Request bmRequestType recipient - other

#define bmREQ_CDCOUT        USB_SETUP_HOST_TO_DEVICE|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE
#define bmREQ_CDCIN         USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE

// CDC Commands defined by PSTN 1.2
#define CDC_SET_COMM_FEATURE				0x02
#define CDC_GET_COMM_FEATURE				0x03
#define CDC_CLEAR_COMM_FEATURE				0x04
#define CDC_SET_AUX_LINE_STATE				0x10
#define CDC_SET_HOOK_STATE					0x11
#define CDC_PULSE_SETUP						0x12
#define CDC_SEND_PULSE						0x13
#define CDC_SET_PULSE_TIME					0x14
#define CDC_RING_AUX_JACK					0x15
#define CDC_SET_LINE_CODING					0x20
#define CDC_GET_LINE_CODING					0x21
#define CDC_SET_CONTROL_LINE_STATE			0x22
#define CDC_SEND_BREAK						0x23
#define CDC_SET_RINGER_PARMS				0x30
#define CDC_GET_RINGER_PARMS				0x31
#define CDC_SET_OPERATION_PARMS				0x32
#define CDC_GET_OPERATION_PARMS				0x33
#define CDC_SET_LINE_PARMS					0x34
#define CDC_GET_LINE_PARMS					0x35
#define CDC_DIAL_DIGITS						0x36

//// Comm Feature Information Structure
////
//struct tCommFeature
//{
//    long wLength;	    // Size of this structure in bytes
//    long dwRingerBitmap;  // Ringer Configuration bitmap for this line.
//    long dwLineState;	    // Defines current state of the line.
//    long dwCallState;	    // Defines current state of first call on the line.
//};

//*****************************************************************************
//
// The prototype for the USB CDC host driver callback function.
//
//*****************************************************************************
typedef void (*tUSBHCDCCallback)(tUSBHCDCInstance *psCDCInstance, uint32_t ui32Event, void *pvEventData, uint32_t ulSize);

//*****************************************************************************
//
// Prototypes for the USB CDC host driver APIs.
//
//*****************************************************************************
extern tUSBHCDCInstance * USBHCDCDriveOpen(uint32_t ui32Drive, tUSBHCDCCallback pfnCallback);
extern void USBHCDCDriveClose(tUSBHCDCInstance *psCDCInstance);
extern int32_t USBHCDCDriveReady(tUSBHCDCInstance *psCDCInstance);
extern int32_t USBHCDCReady(tUSBHCDCInstance *pCDCInstance);
extern int32_t USBHCDCBlockRead(tUSBHCDCInstance *psCDCInstance, uint32_t ui32LBA, uint8_t *pui8Data, uint32_t ui32NumBlocks);
extern int32_t USBHCDCBlockWrite(tUSBHCDCInstance *psCDCInstance, uint32_t ui32LBA, uint8_t *pui8Data, uint32_t ui32NumBlocks);
extern int32_t USBHCDCWrite(tUSBHCDCInstance *psCDCInstance, unsigned char *pucData, int32_t ulSize);
extern void USBHCDCSendChars(unsigned char *pcData, uint32_t Size);
extern void USBHCDCSendString(unsigned char *pcData);

int ACMSetLineCoding(tUSBHCDCInstance *psCDCInstance);  //, struct tLineCoding *dataptr);
int ACMGetLineCoding(tUSBHCDCInstance *psCDCInstance);  //,  struct tLineCoding *dataptr);
int ACMSetControlLineState(tUSBHCDCInstance *psCDCInstance,  int State);
int ACMGetLineStatus(tUSBHCDCInstance *psCDCInstance);  //, struct tLineStatus *dataptr);

//extern void USBHCDCGetLineStatus(int *LineStat, int *CallState);
//extern void USBHCDCSetControlLine(int State);
//extern void USBHCDCSetLineCoding(void);

////*****************************************************************************
////
//// A typedef pointer to a USB APP callback function.
//// This function is called by the USB library to indicate to the application
//// different events.
////
////*****************************************************************************
//typedef void (*tUSBStatusCallback)(int Status);
//typedef void (*tUSBRxCallback)(int Data);
//typedef void (*tUSBIntCallback)(void);
//
//
////*****************************************************************************
////
//// Prototypes for the USB APP
////
////*****************************************************************************
//extern short OSUSBInit(int TaskPriority,
//                tUSBStatusCallback pfnStatusCallback,
//                tUSBRxCallback pfnRxCallback,
//                tUSBIntCallback pfnIntCallback);
//
//extern void extUSB_ResetController(void);

//*****************************************************************************
//
//! @}
//
//*****************************************************************************

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif // __USBHCDC_H__
