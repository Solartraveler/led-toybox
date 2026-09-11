#pragma once

//Its a 12 bit DAC
#define AUIDO_VALUE_MAX 4095


/*Sets up an audio buffer. The playback starts instantly, so pBuffer should
  contain valid audio data.
  samplerate is in [Hz]
*/
bool AudioInit(uint16_t * pBuffer, size_t entries, uint16_t samplerate);

void AudioStop(void);

//Gets the number of elements which can be placed in the buffer
size_t AudioBufferFreeGet(void);

/*Puts audio data into the buffer, check with AudioBufferFreeGet if data can be
  put without an overflow.
*/
void AudioBufferPut(uint16_t data);
