#ifndef _HEADERS_H
#define _HEADERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
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
#include "pwm.h"
#include "hibernate.h"

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

#define IS_BLANK(s) (*(s) == ' ' || *(s) == '\t' || *(s) == '\r' || *(s) == '\n')
#define IS_END(s) (*(s) == '\0' || *(s) == '\r' || *(s) == '\n')
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

#define MAXLINE 256
#define MAXARGS 16

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

/* Button ids corresponding to respective button events */
#define BUTTON_ID_TOGGLE 0
#define BUTTON_ID_MODIFY 1
#define BUTTON_ID_CONFIRM 1
#define BUTTON_ID_ADD 6
#define BUTTON_ID_DEC 7
#define BUTTON_ID_ENABLE 4
#define BUTTON_ID_FLIP 3

/* Define Hibernation data storage index */
#define HBN_VERIFY 0
#define HBN_RTC 1
#define HBN_CLOCK 2
#define HBN_ALARM 8
#define HBN_TIMER 11

/* Define inner timer id */
#define INNERTIMER_GENERAL 0
#define INNERTIMER_BUZZER 1
#define INNERTIMER_ALARM 2
#define INNERTIMER_TIMER 3

/* Define verify code */
#define HBN_CODE_VERIFY 21911101

/* Pitch frequecy(Hz) */
typedef enum
{
    Re = 0,
    C4 = 261,
    D4 = 294,
    E4 = 330,
    F4 = 349,
    G4 = 392,
    A4 = 440,
    B4 = 494,

    C5 = 523,
    D5 = 587,
    E5 = 659,
    F5 = 698,
    G5 = 784,
    A5 = 880,
    B5 = 988
} pitch_t;

#endif
