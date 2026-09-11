/* Usb bulk transfer debug
(c) 2026 by Malte Marwedel

SPDX-License-Identifier: Apache-2.0

*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "usbBulk.h"

#include "ledspiellib/boxusb.h"
#include "ledspiellib/leds.h"
#include "ledspiellib/mcu.h"
#include "ledspiellib/rs232debug.h"

#include "main.h"

#include "usbd_core.h"
#include "usb_std.h"

#include "utility.h"

//If enabled, read and writes are signalled by LEDs
//#define MSC_SIGNAL_LED

#define USB_STRING_MANUF 1
#define USB_STRING_PRODUCT 2
#define USB_STRING_SERIAL 3

/*With the STM32F405, double buffering is currently not working.
  It would work for the STM32L452.
*/
//#define USB_USE_DOUBLEBUFFERING

#define USB_VENDOR "Marwedels.de"
#define USB_PRODUCT "LedSpiel"
#define USB_SERIAL "BulkTest"


//Only 64 works
#define USB_BULK_BLOCKSIZE 64

#define USB_BULK_QUEUE_FROMHOST_LEN 50
#define USB_BULK_QUEUE_TOHOST_LEN 128

//How much time there may have passed to put a packet to the queue
#define USB_TIMEOUTS_MS 100

//from host to device (out)
#define USB_ENDPOINT_FROMHOST 0x02

//from device to host (in)
#define USB_ENDPOINT_TOHOST 0x81

/* The PID used here is reserved for general test purpose.
See: https://pid.codes/1209/
*/
uint8_t g_deviceDescriptor[] = {
	0x12,       //length of this struct
	0x01,       //always 1
	0x00, 0x01, //usb version
	0x0,        //device class
	0x0,        //subclass
	0x0,        //device protocol
	USB_MAX_PACKET_SIZE, //maximum packet size
	0x09,0x12,  //vid
	0x05,0x00,  //pid
	0x00,0x01,  //revision
	USB_STRING_MANUF,   //manufacturer index
	USB_STRING_PRODUCT, //product name index
	USB_STRING_SERIAL,  //serial number index
	0x01        //number of configurations
};

uint8_t g_DeviceConfiguration[] = {
	9,     //length of this entry
	0x2,   //device configuration
	32, 0, //total length of this struct
	0x1,   //number of interfaces
	0x1,   //this config
	0x0,   //descriptor of this config index, not used
	0x80, //bus powered
	25,   //50mA
	//vendor specific interface descriptor
	9,    //length
	0x04, //interface descriptor
	0x00, //interface number
	0x00, //alternate setting
	0x02, //no other endpoints
	0xFF, //vendor specific class
	0x00, //vendor specific sub class
	0x00, //vendor specific protocol
	0, //no string description given
	//bulk-in endpoint descriptor
	7,    //length
	0x05, //endpoint descriptor
	USB_ENDPOINT_TOHOST, //endpoint address - in, number 1
	0x02, //bulk endpoint
	USB_BULK_BLOCKSIZE, 0, //max 64byte per packet
	0x00, //interval, ignored
	//bulk-out endpoint descriptor
	7,    //length
	0x05, //endpoint descriptor
	USB_ENDPOINT_FROMHOST, //endpoint address - out, number 2
	0x02, //bulk endpoint
	USB_BULK_BLOCKSIZE, 0, //max 64byte per packet
	0x00, //interval, ignored
};

static struct usb_string_descriptor g_lang_desc     = USB_ARRAY_DESC(USB_LANGID_ENG_US);
static struct usb_string_descriptor g_manuf_desc_en = USB_STRING_DESC(USB_VENDOR);
static struct usb_string_descriptor g_prod_desc_en  = USB_STRING_DESC(USB_PRODUCT);
//specs allow only 0...9 and A...F as serial number for mass storage
static struct usb_string_descriptor g_serial_desc   = USB_STRING_DESC(USB_SERIAL);

usbd_device g_usbDev;

