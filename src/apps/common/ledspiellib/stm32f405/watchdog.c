/* Ledspiellib
(c) 2026 by Malte Marwedel

SPDX-License-Identifier: BSD-3-Clause
*/

#include "ledspiellib/watchdog.h"

#include "main.h"

uint32_t WatchdogStart(uint32_t timeout) {
	if ((timeout == 0) || (timeout > 32000)) {
		return 0;
	}
	IWDG->KR = 0xCCCC; //start watchdog
	IWDG->KR = 0x5555; //gain access
	IWDG->PR = 0x6; //divide by 256
	timeout = ((timeout * 32) + 255) / 256;  // 1/32kHz * 256 = ~8ms per RLR value
	IWDG->RLR = timeout;
	while (IWDG->SR);
	WatchdogServe(); //otherwise the maximum possible timeout with RLR = 0xFFF is counted down
	return timeout * 256 / 32;
}

void WatchdogServe(void) {
	IWDG->KR = 0xAAAA;
}
