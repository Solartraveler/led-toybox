#pragma once

#include <stdbool.h>
#include <stdint.h>

/*pData needs to stay valid even after returning of this function.
  Returns true if the data provided are valid.
*/
bool AnimationStartRam(const uint8_t * pData, size_t len, bool repeat);

/*The file needs to stay valid even after returning of this function.
  The filename pointer needs to to stay valid.
  Returns true if the data provided are valid.
*/
bool AnimationStartFile(const char * filename, bool repeat);

/*Returns true if the animation continues. If repeat was true, it usually never
  returns false.
*/
bool AnimationProcess(void);

/*Stops the animation and disables the output matrix. Any files or pData arrays
  can now be made invalid.
*/
void AnimationStop(void);
