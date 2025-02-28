#pragma once

#include "leakcheck.h"

#define malloc(x) leakcheck_malloc(x)
#define realloc(x, y) leakcheck_realloc(x, y)
#define free(x) leakcheck_free(x)

/*
#ifndef main
    #define main(x, y) leakcheck_main(x, y)
#endif
*/

