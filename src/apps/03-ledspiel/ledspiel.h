#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void AppInit(void);

void AppCycle(void);

void * mallocIncept(size_t size);

void * callocIncept(size_t nmem, size_t size);

void freeIncept(void * ptr);

void abortIncept(void);
