#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hw_memmap.h"
#include "debug.h"
#include "gpio.h"
#include "hw_i2c.h"
#include "hw_types.h"
#include "i2c.h"
#include "pin_map.h"
#include "sysctl.h"
#include "systick.h"
#include "interrupt.h"
#include "uart.h"
#include "hw_ints.h"

#define SYSTICK_FREQUENCY 1000 // 1000hz

#define I2C_FLASHTIME 500  // 500mS
#define GPIO_FLASHTIME 300 // 300mS
//*****************************************************************************
//
// I2C GPIO chip address and resigster define
//
//*****************************************************************************
#define TCA6424_I2CADDR 0x22
#define PCA9557_I2CADDR 0x18

#define PCA9557_INPUT 0x00
#define PCA9557_OUTPUT 0x01
#define PCA9557_POLINVERT 0x02
#define PCA9557_CONFIG 0x03

#define TCA6424_CONFIG_PORT0 0x0c
#define TCA6424_CONFIG_PORT1 0x0d
#define TCA6424_CONFIG_PORT2 0x0e

#define TCA6424_INPUT_PORT0 0x00
#define TCA6424_INPUT_PORT1 0x01
#define TCA6424_INPUT_PORT2 0x02

#define TCA6424_OUTPUT_PORT0 0x04
#define TCA6424_OUTPUT_PORT1 0x05
#define TCA6424_OUTPUT_PORT2 0x06

#define IS_BLANK(s) (*(s) == ' ' || *(s) == '\t')
#define IS_END(s) (*(s) == '\0')
#define SKIP_BLANK(s)                     \
    do                                    \
    {                                     \
        while (!IS_END(s) && IS_BLANK(s)) \
            (s)++;                        \
    } while (0);

#define SKIP_CHAR(s)                       \
    do                                     \
    {                                      \
        while (!IS_END(s) && !isdigit(*s)) \
            (s)++;                         \
    } while (0);

#define MONTH_JAN 0
#define MONTH_FEB 1
#define MONTH_MAR 2
#define MONTH_APR 3
#define MONTH_MAY 4
#define MONTH_JUN 5
#define MONTH_JUL 6
#define MONTH_AUG 7
#define MONTH_SEP 8
#define MONTH_OCT 9
#define MONTH_NOV 10
#define MONTH_DEC 11