typedef struct {
	uint32_t data[USB_BULK_BLOCKSIZE / sizeof(uint32_t)];
	size_t len;
} bulk_t;

typedef struct {
	bool usbEnabled;
	uint8_t usbTraffic; //down counter for LED control
	uint32_t timeTrafficChecked;
	//bulk queue from PC
	uint8_t toHostFree; //free in the hardware buffer
	bulk_t toHost[USB_BULK_QUEUE_TOHOST_LEN];
	uint32_t toHostR;
	uint32_t toHostW;
	//bulk queue to PC
	bulk_t toDevice[USB_BULK_QUEUE_FROMHOST_LEN];
	uint32_t toDeviceR;
	uint32_t toDeviceW;
	//helper vars to properly serve libusb
	bool epBulkToHostEnabled;
	bool epBulkToDeviceEnabled;
	bool usbIsrDisabled;
} bulkState_t;

typedef struct {
	uint64_t ticksMain; //counts [µs], time processing, accessed from main
	uint64_t tickUsbIsrStart; //timestmap [µs], only accessed within the ISR
	uint32_t usbIsrCount; //accessed from main and ISR
	uint64_t ticksUsbIsr; //counts [µs], time processing, accessed from main and ISR
	uint32_t tickSampleStart; //stamp [ms] when the values above were resetted last, accessed from main
	uint64_t ticksMainLast; //relative time consumend within the last ~1s in [µs], accessed from main
	uint64_t ticksUsbIsrLast; //relative time consumend within the last ~1s in [µs], accessed from main
	uint32_t usbIsrCountLast; //number usb ISRs processed
	bool printPerformance;
} performanceState_t;

static bulkState_t g_bulkState;

static performanceState_t g_performanceState;


void UsbIrqOnEnter(void) {
#ifdef MSC_SIGNAL_LED
	Led1Off();
#endif
	g_performanceState.tickUsbIsrStart = McuTimestampUs();
}

void UsbIrqOnLeave(void) {
	g_bulkState.usbTraffic = 2;
	uint64_t tNow = McuTimestampUs();
	g_performanceState.ticksUsbIsr += tNow - g_performanceState.tickUsbIsrStart;
	g_performanceState.usbIsrCount++;
}

static usbd_respond usbGetDesc(usbd_ctlreq *req, void **address, uint16_t *length) {
	const uint8_t dtype = req->wValue >> 8;
	const uint8_t dnumber = req->wValue & 0xFF;
	void* desc = NULL;
	uint16_t len = 0;
	usbd_respond result = usbd_fail;
	//printfNowait("des: %x-%x-%x-%x\r\n", req->bmRequestType, req->bRequest, req->wValue, req->wIndex);
	switch (dtype) {
		case USB_DTYPE_DEVICE:
			desc = g_deviceDescriptor;
			len = sizeof(g_deviceDescriptor);
			result = usbd_ack;
			break;
		case USB_DTYPE_CONFIGURATION:
			desc = g_DeviceConfiguration;
			len = sizeof(g_DeviceConfiguration);
			result = usbd_ack;
			break;
		case USB_DTYPE_STRING:
			if (dnumber <= USB_STRING_SERIAL) {
				struct usb_string_descriptor * pStringDescr = NULL;
				if (dnumber == 0) {
					pStringDescr = &g_lang_desc;
				}
				if (dnumber == USB_STRING_MANUF) {
					pStringDescr = &g_manuf_desc_en;
				}
				if (dnumber == USB_STRING_PRODUCT) {
					pStringDescr = &g_prod_desc_en;
				}
				if (dnumber == USB_STRING_SERIAL) {
					pStringDescr = &g_serial_desc;
				}
				desc = pStringDescr;
				len = pStringDescr->bLength;
				result = usbd_ack;
			}
			break;
	}
	*address = desc;
	*length = len;
	return result;
}

