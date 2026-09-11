/* Ledspiellib based on Boxlib
(c) 2025 by Malte Marwedel

SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdint.h>
#include <stdbool.h>

#include "ledspiellib/boxusb.h"

#include "ledspiellib/rs232debug.h"
#include "main.h"
#include "usbd_core.h"
#include "usb.h"

usbd_device * g_pUsbDev;

//If another size is needed, define it at main.h
//8 byte are used for control data, everything else is user data
#ifndef USB_BUFFERSIZE_BYTES
#define USB_BUFFERSIZE_BYTES 50
#endif

//must be 4byte aligned
static uint32_t g_usbBuffer[USB_BUFFERSIZE_BYTES / sizeof(uint32_t)];

static uint8_t g_usbLockLevel; //protected by the disabled ISR itself

__weak void UsbIrqOnEnter(void) {
}

__weak void UsbIrqOnLeave(void) {
}

void OTG_FS_IRQHandler(void) {
	UsbIrqOnEnter();
	usbd_poll(g_pUsbDev);
	UsbIrqOnLeave();
}

int32_t UsbStartAdv(usbd_device * usbDev, usbd_cfg_callback configCallback,
 usbd_ctl_callback controlCallback, usbd_dsc_callback descriptorCallback,
 extraInitFunc_t extraInit) {
	g_usbLockLevel = 0;
	g_pUsbDev = usbDev;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_USB_OTG_FS_CLK_ENABLE();

	//now the lib starts
	//the maximum data size is sizeof(g_usbBuffer) - 8 byte
	usbd_init(g_pUsbDev, &usbd_hw, USB_MAX_PACKET_SIZE, g_usbBuffer, sizeof(g_usbBuffer));
	if (configCallback) {
		usbd_reg_config(g_pUsbDev, configCallback);
	}
	if (controlCallback) {
		usbd_reg_control(g_pUsbDev, controlCallback);
	}
	if (descriptorCallback) {
		usbd_reg_descr(g_pUsbDev, descriptorCallback);
	}
	if (extraInit) {
		extraInit(g_pUsbDev);
	}

	usbd_enable(g_pUsbDev, true);
	uint32_t laneState = usbd_connect(g_pUsbDev, true);
	HAL_NVIC_SetPriority(OTG_FS_IRQn, 12, 0);
	NVIC_EnableIRQ(OTG_FS_IRQn);
	return laneState;
}

void UsbLock(void) {
	NVIC_DisableIRQ(OTG_FS_IRQn);
	g_usbLockLevel++;
}

void UsbUnlock(void) {
	if (g_usbLockLevel) {
		g_usbLockLevel--;
	}
	if (g_usbLockLevel == 0) {
		NVIC_EnableIRQ(OTG_FS_IRQn);
	}
}

void UsbStop(void) {
	if (g_pUsbDev) {
		usbd_connect(g_pUsbDev, false);
		usbd_enable(g_pUsbDev, false);
		HAL_Delay(10); //let the USB process disconnection interrupts
		NVIC_DisableIRQ(OTG_FS_IRQn);
		__HAL_RCC_USB_OTG_FS_CLK_DISABLE();
		g_pUsbDev = NULL;
	}
}

static USB_OTG_GlobalTypeDef * const OTG = (void*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_GLOBAL_BASE);

void UsbRxLvlIsrDisable(void) {
	OTG->GINTMSK &= ~USB_OTG_GINTMSK_RXFLVLM;
	OTG->GINTSTS = USB_OTG_GINTSTS_RXFLVL;
}

void UsbRxLvlIsrEnable(void) {
	OTG->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
}
