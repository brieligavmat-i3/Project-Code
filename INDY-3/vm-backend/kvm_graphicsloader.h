#pragma once

#include <stdbool.h>

#define FILENAME_LENGTH 256

/*
* Loads graphics files into memory.
* If the requested binary files exist already, they are used. Otherwise, new binary files are made.
* Return values: the binary filename for success, NULL for failure.
* Make sure to free the returned string after you're done using it.
*/
char* load_graphics(char* filename, bool is_palette);