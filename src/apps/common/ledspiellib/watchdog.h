#pragma once

#include <stdint.h>

/*Starts the hardware watchdog with the timeout in [ms].
  Returns the actual timeout configured (larger than the input parameter).
  Or 0 if the timeout could not be met.
*/
uint32_t WatchdogStart(uint32_t timeout);

//Once started, call regularly
void WatchdogServe(void);
