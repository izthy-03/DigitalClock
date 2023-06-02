/*
 * Digital Clock
 *
 *
 *
 */

#include "initialize.h"

typedef uint8_t byte;

/* Digital clock type */
typedef struct
{
    int sec;      /* second, range 0~59 */
    int min;      /* minute, range 0~59 */
    int hour;     /* hour, range 0~23*/
    int mday;     /* which day in this month, range 1~31 */
    int month;    /* month range 0~11 */
    int year;     /* year */
    int yday;     /* which day in this year, 0~365 */
    int isleap;   /* whether leap year or not */
    int days[12]; /* built-in calendar */
} dgtclock_t;

/* Alarm type*/
typedef struct
{
    int sec;
    int min;
    int hour;
    bool enable;
} alarm_t;

/* Countdown type */
typedef struct
{
    int millisec;
    int sec;
    int min;
    bool enable;
} timer_t;

void test(void);

/* Clock functions */
void clock_init(dgtclock_t *clock, int ss, int mm, int hh, int mday, int month, int year);
void clock_update(dgtclock_t *clock);
int clock_set_date(dgtclock_t *clock, int mday, int month, int year);
int clock_set_time(dgtclock_t *clock, int sec, int min, int hour);
void clock_get_date(dgtclock_t *clock, char *buf);
void clock_get_time(dgtclock_t *clock, char *buf);
void clock_display_date(dgtclock_t *clock);
void clock_display_time(dgtclock_t *clock);

/* Alarm functions */
void alarm_init(alarm_t *alarm);
int alarm_set(alarm_t *alarm, int sec, int min, int hour);
void alarm_get(alarm_t *alarm, char *buf);

/* Timer functions */
void timer_init(timer_t *timer);

/* Util functions */
int lowbit(int x);

/* Global display mode
 * 0 - time
 * 1 - date
 * 2 - alarm
 * 3 - countdown
 */
int mode = 0;

/* Define systick software counter */
volatile uint16_t systick_10ms_counter, systick_100ms_counter, systick_1000ms_counter;
volatile uint8_t systick_10ms_status, systick_100ms_status;

extern uint8_t seg7[];

int main(void)
{
    dgtclock_t clock;
    alarm_t alarm;
    timer_t timer;
    char buf[128];

    IO_initialize();
    test();
    clock_init(&clock, 59, 00, 8, 11, 5, 2023);
    alarm_init(&alarm);
    timer_init(&timer);

    clock_get_date(&clock, buf);
    UARTStringPut((byte *)buf);
    while (1)
    {
        clock_display_date(&clock);
        Delay(1000);
    }
}

void test()
{
    char buf[64] = "0123456789\r\n";
    UARTStringPut((byte *)buf);
    UARTStringPut((byte *)buf);
    UARTStringPut((byte *)buf);
}

/* Clock methods */
void clock_init(dgtclock_t *clock, int ss, int mm, int hh,
                int mday, int month, int year)
{
    int i, tmp[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    clock->sec = ss;
    clock->min = mm;
    clock->hour = hh;
    clock->mday = mday;
    clock->yday = 0;
    clock->month = month;
    clock->year = year;
    clock->isleap = (year % 100 && year % 4 == 0) || (year % 400 == 0);
    /* Set built-in calendar */
    for (i = 0; i <= MONTH_DEC; i++)
        clock->days[i] = tmp[i];
    clock->days[MONTH_FEB] += clock->isleap;
    /* Calculate yday */
    for (i = 0; i < clock->month; i++)
        clock->yday += clock->days[i];
    clock->yday += clock->mday - 1;
    clock_update(clock);
}
/* Update the clock, handle the carry */
void clock_update(dgtclock_t *clock)
{
    if (clock->sec >= 60)
    {
        clock->min += clock->sec / 60;
        clock->sec %= 60;
    }
    if (clock->min >= 60)
    {
        clock->hour += clock->min / 60;
        clock->min %= 60;
    }
    if (clock->hour >= 24)
    {
        clock->yday += clock->hour / 24;
        clock->mday += clock->hour / 24;
        clock->hour %= 24;
    }
    if (clock->yday >= 365 + clock->isleap)
    {
        clock->year += 1;
        clock->yday = 0;
        clock->mday = 1;
        clock->month = 0;
        clock->isleap = (clock->year % 100 && clock->year % 4 == 0) ||
                        (clock->year % 400 == 0);
        clock->days[MONTH_FEB] = 28 + clock->isleap;
    }
    if (clock->mday > clock->days[clock->month])
    {
        clock->mday = 1;
        clock->month += 1;
    }
}
/* Set clock date
 *  0 - set ok
 * -1 - date error
 */
int clock_set_date(dgtclock_t *clock, int mday, int month, int year)
{
    int leap, days;
    if (year < 0 || month > MONTH_DEC || month < 0)
        return -1;
    leap = (year % 100 && year % 4 == 0) || (year % 400 == 0);
    days = (month == MONTH_FEB) ? 28 + leap : clock->days[month];
    if (mday < 1 || mday > days)
        return -1;
    clock_init(clock, clock->sec, clock->min, clock->hour, mday, month, year);
    return 0;
}
/* Set clock time
 *  0 - set ok
 * -1 - time error
 */
int clock_set_time(dgtclock_t *clock, int sec, int min, int hour)
{
    if (sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23)
        return -1;
    clock->sec = sec;
    clock->min = min;
    clock->hour = hour;
    return 0;
}
/* Get clock date */
void clock_get_date(dgtclock_t *clock, char *buf)
{
    sprintf(buf, "Date %d-%02d-%02d\r\n", clock->year, clock->month + 1, clock->mday);
}
/* Get clock time */
void clock_get_time(dgtclock_t *clock, char *buf)
{
    sprintf(buf, "Time %02d:%02d:%02d\r\n", clock->hour, clock->min, clock->sec);
}
/* Display date */
void clock_display_date(dgtclock_t *clock)
{
    int i, bits[8];
    uint8_t mask = 0x80, dot_seg = 0x00;
    bits[0] = clock->mday % 10, bits[1] = clock->mday / 10;
    bits[2] = (clock->month + 1) % 10, bits[3] = (clock->month + 1) / 10;
    bits[4] = clock->year % 10, bits[5] = clock->year / 10 % 10;
    bits[6] = clock->year / 100 % 10, bits[7] = clock->year / 1000;
    for (i = 0; i < 8; i++, mask >>= 1)
    {
        dot_seg = (i == 2 || i == 4) ? 0x80 : 0x00;

        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[bits[i]] | dot_seg);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask);
        Delay(2000);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        // Delay(1000);
    }
}
/* Display time */
void clock_display_time(dgtclock_t *clock)
{
}