//Must be called from the USB interrupt, or within the UsbLock from other threads
void UsbBulkDequeueToHost(usbd_device * dev) {
	uint32_t thisIndex = g_bulkState.toHostR;
	if ((g_bulkState.toHostW != thisIndex) && (g_bulkState.toHostFree > 0)) {
		size_t len = g_bulkState.toHost[thisIndex].len;
		if (usbd_ep_write(dev, USB_ENDPOINT_TOHOST, g_bulkState.toHost[thisIndex].data, len) == len) {
			uint32_t nextIndex = (thisIndex + 1) % USB_BULK_QUEUE_TOHOST_LEN;
			g_bulkState.toHostFree--;
			g_bulkState.toHostR = nextIndex;
		} else {
			//printfNowait("Err, write\r\n");
		}
	}
}

//Must be called from the USB interrupt, or within the UsbLock from other threads
bool UsbBulkQueueToHost(usbd_device * dev, const bulk_t * packet) {
	uint32_t thisIndex = g_bulkState.toHostW;
	uint32_t nextIndex = (thisIndex + 1) % USB_BULK_QUEUE_TOHOST_LEN;
	bool queued = false;
	if (g_bulkState.toHostR != nextIndex) { //space in queue
		memcpy(g_bulkState.toHost[thisIndex].data, packet->data, packet->len);
		g_bulkState.toHost[thisIndex].len = packet->len;
		g_bulkState.toHostW = nextIndex;
		queued = true;
	}
	UsbBulkDequeueToHost(dev); //try to send if there is space in the hardware buffer
	return queued;
}

bool UsbBulkDequeueToDevice(bulk_t * packet) {
	uint32_t thisIndex = g_bulkState.toDeviceR;
	if (g_bulkState.toDeviceW != thisIndex) {
		//printfNowait("FromIndex: %u\r\n", (unsigned int)thisIndex);
		memcpy(packet, &(g_bulkState.toDevice[thisIndex]), sizeof(bulk_t));
		uint32_t nextIndex = (thisIndex + 1) % USB_BULK_QUEUE_FROMHOST_LEN;
		g_bulkState.toDeviceR = nextIndex;
		return true;
	}
	return false;
}

//Must be called from the USB interrupt, or within the UsbLock from other threads
bool UsbBulkQueueToDevice(usbd_device * dev, const bulk_t * packet) {
	uint32_t thisIndex = g_bulkState.toDeviceW;
	uint32_t nextIndex = (thisIndex + 1) % USB_BULK_QUEUE_FROMHOST_LEN;
	if (g_bulkState.toDeviceR != nextIndex) { //space in queue
		//printfNowait("ToIndex: %u\r\n", (unsigned int)thisIndex);
		memcpy(&(g_bulkState.toDevice[thisIndex]), packet, sizeof(bulk_t));
		g_bulkState.toDeviceW = nextIndex;
		return true;
	}
	return false;
}

bool UsbBulkQueueToDeviceSpaceAvailable(void) {
	uint32_t thisIndex = g_bulkState.toDeviceW;
	uint32_t nextIndex = (thisIndex + 1) % USB_BULK_QUEUE_FROMHOST_LEN;
	if (g_bulkState.toDeviceR != nextIndex) {
		return true;
	}
	return false;
}

bool EndpointBulkOut(usbd_device *dev, uint8_t event, uint8_t ep) {
	bulk_t packet;
	if (UsbBulkQueueToDeviceSpaceAvailable() == false) {
		/*Disabling the USB ISR is the only option we have. Because unless
		  usbd_ep_read is read, the ISR stays active, resulting in an endless ISR
		  calling and the main loop would have no time to free the FIFO.
		*/
		//printfDirect("Bulk out event %u ep %u\r\n", event, ep);
		g_bulkState.usbIsrDisabled = true;
		UsbRxLvlIsrDisable();
		//printfDirect("To device queue full\r\n");
		return false;
	}
	uint32_t res = usbd_ep_read(dev, ep, &(packet.data[0]), sizeof(packet.data));
	if (res > 0) {
		packet.len = res;
		if (!UsbBulkQueueToDevice(dev, &packet)) {
			printfNowait("To device queue full\r\n");
		}
	} else {
		printfNowait("No data read\r\n");
	}
	return true;
}

