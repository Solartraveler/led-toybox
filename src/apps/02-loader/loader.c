/* Loader
(c) 2025 by Malte Marwedel

SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "loader.h"

#include "ledspiellib/leds.h"
#include "ledspiellib/rs232debug.h"
#include "ledspiellib/boxusb.h"
#include "ledspiellib/mcu.h"

#include "dfuMemory.h"

#include "md5.h"

#include "main.h"

#include "usbd_core.h"
#include "usb_std.h"

#include "utility.h"
#include "tarextract.h"

#include "json.h"

#define USB_STRING_MANUF 1
#define USB_STRING_PRODUCT 2
#define USB_STRING_SERIAL 3
#define USB_STRING_TARGET 4

/* The PID used here is reserved for this DFU compatible project.
See: https://pid.codes/1209/
*/
uint8_t g_deviceDescriptor[] = {
	0x12,       //length of this struct
	0x01,       //always 1
	0x00,0x01,  //usb version
	0x0,        //device class
	0x0,        //subclass
	0x0,        //device protocol
	USB_MAX_PACKET_SIZE, //maximum packet size
	0x09,0x12,  //vid
	0x02,0x77,  //pid
	0x00,0x01,  //revision
	USB_STRING_MANUF,   //manufacturer index
	USB_STRING_PRODUCT, //product name index
	USB_STRING_SERIAL,  //serial number index
	0x01        //number of configurations
};

uint8_t g_DeviceConfiguration[] = {
	9,     //length of this entry
	0x2,   //device configuration
	27, 0, //total length of this struct
	0x1,   //number of interfaces
	0x1,   //this config
	0x0,   //descriptor of this config index, not used
	0x80, //bus powered
	25,   //50mA
	//DFU interface descriptor for copy to RAM
	9,    //length
	0x04, //interface descriptor
	0x00, //interface number
	0x00, //alternate setting
	0x00, //no other endpoints
	0xFE, //application specific class
	0x01, //device firmware upgrade code
	0x02, //DFU mode protocol
	USB_STRING_TARGET,
	//DFU functional descriptor
	9,    //length
	0x21, //dfu functional descriptor
	0x07, //no detach, communicate after mainfestation, support download + upload
	0xFF, 0x00, //255ms timeout

/* Number of bytes per write before requesting the state. Using more byts speeds things up.
   But the number must always be smaller than USB_BUFFERSIZE_BYTES - 8.
*/
	0x00, 0x08, //2048 byte
	0x01, 0x01, //DFU version 1.1
};

/* The dfu-util can not load to address 0. So we give an offset as workaround.
   The same offset must be encoded in the g_target_desc, and the dfu-upload.sh scripts
*/

#define DFU_FAKEOFFSET 0x1000

static struct usb_string_descriptor g_lang_desc     = USB_ARRAY_DESC(USB_LANGID_ENG_US);
static struct usb_string_descriptor g_manuf_desc_en = USB_STRING_DESC("marwedels.de");
static struct usb_string_descriptor g_prod_desc_en  = USB_STRING_DESC("LedSpiel");
static struct usb_string_descriptor g_serial_desc   = USB_STRING_DESC("LedLoader");


#define MAX_DESCRIPTOR_CHARS 48
//TODO: How to reserve 48 uint16_t in the dynamic array?
static struct usb_string_descriptor g_target_desc = USB_STRING_DESC("                                               ");

usbd_device g_usbDev;

#define DFU_STATUS_OK 0

//We can't write te memory - main thread is processing the data
#define DFU_STATUS_WRITE 3
//address out of range
#define DFU_STATUS_ADDRESS 8


#define DFU_STATE_DFUIDLE 2
#define DFU_STATE_DNBUSY 4
#define DFU_STATE_DOWNLOAD_IDLE 5
#define DFU_STATE_MANIFEST_SYNC 6
#define DFU_STATE_MANIFEST 7
#define DFU_STATE_ERROR 10

typedef struct {
	uintptr_t address;
	uintptr_t highestAddress;
	uint32_t blockId;
	uint32_t state;
	uint32_t status; //error status
	uint8_t target; //0 = only RAM, 1 = copy to external flash

	//accessed by the main thread for printing progress
	volatile uint32_t bytesDownloaded;

	//should the program be executed? Sets by the ISR, cleared by the main thread
	bool commStartProgram;

	/* Set and cleared by the main thread. If true, the ISR may not update the comm
	   variables and the transfer RAM. */
	bool commMainProcessing;
	uint32_t commFileSize;
	bool commToFlash; //should the file be stored in the flash?
	//set by the interrupt, cleared by the main thread
	bool commTransferDone;

} dfuState_t;

