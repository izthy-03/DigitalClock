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

/* Global display mode
 * 0 - time
 * 1 - date
 * 2 - alarm
 * 3 - countdown
 */
int display_mode = 0;
/* Global modification mode
 * 0 - display mode
 * 1 - modification mode
 */
int modify_mode = 0;
/* Global modification pointer
 *   0 - undefined
 * 1~3 - correspond to respective position
 */
int modify_ptr = 0;

/* Global button events */
int BUTTON_EVENT_TOGGLE, BUTTON_EVENT_MODIFY, BUTTON_EVENT_CONFIRM;
int BUTTON_EVENT_ADD, BUTTON_EVENT_DEC;

/* Define systick software counter */
volatile uint16_t systick_10ms_counter, systick_100ms_counter, systick_1000ms_counter;
volatile uint8_t systick_10ms_status, systick_100ms_status, systick_1000ms_status;

volatile uint16_t UART_timeout;
extern uint8_t seg7[40];

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
void alarm_display(alarm_t *alarm);

/* Timer functions */
void timer_init(timer_t *timer);
void timer_update(timer_t *timer);
void timer_display(timer_t *timer);

/* Command functions */
int parse_command(char *cmd, int *argc, char *argv[]);
int execute_command(int, char **);

/* Util functions */
void test(void);
int lowbit(int x);
int get_format_nums(char *buf, int *x, int *y, int *z);

/* Create clock_t, alarm_t, timer_t instance */
dgtclock_t clock;
alarm_t alarm;
timer_t timer;

int main(void)
{
    char buf[MAXLINE];

    IO_initialize();
    test();
    // clock=(dgtclock_t*)malloc(sizeof(dgtclock_t));
    clock_init(&clock, 59, 00, 8, 11, 5, 2023);
    alarm_init(&alarm);
    timer_init(&timer);

    clock_get_date(&clock, buf);
    UARTStringPut((byte *)buf);

    while (1)
    {
        // clock_display_date(&clock);
        // clock_display_time(&clock);
        // alarm_display(&alarm);
        // timer_display(&timer);
        switch (display_mode)
        {
        /* Time mode */
        case 0:

            break;

        /* Date mode */
        case 1:

            break;

        /* Alarm mode */
        case 2:

            break;

        /* Countdown mode */
        case 3:

            break;

        default:
            break;
        }
        Delay(1000);
    }
}

void test()
{
    char buf[MAXLINE] = "0123456789\r\n";
    UARTStringPut((byte *)buf);
    UARTStringPut((byte *)buf);
    UARTStringPut((byte *)buf);
}

void events_catch()
{
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
 * -1 - invalid date
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
 * -1 - invalid time
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
    sprintf(buf, "Date %d-%02d-%02d\n", clock->year, clock->month + 1, clock->mday);
}
/* Get clock time */
void clock_get_time(dgtclock_t *clock, char *buf)
{
    sprintf(buf, "Time %02d:%02d:%02d\n", clock->hour, clock->min, clock->sec);
}
/* Display date once */
void clock_display_date(dgtclock_t *clock)
{
    /* Format: yyyy.mm.dd */
    int i, bits[8];
    uint8_t mask = 0x80, seg_dot = 0x00;
    bits[0] = clock->mday % 10, bits[1] = clock->mday / 10;
    bits[2] = (clock->month + 1) % 10, bits[3] = (clock->month + 1) / 10;
    bits[4] = clock->year % 10, bits[5] = clock->year / 10 % 10;
    bits[6] = clock->year / 100 % 10, bits[7] = clock->year / 1000;
    for (i = 0; i < 8; i++, mask >>= 1)
    {
        seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[bits[i]] | seg_dot);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask);
        Delay(1000);
    }
}
/* Display time once  */
void clock_display_time(dgtclock_t *clock)
{
    /* Format: hh.mm.ss*/
    int i, bits[6];
    uint8_t mask = 0x80, seg_dot = 0x00;
    bits[0] = clock->sec % 10, bits[1] = clock->sec / 10;
    bits[2] = clock->min % 10, bits[3] = clock->min / 10;
    bits[4] = clock->hour % 10, bits[5] = clock->hour / 10;
    for (i = 0; i < 6; i++, mask >>= 1)
    {
        seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[bits[i]] | seg_dot);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask);
        Delay(1000);
    }
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
    sprintf(buf, "Alarm %02d:%02d:%02d\nEnabled: ", alarm->hour, alarm->min, alarm->sec);
    strcat(buf, alarm->enable ? "True\n" : "False\n");
}
/* Display alarm once */
void alarm_display(alarm_t *alarm)
{
    /* Format: AL xx.yy.zz */
    int i, bits[8];
    uint8_t mask = 0x80, seg_dot = 0x00;
    bits[0] = alarm->sec % 10, bits[1] = alarm->sec / 10;
    bits[2] = alarm->min % 10, bits[3] = alarm->min / 10;
    bits[4] = alarm->hour % 10, bits[5] = alarm->hour / 10;
    bits[6] = 'L' - 'A' + 10, bits[7] = 'A' - 'A' + 10;
    for (i = 0; i < 8; i++, mask >>= 1)
    {
        seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[bits[i]] | seg_dot);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask);
        Delay(1000);
    }
}

