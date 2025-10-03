#ifndef UTILITIES_HWTEST_H
#define UTILITIES_HWTEST_H

#include <stdint.h>
#include <stdbool.h>

typedef enum 
{
    HWTEST_PASS = 0,
    HWTEST_UNUSED = 1,
    HWTEST_NO_DETECT = 2,
} hwtest_t;

#endif