//call from USB ISR or within UsbLock
void UsbBulkStateReset(void) {
	g_bulkState.toHostFree = 0;
	g_bulkState.toHostR = 0;
	g_bulkState.toHostW = 0;
	g_bulkState.toDeviceR = 0;
	g_bulkState.toDeviceW = 0;
}

#ifdef USB_USE_DOUBLEBUFFERING
//function copied from usb stack:
inline static USB_OTG_INEndpointTypeDef* EPIN(uint32_t ep) {
    return (void*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE + (ep << 5));
}
#if 0
inline static volatile uint16_t *EPR(uint8_t ep) {
    return (uint16_t*)((ep & 0x07) * 4 + USB_BASE);
}
#endif
#endif

bool EndpointEventTx(usbd_device *dev, uint8_t event, uint8_t ep) {
	if ((ep == USB_ENDPOINT_TOHOST) && (event == usbd_evt_eptx)) {
#ifdef USB_USE_DOUBLEBUFFERING
		/*The problem, when there is a high CPU load, not every sent USB tx packet
		  gets a proper callback. So there is the need to fix toHostFree back to 2
		  if this happens. Without the fix, the device continues to work as if only
		  one buffer is used.
		  So we simply read out the number of available packets:
		*/
		USB_OTG_INEndpointTypeDef* epi = EPIN(ep & 0x7);
		uint32_t bytesFree = ((epi->DTXFSTS) & 0xFFFF) * 4;
		g_bulkState.toHostFree = bytesFree / USB_BULK_BLOCKSIZE;
		//printfNowait("%u\r\n", g_storageState.toHostFree);
		//printfNowait("E%xFree: 0x%x\r\n", epi, epi->DTXFSTS);
		UsbBulkDequeueToHost(dev); //extra call if a callback was forgotten
#else
		g_bulkState.toHostFree++;
#endif
		UsbBulkDequeueToHost(dev); //normal call
	}
	return true;
}

static void UsbBulkDisableEp(usbd_device * dev) {
	if (g_bulkState.epBulkToHostEnabled) {
		usbd_ep_deconfig(dev, USB_ENDPOINT_TOHOST);
	}
	g_bulkState.epBulkToHostEnabled = false;
	if (g_bulkState.epBulkToDeviceEnabled) {
		usbd_ep_deconfig(dev, USB_ENDPOINT_FROMHOST);
	}
	g_bulkState.epBulkToDeviceEnabled = false;
}

static usbd_respond usbSetConf(usbd_device * dev, uint8_t cfg) {
	usbd_respond result = usbd_fail;
	switch (cfg) {
		case 0:
			//deconfig
			printfNowait("Deconfig\r\n");
			UsbBulkDisableEp(dev);
			break;
		case 1:
			//set config
			printfNowait("Set config\r\n\r\n");
			/*At least the stm32f429 driver allocates new FIFO buffer memory on every
			  usbd_ep_config call. So it would fail after multiple calls because
			  no more memory can be allocated. Therefore disable before is required.
			*/
			UsbBulkDisableEp(dev);
			UsbBulkStateReset();
			uint8_t epType = USB_EPTYPE_BULK;
#ifdef USB_USE_DOUBLEBUFFERING
			epType |= USB_EPTYPE_DBLBUF;
#endif
			if (!usbd_ep_config(dev, USB_ENDPOINT_TOHOST, epType, USB_BULK_BLOCKSIZE)) {
				printfNowait("Error, configure ep to host\r\n");
			} else {
				g_bulkState.epBulkToHostEnabled = true;
#ifdef USB_USE_DOUBLEBUFFERING2
				g_bulkState.toHostFree = 2;
#else
				g_bulkState.toHostFree = 1;
#endif
			}
			if (!usbd_ep_config(dev, USB_ENDPOINT_FROMHOST, epType, USB_BULK_BLOCKSIZE)) {
				printfNowait("Error, configure ep from host\r\n");
			} else {
				g_bulkState.epBulkToDeviceEnabled = true;
			}
			usbd_reg_endpoint(dev, USB_ENDPOINT_FROMHOST, &EndpointBulkOut);
			usbd_reg_event(dev, usbd_evt_eptx, EndpointEventTx);
			result = usbd_ack;
			break;
	}
	return result;
}