/* Timer methods */
void timer_init(timer_t *timer)
{
    timer->enable = false;
    timer->millisec = 233;
    timer->sec = 13;
    timer->min = 0;
}
/* Update timer when counting down */
void timer_update(timer_t *timer)
{
    if (timer->millisec < 0)
    {
        timer->sec--;
        timer->millisec = 999;
    }
    if (timer->sec < 0)
    {
        timer->min--;
        timer->sec = 59;
    }
    /* Time up */
    if (timer->min < 0)
    {
        timer->enable = false;
        timer->millisec = 0;
        timer->sec = 0;
        timer->min = 0;
        UARTStringPut("\n>>> Time is up!\n");
    }
}
/* Wrapped enable function */
void timer_enable(timer_t *timer)
{
    if (timer->enable)
    {
        UARTStringPut("Countdown has already started\n");
    }
    else
    {
        UARTStringPut("Start countdown\n");
        timer->enable = true;
    }
}
/* Display timer once */
void timer_display(timer_t *timer)
{
    /* Format: cd xx.yy.zz*/
    int i, bits[8];
    uint8_t mask = 0x80, seg_dot = 0x00;
    bits[0] = timer->millisec / 10 % 10, bits[1] = timer->millisec / 100;
    bits[2] = timer->sec % 10, bits[3] = timer->sec / 10;
    bits[4] = timer->min % 10, bits[5] = timer->min / 10;
    bits[6] = 'D' - 'A' + 10, bits[7] = 'C' - 'A' + 10;
    for (i = 0; i < 8; i++, mask >>= 1)
    {
        seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, seg7[bits[i]] | seg_dot);
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask);
        Delay(1000);
    }
}

