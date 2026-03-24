#pragma once

#include <stdbool.h>
#include <stdint.h>

/*Static memory requirements in bytes:
  MATRIX_COLORS_VAL_MAX * 2 + MATRIX_Y * MATRIX_DIM_FACTOR_MAX
  Bus transfers per second:
  (value set by MatrixInit + 1) * MATRIX_Y * (value set by MatrixBrightness()) * MATRIX_REFRESHRATE
*/

#define MATRIX_X 5

#define MATRIX_Y 5


//This represents a 9 bit PWM value
#define MATRIX_COLORS_VAL_MAX 511
#define MATRIX_COLORS_PER_PIXEL 3

//A higher value results in more bus transfers
#define MATRIX_DIM_FACTOR_MAX 8

//In [Hz]
#define MATRIX_REFRESHRATE 100


/*Initializes the LED matrix. colorMax is the maximum value found in the frame data.
  Must be <= MATRIX_COLORS_VAL_MAX and >= 2.
  Example: 8 bit data -> colorMax = 255.
  Returns true if valMax is within the supported range of the buffer.
*/
bool MatrixInit(uint16_t colorMax);

//The matrix data must be of MATRIX_X * MATRIX_Y * bytesPerPixel size.
void MatrixFrame(uint8_t bytesPerPixel, const uint8_t * pFrame);

//Dims the matrix. 255 = maximum brightness, 0 = minimum brighness
void MatrixBrightness(uint8_t value);

//Matrix gets dark. Entering power saving mode.
void MatrixDisable(void);

