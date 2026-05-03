#pragma once

//Before using any of the functions, AdcInit needs to be called once

//returns [mV]
uint16_t AdcBatteryVoltageGet(void);

//returns the brightness. The value has no unit and is not linear.
uint32_t AdcLightLevelGet(void);

//returns [m°C]
int32_t AdcTemperatureGet(void);

//returns [mV]
uint16_t AdcAvrefGet(void);

//returns [mV]
uint16_t AdcAvccGet(void);