/* Alarm methods */
void alarm_init(alarm_t *alarm)
{
    alarm->enable = false;
    alarm->hour = 8;
    alarm->min = 2;
    alarm->sec = 0;
}
/* Set alarm time
 *  0 - set ok
 * -1 - time error
 */
int alarm_set(alarm_t *alarm, int sec, int min, int hour)
{
    if (sec < 0 || sec > 59 || min < 0 || min > 59 || hour < 0 || hour > 23)
        return -1;
    alarm->sec = sec;
    alarm->min = min;
    alarm->hour = hour;
    return 0;
}
/* Get alarm time */
void alarm_get(alarm_t *alarm, char *buf)
{
    sprintf(buf, "Alarm %02d:%02d:%02d\r\nEnabled: ", alarm->hour, alarm->min, alarm->sec);
    strcat(buf, alarm->enable ? "True\r\n" : "False\r\n");
}

/* Timer methods */
void timer_init(timer_t *timer)
{
    timer->enable = false;
    timer->millisec = 233;
    timer->sec = 13;
    timer->min = 0;
}

/*
    Corresponding to the startup_TM4C129.s vector table systick interrupt program name
*/
void SysTick_Handler(void)
{
    if (systick_100ms_counter != 0)
        systick_100ms_counter--;
    else
    {
        systick_100ms_counter = SYSTICK_FREQUENCY / 10;
        systick_100ms_status = 1;
    }

    if (systick_10ms_counter != 0)
        systick_10ms_counter--;
    else
    {
        systick_10ms_counter = SYSTICK_FREQUENCY / 100;
        systick_10ms_status = 1;
    }
    while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0) == 0)
    {
        systick_100ms_status = systick_10ms_status = 0;
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
    }
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);
}

void UART_response(char *receive)
{
    UARTStringPut((byte *)receive);
}
/*
    Corresponding to the startup_TM4C129.s vector table UART0_Handler interrupt program name
*/
void UART0_Handler(void)
{
    char buf[64];
    int cnt = 0;
    int32_t uart0_int_status;
    uart0_int_status = UARTIntStatus(UART0_BASE, true); // Get the interrrupt status.

    UARTIntClear(UART0_BASE, uart0_int_status); // Clear the asserted interrupts

    while (UARTCharsAvail(UART0_BASE)) // Loop while there are characters in the receive FIFO.
    {
        /// Read the next character from the UART and write it back to the UART.
        // UARTCharPutNonBlocking(UART0_BASE, UARTCharGetNonBlocking(UART0_BASE));
        // GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
        // Delay(1000);
        buf[cnt++] = UARTCharGetNonBlocking(UART0_BASE);
    }
    buf[cnt] = '\0', cnt = 0;
    UART_response(buf);

    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);

    while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0)
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);

    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
}

/* Return the lowest bit, start from 0 */
int lowbit(int x)
{
    int k = 0;
    for (; !(x & 1); k++)
        x >>= 1;
    return k;
}