/*
    Corresponding to the startup_TM4C129.s vector table systick interrupt program name
*/
void SysTick_Handler(void)
{
    if (timer.enable)
    {
        timer.millisec--;
        timer_update(&timer);
    }

    if (UART_timeout != 0)
    {
        UART_timeout--;
    }

    if (systick_1000ms_counter != 0)
    {
        systick_1000ms_counter--;
    }
    else
    {
        systick_1000ms_counter = SYSTICK_FREQUENCY;
        systick_1000ms_status = 1;

        clock.sec++;
        clock_update(&clock);
    }

    if (systick_100ms_counter != 0)
    {
        systick_100ms_counter--;
    }
    else
    {
        systick_100ms_counter = SYSTICK_FREQUENCY / 10;
        systick_100ms_status = 1;
    }

    if (systick_10ms_counter != 0)
    {
        systick_10ms_counter--;
    }
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

/* Parse the command string received
 *  0 - parse succeeded
 * -1 - parse failed
 */
int parse_command(char *cmd, int *argc, char *argv[])
{
    char buf[MAXLINE];
    int len = -1, k;
    SKIP_BLANK(cmd);
    while (len && *argc <= MAXARGS)
    {
        k = *argc;
        len = 0;
        while (!IS_BLANK(cmd + len) && !IS_END(cmd + len))
            len++;
        if (len)
        {
            argv[k] = (char *)malloc(len + 5);
            strncpy(argv[k], cmd, len);
            strcat(argv[k], "\0");
            (*argc)++;
            cmd += len;
            SKIP_BLANK(cmd);
        }
    }
    if (*argc > MAXARGS)
    {
        sprintf(buf, "argc = %d\n", *argc);
        UARTStringPut((byte *)buf);
        UARTStringPut("Command parse error: too many arguments\n");
        return -1;
    }
    if (!(*argc))
    {
        return -1;
    }
    return 0;
}
/* Execute the command string received
 *  0 - execution succeeded
 * -1 - execution failed
 */
int execute_command(int argc, char *argv[])
{
    char buf[MAXLINE];
    if (!strcmp(argv[0], "?"))
    {
        UARTStringPut("Available commands:\n");
        UARTStringPut("\tinit clock                       : intialize the clock to 00:00:00\n");
        UARTStringPut("\tget <TIME/DATE/ALARM>            : get status\n");
        UARTStringPut("\tset <TIME/ALARM/DATE> <xx:xx:xx> : set clock status\n");
        UARTStringPut("\trun <TIME/DATE/STWATCH>          : run functions\n");
        return 0;
    }
    /* Execute INIT command */
    else if (!strcasecmp(argv[0], "init"))
    {
        if (argc == 2 && !strcasecmp(argv[1], "clock"))
        {
            clock_init(&clock, 0, 0, 0, 1, 0, 2000);
            UARTStringPut("Clock reset to 2000-1-1-00:00:00\n");
            return 0;
        }
        else
        {
            UARTStringPut("Usage: init clock\n");
            return -1;
        }
    }
    /* Execute GET command */
    else if (!strcasecmp(argv[0], "get"))
    {
        bool valid = (argc == 2) && (!strcasecmp(argv[1], "time") ||
                                     !strcasecmp(argv[1], "date") ||
                                     !strcasecmp(argv[1], "alarm"));
        if (valid)
        {
            if (!strcasecmp(argv[1], "time"))
            {
                clock_get_time(&clock, buf);
                UARTStringPut((byte *)buf);
            }
            else if (!strcasecmp(argv[1], "date"))
            {
                clock_get_date(&clock, buf);
                UARTStringPut((byte *)buf);
            }
            else if (!strcasecmp(argv[1], "alarm"))
            {
                alarm_get(&alarm, buf);
                UARTStringPut((byte *)buf);
            }
            return 0;
        }
        else
        {
            UARTStringPut("Usage: get time  - return clock time\n");
            UARTStringPut("       get date  - return clock date\n");
            UARTStringPut("       get alarm - return alarm status\n");
            return -1;
        }
    }
    /* Execute SET command */
    else if (!strcasecmp(argv[0], "set"))
    {
        int xx, yy, zz;
        bool valid = (argc == 3) && (!strcasecmp(argv[1], "time") ||
                                     !strcasecmp(argv[1], "date") ||
                                     !strcasecmp(argv[1], "alarm"));
        if (valid)
        {
            if (get_format_nums(argv[2], &xx, &yy, &zz) != 0)
            {
                UARTStringPut("Invalid time/date format\n");
                sprintf(buf, "Usage: set %s <%sxx:yy:zz>/<%sxx-yy-zz>\n",
                        argv[1], !strcasecmp(argv[1], "date") ? "xx" : "",
                        !strcasecmp(argv[1], "date") ? "xx" : "");
                UARTStringPut((byte *)buf);
                return -1;
            }
            if (!strcasecmp(argv[1], "time"))
            {
                if (clock_set_time(&clock, zz, yy, xx) != 0)
                {
                    UARTStringPut("Invalid time\n");
                    return -1;
                }
                UARTStringPut("Clock time set successfully\n");
            }
            else if (!strcasecmp(argv[1], "date"))
            {
                if (clock_set_date(&clock, zz, yy - 1, xx) != 0)
                {
                    UARTStringPut("Invalid date\n");
                    return -1;
                }
                UARTStringPut("Clock date set successfully\n");
            }
            else if (!strcasecmp(argv[1], "alarm"))
            {
                if (alarm_set(&alarm, zz, yy, xx) != 0)
                {
                    UARTStringPut("Invalid alarm time\n");
                    return -1;
                }
                UARTStringPut("Alarm time set successfully\n");
            }
            return 0;
        }
        else
        {
            UARTStringPut("Usage: set date <year-month-day>       - set clock date\n");
            UARTStringPut("       set time <hh:mm:ss>/<hh-mm-ss>  - set clock time\n");
            UARTStringPut("       set alarm <hh:mm:ss>/<hh-mm-ss> - set alarm time\n");
            return -1;
        }
    }
    /* Execute RUN command */
    else if (!strcasecmp(argv[0], "run"))
    {
        bool valid = (argc == 2) && (!strcasecmp(argv[1], "time") ||
                                     !strcasecmp(argv[1], "date") ||
                                     !strcasecmp(argv[1], "cdown"));
        if (valid)
        {
            if (!strcasecmp(argv[1], "time"))
            {
                display_mode = 0;
                UARTStringPut("Display clock time\n");
            }
            else if (!strcasecmp(argv[1], "date"))
            {
                display_mode = 1;
                UARTStringPut("Display clock date\n");
            }
            else if (!strcasecmp(argv[1], "cdown"))
            {
                display_mode = 3;
                timer_enable(&timer);
                // UARTStringPut("Display and start countdown\n");
            }
            return 0;
        }
        else
        {
            UARTStringPut("Usage: run date  - display clock date\n");
            UARTStringPut("       run time  - display clock time\n");
            UARTStringPut("       run cdown - display and start timer countdown\n");
            return -1;
        }
    }
    /* Execute ENABLE command */
    else if (!strcasecmp(argv[0], "enable"))
    {
        bool valid = (argc == 2) && (!strcasecmp(argv[1], "alarm") ||
                                     !strcasecmp(argv[1], "cdown"));
        if (valid)
        {
            if (!strcasecmp(argv[1], "alarm"))
            {
                alarm.enable = true;
                UARTStringPut("Alarm is enabled now\n");
            }
            else if (!strcasecmp(argv[1], "cdown"))
            {
                timer_enable(&timer);
                // UARTStringPut("Start countdown\n");
            }
            return 0;
        }
        else
        {
            UARTStringPut("Usage: enable alarm  - enable the alarm to go off\n");
            UARTStringPut("       enable cdown  - start timer countdown\n");
            return -1;
        }
    }
    /* Execute DISABLE command */
    else if (!strcasecmp(argv[0], "disable"))
    {
        bool valid = (argc == 2) && (!strcasecmp(argv[1], "alarm") ||
                                     !strcasecmp(argv[1], "cdown"));
        if (valid)
        {
            if (!strcasecmp(argv[1], "alarm"))
            {
                alarm.enable = false;
                UARTStringPut("Alarm is disabled\n");
            }
            else if (!strcasecmp(argv[1], "cdown"))
            {
                timer.enable = false;
                UARTStringPut("Stop countdown\n");
            }
            return 0;
        }
        else
        {
            UARTStringPut("Usage: disable alarm  - disable the alarm to go off\n");
            UARTStringPut("       disable cdown  - stop timer countdown\n");
            return -1;
        }
    }
    else
    {
        UARTStringPut("Command not found. Type '?' for help\n");
        return -1;
    }
}

/*
    Corresponding to the startup_TM4C129.s vector table UART0_Handler interrupt program name
*/
void UART0_Handler(void)
{
    char buf[MAXLINE], c[1];
    char *argv[16]; // max 16 parameters
    int argc = 0, i;
    int32_t uart0_int_status;

    memset(buf, 0, sizeof(buf));
    uart0_int_status = UARTIntStatus(UART0_BASE, true); // Get the interrrupt status.
    UARTIntClear(UART0_BASE, uart0_int_status);         // Clear the asserted interrupts

    while (UARTCharsAvail(UART0_BASE)) // Loop while there are characters in the receive FIFO.
    {
        /* Read the next character from the UART and write it back to the UART. */
        c[0] = UARTCharGet(UART0_BASE);
        strcat(buf, c);
        if (c[0] == '\r')
            break;
        /* wait for the line to come to the end */
        if (!UARTCharsAvail(UART0_BASE))
        {
            UART_timeout = 5;
            while (UART_timeout)
                ;
        }
    }

    if (parse_command(buf, &argc, argv) != 0)
    {
        for (i = 0; i < argc; i++)
            free(argv[i]);
        return;
    }

    execute_command(argc, argv);

    /* output parsed arguments for test */
    // sprintf(buf, "argc = %d\r\n", argc);
    // UARTStringPut((byte *)buf);
    // for (i = 0; i < argc; i++)
    // {
    //     UARTStringPut((byte *)argv[i]);
    //     UARTStringPut("\r\n");
    // }
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
    while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0)
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
}

/* Util functions */

/* Return the lowest bit, start from 0 */
int lowbit(int x)
{
    int k = 0;
    for (; !(x & 1); k++)
        x >>= 1;
    return k;
}
/* Parse formatted numbers, e.g.2023-6-12, 13:54:00
 * return:  0 - valid format
 *         -1 - invalid format
 */
int get_format_nums(char *buf, int *x, int *y, int *z)
{
    char delim;
    if (strstr(buf, ":"))
        delim = ':';
    else if (strstr(buf, "-"))
        delim = '-';
    else
        return -1;

    *x = strtol(buf, &buf, 0);
    if (*buf != delim || !isdigit(*(buf + 1)))
        return -1;
    SKIP_CHAR(buf);
    *y = strtol(buf, &buf, 0);
    if (*buf != delim || !isdigit(*(buf + 1)))
        return -1;
    SKIP_CHAR(buf);
    *z = strtol(buf, &buf, 0);
    if (!IS_END(buf))
        return -1;
    return 0;
}