static usbd_respond usbControl(usbd_device *dev, usbd_ctlreq *req, usbd_rqc_callback *callback) {
	//Printing can be done here as long it is buffered. Otherwise it might be too slow
	//bulk only reset
	if ((req->bmRequestType == 0x21) && (req->bRequest == 0xFF) && (req->wValue == 0) && (req->wLength == 1)) {
		printfNowait("Bulk reset\r\n");
		UsbBulkStateReset();
		return usbd_ack;
	}
	//set interface
	if ((req->bmRequestType == 0x01) && (req->bRequest == 0x0B) && (req->wLength == 0)) {
		return usbd_ack; //we only use interface 0 anyway...
	}
	//get interface
	if ((req->bmRequestType == 0x81) && (req->bRequest == 0x0A) && (req->wValue == 0) && (req->wLength == 1)) {
		req->data[0] = 0;
		return usbd_ack;
	}
	/* Other valid commands:
	req: 0-5-6-0-0 -> set address
	req: 0-9-1-0-0 -> set configuration
	*/
	if ((req->bmRequestType != 0x80) && (req->bRequest != 0x6)) { //filter usbGetDesc resquests
		//printfNowait("req: %x-%x-%x-%x-%x\r\n", req->bmRequestType, req->bRequest, req->wValue, req->wIndex, req->wLength);
	}
	return usbd_fail;
}

void UsbBulkInit(void) {
	UsbLock();
	UsbBulkStateReset();
	UsbUnlock();
	printf("Starting USB\r\n");
	int32_t result = UsbStart(&g_usbDev, &usbSetConf, &usbControl, &usbGetDesc);
	if (result == -1) {
		printf("Error, failed to start clock. Error: %u\r\n", (unsigned int)result);
	}
	if (result == -2) {
		printf("Error, failed to set USB clock source\r\n");
	}
	if (result >= 0) {
		g_bulkState.usbEnabled = true;
		g_bulkState.usbIsrDisabled = false;
	}
}

void UsbBulkStop(void) {
	if (g_bulkState.usbEnabled == true) {
		printf("\r\nStopping USB\r\n");
		UsbStop();
		printf("USB disconnected\r\n");
		g_bulkState.usbEnabled = false;
	}
}

void UsbBulkStart(void) {
	if (g_bulkState.usbEnabled == false) {
		UsbBulkInit();
	}
}

bool UsbBulkQueueToHostWithTimeout(usbd_device *dev, bulk_t * packet) {
	uint32_t start = HAL_GetTick();
	const uint32_t timeout = USB_TIMEOUTS_MS;
	bool success;
	do {
		UsbLock();
		success = UsbBulkQueueToHost(dev, packet);
		UsbUnlock();
	} while ((!success) && ((HAL_GetTick() - start) < timeout));
	return success;
}

bool UsbBulkProcessLoop(void) {
	bulk_t packet;
	uint32_t packets = 0;
	size_t bytes = 0;
	UsbLock();
	while (UsbBulkDequeueToDevice(&packet)) {
		if (g_bulkState.usbIsrDisabled) {
			UsbRxLvlIsrEnable();
			g_bulkState.usbIsrDisabled = false;
			printf("Re-enable RX isr\r\n");
		}
		UsbUnlock();
		//outgoing packets need to be processed, otherwise UsbBulkQueueToHostWithTimeout might time out
		UsbLock();
		packets++;
		bytes += packet.len;
		if (!UsbBulkQueueToHostWithTimeout(&g_usbDev, &packet)) {
			printf("Error, could not queue block\r\n");
			break;
		}
		printf("%u: %02x\r\n", (unsigned int)packets, (uint8_t)(packet.data[0]));
	}
	UsbUnlock();
	if (packets) {
		printf("Copied %u packets, %u bytes\r\n", (unsigned int)packets, (unsigned int)bytes);
		return true;
	}
	return false;
}

