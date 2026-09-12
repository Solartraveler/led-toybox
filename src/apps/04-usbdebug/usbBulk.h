#pragma once

//Call at first.
void UsbBulkInit(void);

/* Call to process the storage. The SD card should be initialized before.
  If input is not 0, process the debug work:
  'u': Toggle USB state
  'p': Toggle print benchmark
  returns true: a read or write transfer has been done
*/
bool UsbBulkCycle(char input);

//Connects the USB again after UsbDebugStop has been called
void UsbBulkStart(void);

//Disconnects the USB
void UsbBulkStop(void);

void UsbBulkQueue(const uint8_t * data, size_t len);

