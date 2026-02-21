#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//Returns true if starting was successful
bool PlaybackStart(const char * filename, float volume);

//Call within a short timeframe. Retunrs true if there was actually anything to do.
bool PlaybackProcess(void);

//Stops the playback. Returns true if successful
bool PlaybackStop(void);

/*Can be called anytime to adjust the volume, however there will be some delay in the
  resulting volume until the FIFO is processed.
  scale = 1.0 -> original file volume, 0 = muted, 2 -> more volume. Values above 1.0 might
  result in clipping.
*/
void PlaybackVolume(float scale);

void PlaybackEvaluatePerformance(void);

void * mallocIncept(size_t size);

void * callocIncept(size_t nmem, size_t size);

void freeIncept(void * ptr);

void abortIncept(void);