void PrintPerformance(void) {
	unsigned int mainPerc = g_performanceState.ticksMainLast / 10000; //µs to percent
	unsigned int isrPerc = g_performanceState.ticksUsbIsrLast / 10000; //µs to percent
	unsigned int numIsr = g_performanceState.usbIsrCountLast;
	printf("CPU load of packet copying: %2u%c, Usb ISR: %03u - %2u%c, \r\n", mainPerc, '%', numIsr, isrPerc, '%');
}

static void UsbBulkTogglePrintPerformance(void) {
	g_performanceState.printPerformance = !g_performanceState.printPerformance;
}

bool UsbBulkCycle(char input) {
	//call this loop as fast as possible to get the maxium flash read/write performance
	switch (input) {
		case 'p': UsbBulkTogglePrintPerformance(); break;
		default: break;
	}

	//handle LED
	uint32_t stamp = HAL_GetTick();
	if ((stamp - g_bulkState.timeTrafficChecked) > 0) {
		g_bulkState.timeTrafficChecked = stamp;
		UsbLock();
		if (g_bulkState.usbTraffic) {
			g_bulkState.usbTraffic--;
		} else {
#ifdef MSC_SIGNAL_LED
			Led1Green(); //no ISR for ~2 cycles
#endif
		}
		UsbUnlock();
	}
	//handle flash read-write
	uint64_t tStart = McuTimestampUs();
	UsbLock();
	uint64_t ticksIsrStart = g_performanceState.ticksUsbIsr;
	UsbUnlock();
	bool todo = UsbBulkProcessLoop();
	if (todo) { //otherwise we would measure only the polling and have a higher value the more we poll
		UsbLock();
		uint64_t ticksIsrStop = g_performanceState.ticksUsbIsr;
		UsbUnlock();
		uint32_t tStop = McuTimestampUs();
		g_performanceState.ticksMain += tStop - tStart;
		/* Because the USB ISR runs while the ProcessFlashAccess has been processed,
		   the time for the ISRs is parts of the ticksMain too, so they must be
		   substracted to avoid a result of > 100%.
		   Since the variable is reset in the main loop, it can not overflow
		   during the measurement here.
		*/
		g_performanceState.ticksMain -= (ticksIsrStop - ticksIsrStart);
	}
	//handle performance statistics
	if ((HAL_GetTick() - g_performanceState.tickSampleStart) >= 1000) { //1s passed
		uint32_t lastStart = g_performanceState.tickSampleStart;
		UsbLock(); //atomic update
		uint32_t thisStart = HAL_GetTick();
		uint64_t ticksUsb = g_performanceState.ticksUsbIsr;
		g_performanceState.ticksUsbIsr = 0;
		g_performanceState.tickSampleStart = thisStart;
		uint32_t isrCount = g_performanceState.usbIsrCount;
		g_performanceState.usbIsrCount = 0;
		UsbUnlock();
		uint64_t ticksMain = g_performanceState.ticksMain;
		g_performanceState.ticksMain = 0;
		//if this is run after more than 1000ms, the values needs to be adjusted
		uint32_t delta = thisStart - lastStart;
		if ((delta > 0) && (g_performanceState.printPerformance)) {
			g_performanceState.ticksMainLast = ticksMain * 1000 / delta;
			g_performanceState.ticksUsbIsrLast = ticksUsb * 1000 / delta;
			g_performanceState.usbIsrCountLast = isrCount * 1000 / delta;
			PrintPerformance();
		}
	}
	return todo;
}

void UsbBulkQueue(const uint8_t * data, size_t len) {
	while (len) {
		bulk_t packet;
		size_t l = MIN(len, sizeof(packet.data));
		packet.len = l;
		memcpy(packet.data, data, l);
		UsbLock();
		UsbBulkQueueToHost(&g_usbDev, &packet);
		UsbUnlock();
		len -= l;
		data += l;
	}
}