dfuState_t g_dfuState = {0, 0, 0, DFU_STATE_DFUIDLE, DFU_STATUS_OK,  0, false};

uint8_t * g_DfuMem;
size_t g_DfuMemSize;

#define TARFILENAME_MAX 64

typedef struct {
	bool usbEnabled;
	uint32_t downloadedLast;
	uint32_t ledCycle;

	uint8_t * memStart; //always equivalent to g_DfuMem,
	size_t memSize; //maximum allowed size, equivalent to g_DfuMemSize
	size_t tarSize;
	bool savedToDisk;
	bool inFile;
	char filename[TARFILENAME_MAX];
	uint32_t unixTimestamp;
	bool watchdogEnforced;
	bool watchdogEnabled;
	uint16_t watchdogCounter; //counts up every main loop cycle [10ms], resets the watchdog every 10s
} loaderState_t;

//Only functions starting with Loader shall use this global variable directly
loaderState_t g_loaderState;


//================== code for USB DFU ==============

void UsbIrqOnEnter(void) {
	Led1Red();
}

void UsbIrqOnLeave(void) {
	Led1Off();
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
			if (dnumber <= USB_STRING_TARGET) {
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
				if (dnumber == USB_STRING_TARGET) {
					pStringDescr = &g_target_desc;
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

static usbd_respond usbSetConf(usbd_device *dev, uint8_t cfg) {
	usbd_respond result = usbd_fail;
	switch (cfg) {
		case 0:
			//deconfig
			break;
		case 1:
			//set config
			result = usbd_ack;
			break;
	}
	return result;
}

//out will be always 6 bytes in length
void DfuGetStatus(uint8_t * out) {
	out[0] = g_dfuState.status; //dfu error code
	out[1] = 1; //can poll every 10ms (LSBs)
	out[2] = 0; //poll timeout MSBs
	out[3] = 0; //poll timeout HSBs
	out[4] = g_dfuState.state; //state after this command
	out[5] = 0; //status descriptor index
	//more or less the call of GetStatus should execute the current state, so
	//the call does the transition to the next state.
	if (g_dfuState.state == DFU_STATE_DNBUSY) {
		g_dfuState.state = DFU_STATE_DFUIDLE;
	}
	if (g_dfuState.state == DFU_STATE_MANIFEST_SYNC) {
		//printfNowait("Start prog\r\n");
		g_dfuState.state = DFU_STATE_MANIFEST;
		g_dfuState.commStartProgram = true;
	}
}

void DfuGetState(uint8_t * out) {
	out[0] = g_dfuState.state;
}

void DfuClearState(void) {
	g_dfuState.address = 0;
	g_dfuState.highestAddress = 0;
	g_dfuState.bytesDownloaded = 0;
	g_dfuState.blockId = 0;
	g_dfuState.state = DFU_STATE_DFUIDLE;
	g_dfuState.status = DFU_STATUS_OK;
}

void DfuTransferEnd(void) {
	g_dfuState.bytesDownloaded = 0; //only for progress shown.
	if ((g_dfuState.highestAddress) && (g_dfuState.commMainProcessing == false)) {
		g_dfuState.commFileSize = g_dfuState.highestAddress;
		if (g_dfuState.target == 1) {
			g_dfuState.commToFlash = true;
		} else {
			g_dfuState.commToFlash = false;
		}
		__sync_synchronize();
		g_dfuState.commTransferDone = true;
		__sync_synchronize();
	}
}

void DfuDownloadBlock(const uint8_t * data, size_t dataLen, uint32_t wValue) {
	size_t ramSize = g_DfuMemSize;
	bool accept = false;

	if (wValue > 0) {
		if (g_dfuState.state == DFU_STATE_DFUIDLE) {
			g_dfuState.blockId = wValue;
			g_dfuState.state = DFU_STATE_DOWNLOAD_IDLE;
			accept = true;
		}
		if (g_dfuState.state == DFU_STATE_DOWNLOAD_IDLE) {
			if ((g_dfuState.blockId + 1) == wValue) {
				g_dfuState.blockId = wValue;
				accept = true;
			}
		}
		if (accept) {
			if (dataLen == 0) {//end of file
				//printfNowait("Eof\r\n");
				g_dfuState.state = DFU_STATE_MANIFEST_SYNC;
			} else {
				if ((g_dfuState.address + dataLen) <= ramSize) {
					//printfNowait("Data\r\n");
					if (g_dfuState.commMainProcessing == false) {
						memcpy(g_DfuMem + g_dfuState.address, data, dataLen);
						g_dfuState.address += dataLen;
						g_dfuState.bytesDownloaded += dataLen;
						g_dfuState.highestAddress = MAX(g_dfuState.address, g_dfuState.highestAddress);
					} else {
						g_dfuState.state = DFU_STATE_ERROR;
						g_dfuState.status = DFU_STATUS_ADDRESS;
					}
				} else {
					g_dfuState.state = DFU_STATE_ERROR;
					g_dfuState.status = DFU_STATUS_WRITE;
				}
			}
		}
	} else if (dataLen > 0) {
		if (data[0] == 0x41) { //erase page
			//printfNowait("erase\r\n");
			g_dfuState.state = DFU_STATE_DNBUSY; //we are not interested in the state, we just need to support it for dfu-util
		}
		if ((data[0] == 0x21) && (dataLen == 5)) { //set address
			uint32_t address = (data[4] << 24) | (data[3] << 16) | (data[2] << 8) | data[1];
			address -= DFU_FAKEOFFSET;
			//printfNowait("Set address %x\r\n", address);
			if (address < ramSize) {
				g_dfuState.address = address;
				g_dfuState.state = DFU_STATE_DNBUSY;
			} else {
				g_dfuState.state = DFU_STATE_ERROR;
				g_dfuState.status = DFU_STATUS_ADDRESS;
			}
		}
		if (data[0] == 0x91) { //read unprotected. Not called by dfu-upload
			//printfNowait("read unprotected\r\n");
		}
	}
}

static usbd_respond usbControl(usbd_device *dev, usbd_ctlreq *req, usbd_rqc_callback *callback) {
	//Printing can be done here as long it is buffered. Otherwise it might be too slow
	if (req->wIndex == 0) { //the interface
		if (req->bmRequestType == 0xA1) { //DFU device -> host
			if (req->bRequest == 2) { //upload
				//Not implemented
				//return usbd_ack;
			}
			if ((req->bRequest == 3) && (req->wLength == 6)) { //get status
				//printfNowait("status\r\n");
				DfuGetStatus(req->data);
				return usbd_ack;
			}
			if ((req->bRequest == 5) && (req->wLength == 1)) { //get state
				//printfNowait("state\r\n");
				DfuGetState(req->data);
				return usbd_ack;
			}
		}
		if (req->bmRequestType == 0x21) { //DFU host -> device
			if ((req->bRequest == 0) && (req->wLength == 0)) { //detach -> start the application
				//printfNowait("detach\r\n");
			}
			if (req->bRequest == 1) { //download
				DfuDownloadBlock(req->data, req->wLength, req->wValue);
				return usbd_ack;
			}
			if ((req->bRequest == 4) && (req->wLength == 0)) { //clear status
				//by default, this is never called by dfu-util
				//printfNowait("clrState\r\n");
				DfuClearState();
				return usbd_ack;
			}
			if ((req->bRequest == 6) && (req->wLength == 0)) { //abort - this also marks the end of the transfer
				//printfNowait("abort\r\n");
				DfuTransferEnd();
				DfuClearState();
				return usbd_ack;
			}
		}
	}
	if ((req->bmRequestType == 0x01) && (req->bRequest == 0x0B) && (req->wLength == 0)) { //set interface
		if (req->wValue < 2) {
			g_dfuState.target = req->wValue;
			return usbd_ack;
		}
	}
	if ((req->bmRequestType == 0x81) && (req->bRequest == 0x0A) && (req->wValue == 0) && (req->wLength == 1)) { //get interface
		req->data[0] = g_dfuState.target;
		return usbd_ack;
	}

	if ((req->bmRequestType != 0x80) && (req->bRequest != 0x6)) { //filter usbGetDesc resquests
		//printfNowait("req: %x-%x-%x-%x-%x\r\n", req->bmRequestType, req->bRequest, req->wValue, req->wIndex, req->wLength);
	}

	return usbd_fail;
}

void MainMenu(void) {
	printf("\r\nSelect operation:\r\n");
	printf("h: This screen\r\n");
	printf("r: Reboot with reset controller\r\n");
	printf("b: Jump to ST DFU bootloader\r\n");
	printf("u: Toggle USB device\r\n");
}

//bufferOut must have >= elements than strlen(text)
void LoaderTextToDescriptor(const char * text, struct usb_string_descriptor * pDescr) {
	size_t l = strlen(text);
	for (uint32_t i = 0; i < l; i++) {
		pDescr->wString[i] = text[i];
	}
	pDescr->bDescriptorType = USB_DTYPE_STRING;
	pDescr->bLength = 2 + l * 2;
}

void AppInit(void) {
	DfuMemInit(&g_DfuMem, &g_DfuMemSize);
	LedsInit();
	Led1Yellow();
	uint8_t clockError = McuClockToHsePll(48000000, RCC_HCLK_DIV1);
	Rs232Init();
	printf("\r\nLedLoader %s\r\n", APPVERSION);
	if (clockError) {
		printf("Error, setting up PLL - %u\r\n", clockError);
	}
	g_loaderState.memStart = g_DfuMem;
	g_loaderState.memSize = g_DfuMemSize;
	char buffer[MAX_DESCRIPTOR_CHARS];
	//the g encodes: read + write + eraseable
	snprintf(buffer, sizeof(buffer), "@Internal RAM/0x%x/%04u*0001Kg", DFU_FAKEOFFSET, (unsigned int)(g_DfuMemSize / 1024));
	LoaderTextToDescriptor(buffer, &g_target_desc);
	int32_t result = UsbStart(&g_usbDev, &usbSetConf, &usbControl, &usbGetDesc);
	if (result == -1) {
		printf("Error, failed to start 48MHz clock. Error: %u\r\n", (unsigned int)result);
	}
	if (result == -2) {
		printf("Error, failed to set USB clock source\r\n");
	}
	if (result >= 0) {
		g_loaderState.usbEnabled = true;
	}
	Led1Green();
	printf("Ready. Press h for available commands\r\n");
}

void PrepareOtherProgam(void) {
	Led2Off();
	//first all peripheral clocks should be disabled
	UsbStop(); //stops USB clock
	Rs232Flush();
	Rs232Stop(); //stops UART1 clock
}

void JumpDfu(void) {
	uintptr_t dfuStart = ROM_BOOTLOADER_START_ADDRESS;
	Led1Green();
	printf("\r\nDirectly jump to the DFU bootloader\r\n");
	volatile uintptr_t * pProgramStart = (uintptr_t *)(dfuStart + 4);
	printf("Program start will be at 0x%x\r\n", (unsigned int)(*pProgramStart));
	PrepareOtherProgam();
	McuStartOtherProgram((void *)dfuStart, true); //usually does not return
}

bool MetaNameGet(uint8_t * jsonStart, size_t jsonLen, char * nameOut, size_t nameMax) {
	return JsonValueGet(jsonStart, jsonLen, "name", nameOut, nameMax);
}

bool MetaMcuGet(uint8_t * jsonStart, size_t jsonLen, char * mcuOut, size_t mcuMax) {
	return JsonValueGet(jsonStart, jsonLen, "mcu", mcuOut, mcuMax);
}

bool MetaVersionGet(uint8_t * jsonStart, size_t jsonLen, char * nameOut, size_t nameMax) {
	return JsonValueGet(jsonStart, jsonLen, "version", nameOut, nameMax);
}

bool MetaDateGet(uint8_t * jsonStart, size_t jsonLen, char * nameOut, size_t nameMax) {
	return JsonValueGet(jsonStart, jsonLen, "compiled", nameOut, nameMax);
}

bool MetaLicenseGet(uint8_t * jsonStart, size_t jsonLen, char * nameOut, size_t nameMax) {
	return JsonValueGet(jsonStart, jsonLen, "license", nameOut, nameMax);
}

bool MetaAuthorGet(uint8_t * jsonStart, size_t jsonLen, char * nameOut, size_t nameMax) {
	return JsonValueGet(jsonStart, jsonLen, "author", nameOut, nameMax);
}

bool MetaWatchdogGet(uint8_t * jsonStart, size_t jsonLen, uint16_t * watchdogOut) {
	char buffer[16];
	bool success = JsonValueGet(jsonStart, jsonLen, "watchdog", buffer, sizeof(buffer));
	if (success) {
		*watchdogOut = AsciiScanDec(buffer);
		return true;
	}
	return false;
}

bool MetaProgramStartGet(uint8_t * jsonStart, size_t jsonLen, uintptr_t * programStart) {
	char addr[20] = {0};
	if (JsonValueGet(jsonStart, jsonLen, "appaddr", addr, sizeof(addr))) {
		uint32_t x = AsciiScanHex(addr);
		*programStart = x;
		return true;
	}
	return false;
}

bool MetaMd5Get(uint8_t * jsonStart, size_t jsonLen, uint8_t * checksumOut) {
	char checksum[33] = {0};
	if (JsonValueGet(jsonStart, jsonLen, "md5sum", checksum, sizeof(checksum))) {
		for (uint32_t i = 0; i < 16; i++) {
			char tmp[3] = {0};
			tmp[0] = checksum[i * 2];
			tmp[1] = checksum[i * 2 + 1];
			uint32_t out = AsciiScanHex(tmp);
			checksumOut[i] = out;
		}
		return true;
	}
	return false;
}

bool TarNameGet(uint8_t * tarStart, size_t tarLen, char * nameOut, size_t nameMax) {
	uint8_t * metaStart;
	size_t metaLen;
	if (TarFileStartGet("metadata.json", tarStart, tarLen, &metaStart, &metaLen, NULL)) {
		return JsonValueGet(metaStart, metaLen, "name", nameOut, nameMax);
	}
	return false;
}

void ProgTarStart(void * tarStart, size_t tarLen, bool watchdogEnforced, bool watchdogEnabled) {
	uint8_t * fileStart;
	size_t fileLen;
	uint8_t * metaStart;
	size_t metaLen;
	uint16_t watchdogTimeout = 0;
	uintptr_t programStart = 0;
	if (TarFileStartGet("metadata.json", tarStart, tarLen, &metaStart, &metaLen, NULL)) {
		if (!MetaWatchdogGet(metaStart, metaLen, &watchdogTimeout)) {
			printf("Warning, no watchdog value present\r\n");
		}
		if (!MetaProgramStartGet(metaStart, metaLen, &programStart)) {
			printf("Warning, no program start address present\r\n");
		}
	} else {
		printf("Warning, no metadata present\r\n");
	}
	if (TarFileStartGet("application.bin", tarStart, tarLen, &fileStart, &fileLen, NULL)) {
		if (fileLen <= g_DfuMemSize) {
			void * ramStart = g_DfuMem;
			if ((programStart >= (uintptr_t)g_DfuMem) && ((programStart + fileLen) <= ((uintptr_t)g_DfuMem+ g_DfuMemSize))) {
				ramStart = (void *)programStart;
			} else {
				printf("Error, address 0x%x out of bounds\r\n", (unsigned int)programStart);
				return;
			}
			memmove(ramStart, fileStart, fileLen);
			uint8_t md5sum[16] = {0};
			md5(ramStart, fileLen, md5sum);
			printf("Starting program with size %u. Md5sum:\r\n", (unsigned int)fileLen);
			PrintHex(md5sum, sizeof(md5sum));
			Led1Green();
			volatile uint32_t * pProgramStart = (uint32_t *)((uintptr_t)ramStart + 0x4);
			printf("Program start will be at 0x%x\r\n", (unsigned int)(*pProgramStart));
			PrepareOtherProgam();
			McuStartOtherProgram(ramStart, true); //usually does not return
		}
	}
	printf("Error, program start failed\r\n");
}

bool ProgTarCheck(void * tarStart, size_t tarLen) {
	uint8_t * fileStart;
	size_t fileLen = 0;
	uint8_t * metaStart;
	size_t metaLen;
	uint8_t md5sum1[16];
	uint8_t md5sum2[16];
	if (!TarFileStartGet("application.bin", tarStart, tarLen, &fileStart, &fileLen, NULL)) {
		printf("Error, no application found. Tar len: %u\r\n", (unsigned int)tarLen);
		md5(tarStart, tarLen, md5sum1);
		PrintHex(md5sum1, sizeof(md5sum1));
		return false;
	}
	md5(fileStart, fileLen, md5sum1);
	if (!TarFileStartGet("metadata.json", tarStart, tarLen, &metaStart, &metaLen, NULL)) {
		printf("Error, no metadata found\r\n");
		return false;
	}
	if (!MetaMd5Get(metaStart, metaLen, md5sum2)) {
		printf("Error, no checksum in metadata\r\n");
		return false;
	}
	if (memcmp(md5sum1, md5sum2, sizeof(md5sum1))) {
		printf("Error, checksum mismatch\r\n");
		printf("Should:\r\n");
		PrintHex(md5sum2, sizeof(md5sum2));
		printf("Is:\r\n");
		PrintHex(md5sum1, sizeof(md5sum1));
		return false;
	}
	char mcu[16];
	if (!MetaMcuGet(metaStart, metaLen, mcu, sizeof(mcu))) {
		printf("Error, no MCU in metadata\r\n");
		return false;
	}
	if (strcasecmp(mcu, "STM32F405")) {
		printf("Error, program is for wrong MCU %s\r\n", mcu);
		return false;
	}
	return true;
}

void ToggleUsb(void) {
	if (g_loaderState.usbEnabled == true) {
		printf("\r\nStopping USB\r\n");
		UsbStop();
		printf("USB disconnected\r\n");
		g_loaderState.usbEnabled = false;
	} else {
		printf("\r\nRestarting USB\r\n");
		int32_t result = UsbStart(&g_usbDev, &usbSetConf, &usbControl, &usbGetDesc);
		printf("Result: %i\r\n", (int)result);
		g_loaderState.usbEnabled = true;
	}
}

void LoaderMemLock(void) {
	g_dfuState.commMainProcessing = true;
	__sync_synchronize();
}

void LoaderMemUnlock(void) {
	__sync_synchronize();
	g_dfuState.commMainProcessing = false; //now the ISR may access again
	__sync_synchronize();
}

void LoaderProgramStart(void) {
	LoaderMemLock();
	if (ProgTarCheck(g_loaderState.memStart, g_loaderState.tarSize)) {
		ProgTarStart(g_loaderState.memStart, g_loaderState.tarSize,
		  g_loaderState.watchdogEnforced, g_loaderState.watchdogEnabled); //usually does not return
	}
	__disable_irq();
	g_dfuState.commStartProgram = false;
	LoaderMemUnlock();
	__enable_irq();
}

void LoaderUpdateTimestamp(void) {
	uint8_t * startAddr;
	size_t fileLen;
	g_loaderState.unixTimestamp = 0;
	TarFileStartGet("application.bin", g_loaderState.memStart, g_loaderState.tarSize, &startAddr, &fileLen, &(g_loaderState.unixTimestamp));
}

void AppCycle(void) {
	//led flash
	if (g_loaderState.ledCycle < 25) {
		Led2Green();
	} else {
		Led2Off();
	}
	if (g_loaderState.ledCycle >= 50) {
		g_loaderState.ledCycle = 0;
	}
	g_loaderState.ledCycle++;

	char input = Rs232GetChar();
	if (input) {
		printf("%c", input);
	}
	switch (input) {
		case 'h': MainMenu(); break;
		case 'r': NVIC_SystemReset(); break;
		case 'b': JumpDfu(); break;
		case 'u': ToggleUsb(); break;
		default: break;
	}
	uint32_t downloaded = g_dfuState.bytesDownloaded;
	if ((downloaded != g_loaderState.downloadedLast) && (downloaded > 0)) {
		g_loaderState.downloadedLast = downloaded;
		printf("\rDownloading... %u\r", (unsigned int)downloaded);
		Rs232Flush(); //needed for PC emulation
	}
	__disable_irq();
	bool transferDone = g_dfuState.commTransferDone;
	//startProgram can be set in an other cycle than transferDone becomes true
	bool startProgram = g_dfuState.commStartProgram;
	if (transferDone) {
		LoaderMemLock();
	}
	__enable_irq();
	if (transferDone) {
		g_loaderState.tarSize = g_dfuState.commFileSize;
		g_loaderState.watchdogEnforced = false;
		printf("Program with %ubytes transferred\r\n", (unsigned int)g_loaderState.tarSize);
		if (ProgTarCheck(g_loaderState.memStart, g_loaderState.tarSize)) {
			g_loaderState.inFile = false;
			char name[32];
			if (TarNameGet(g_loaderState.memStart, g_loaderState.tarSize, name, sizeof(name))) {
				snprintf(g_loaderState.filename, TARFILENAME_MAX, "/bin/%s.tar", name);
			} else {
				g_loaderState.filename[0] = '\0';
				printf("Error, could not get program name\r\n");
			}
			LoaderUpdateTimestamp();
			printf("Valid program in RAM. Awaiting action\r\n");
		}
		g_dfuState.commTransferDone = false;
		LoaderMemUnlock();
	}
	if (startProgram) {
		LoaderProgramStart();
	}
	HAL_Delay(10); //call this loop ~100x per second
}
