#pragma once

//Call at first.
void StorageInit(void);

/* Call to process the storage. The SD card should be initialized before.
  If input is not 0, process the debug work:
  'u': Toggle USB state
  'w': Toggle writeprotect
  'p': Toggle print benchmark
  returns true: a read or write transfer has been done
*/
bool StorageCycle(char input);


void StorageStart(void);

void StorageStop(void);

