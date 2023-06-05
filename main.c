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

typedef struct
{
    int notelen;
    int *notes;
    int *notetime;
} music_t;

/* Global display mode
 * 0 - time
 * 1 - date
 * 2 - alarm
 * 3 - countdown
 * */
volatile int global_display_mode = 0;

/* Global modification mode
 * 0 - display mode
 * 1 - modification mode  */
volatile int global_modify_mode = 0;

/* Global modification pointer
 *   0 - undefined
 * 1~3 - correspond to respective position */
volatile int global_modify_ptr = 0;

/* Global blink mask vector*/
volatile uint8_t global_blink_mask = 0xff;

/* Global button events */
volatile int BUTTON_EVENT_TOGGLE, BUTTON_EVENT_MODIFY, BUTTON_EVENT_CONFIRM;
volatile int BUTTON_EVENT_ADD, BUTTON_EVENT_DEC, BUTTON_EVENT_ENABLE;
volatile int BUTTON_EVENT_USR0_PRESSED;

/* Define systick software counter */
volatile uint16_t blink_500ms_counter, systick_500ms_counter, systick_1000ms_counter;
volatile uint8_t systick_10ms_status, systick_500ms_status, systick_1000ms_status;

volatile int global_timeout, buzzer_last_time;
extern uint8_t seg7[40];
extern uint32_t ui32SysClock;

/* Function prototypes */

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
void alarm_go_off(alarm_t *alarm, dgtclock_t *clock);

/* Timer functions */
void timer_init(timer_t *timer);
void timer_update(timer_t *timer);
void timer_display(timer_t *timer);
void timer_go_off(timer_t *timer);

/* Serial command functions */
int parse_command(char *cmd, int *argc, char *argv[]);
int execute_command(int argc, char *argv[]);

/*  Event functions */
void events_catch(void);
void events_clear(void);
void start_up(void);

/* Buzzer functions */
void buzzer_on(int freq, int time_ms);

/* Util functions */
void test(void);
int lowbit(int x);
bool istriggered(uint8_t keyval, uint8_t preval, int eventid);
int get_format_nums(char *buf, int *x, int *y, int *z);
void delay_ms(int ms);
void update_blink_mask(uint8_t *mask, int ptr);
void modify_value(int *target, int incr, int bound);
void led_show_info(void);

/* Create clock_t, alarm_t, timer_t instance */
dgtclock_t clock;
alarm_t alarm;
timer_t timer;
/* Create C-major pitch instance */
pitch_t C_major;
/* Create music instances */

int main(void)
{
    char buf[MAXLINE];
    int incr = 0, tmp;

    IO_initialize();
    test();
    sprintf(buf, "SystemClock = %d\n", ui32SysClock);
    UARTStringPut((byte *)buf);
    clock_init(&clock, 59, 00, 8, 11, 5, 2023);
    alarm_init(&alarm);
    timer_init(&timer);

    start_up();
    clock_get_date(&clock, buf);
    UARTStringPut((byte *)buf);

    while (1)
    {
        /* Catch button events */
        events_catch();
        led_show_info();
        alarm_go_off(&alarm, &clock);
        /* Handle button events */
        if (BUTTON_EVENT_TOGGLE)
        {
            if (!global_modify_mode)
                global_display_mode = (global_display_mode + 1) % 4;
            global_modify_mode = 0;
            global_modify_ptr = 0;
            update_blink_mask((uint8_t *)&global_blink_mask, global_modify_ptr);
            continue;
        }
        if (BUTTON_EVENT_CONFIRM)
        {
            if (global_modify_mode)
            {
                global_modify_ptr++;
                update_blink_mask((uint8_t *)&global_blink_mask, global_modify_ptr);
                if (global_modify_ptr > 3)
                {
                    global_modify_mode = 0,
                    global_modify_ptr = 0;
                    continue;
                }
            }
            BUTTON_EVENT_CONFIRM = 0;
        }
        if (BUTTON_EVENT_MODIFY)
        {
            if (!global_modify_mode)
                global_modify_mode = 1,
                global_modify_ptr = 1,
                update_blink_mask((uint8_t *)&global_blink_mask, global_modify_ptr);
            continue;
        }
        if (BUTTON_EVENT_ADD)
            incr = 1;
        else if (BUTTON_EVENT_DEC)
            incr = -1;

        switch (global_display_mode)
        {
        /* Time mode */
        case 0:
            clock_display_time(&clock);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                {
                    switch (global_modify_ptr)
                    {
                    case 1:
                        modify_value(&clock.hour, incr, 24);
                        break;
                    case 2:
                        modify_value(&clock.min, incr, 60);
                        break;
                    case 3:
                        modify_value(&clock.sec, incr, 60);
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        /* Date mode */
        case 1:
            clock_display_date(&clock);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                {
                    switch (global_modify_ptr)
                    {
                    case 1:
                        modify_value(&clock.year, incr, 10000);
                        break;
                    case 2:
                        modify_value(&clock.month, incr, 12);
                        break;
                    case 3:
                        tmp = clock.mday - 1;
                        modify_value(&tmp, incr, clock.days[clock.month]);
                        clock.mday = tmp + 1;
                        break;
                    default:
                        break;
                    }
                    /* Update leap year */
                    clock_init(&clock, clock.sec, clock.min, clock.hour,
                               clock.mday, clock.month, clock.year);
                }
            }
            break;

        /* Alarm mode */
        case 2:
            alarm_display(&alarm);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                {
                    switch (global_modify_ptr)
                    {
                    case 1:
                        modify_value(&alarm.hour, incr, 24);
                        break;
                    case 2:
                        modify_value(&alarm.min, incr, 60);
                        break;
                    case 3:
                        modify_value(&alarm.sec, incr, 60);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (BUTTON_EVENT_ENABLE)
            {
                alarm.enable = !alarm.enable;
                sprintf(buf, "%s\n",
                        alarm.enable ? "Alarm is enabled now" : "Alarm is disabled");
                UARTStringPut((byte *)buf);
            }
            break;

        /* Countdown mode */
        case 3:
            timer_display(&timer);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                {
                    switch (global_modify_ptr)
                    {
                    case 1:
                        modify_value(&timer.min, incr, 60);
                        break;
                    case 2:
                        modify_value(&timer.sec, incr, 60);
                        break;
                    case 3:
                        modify_value(&timer.millisec, incr * 10, 1000);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (BUTTON_EVENT_ENABLE)
            {
                global_modify_mode = global_modify_ptr = 0;
                timer.enable = !timer.enable;
                sprintf(buf, "%s\n",
                        timer.enable ? " Start countdown" : "Pause countdown");
                UARTStringPut((byte *)buf);
            }
            break;

        default:
            break;
        }
        events_clear();
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

void start_up()
{
    pitch_t notes[14] = {C4, D4, E4, F4, G4, A4, B4, C5, D5, E5, F5, G5, A5, B5};
    int note_time[14] = {400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400};
    int student_id[8] = {2, 1, 9, 1, 1, 1, 0, 1};
    int i;
    uint8_t mask = 0x80;
    for (i = 0; i < 7; i++)
        buzzer_on(notes[i], 100);

    global_modify_mode = global_modify_ptr = 1;
    global_timeout = 3000;
    while (global_timeout)
    {
        for (i = 0, mask = 0x80; i < 8; i++, mask >>= 1)
        {
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[student_id[8 - i - 1]]));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask);
            Delay(1000);
            if (global_blink_mask != 0xff)
                I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0x00);
            else
                I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0xff);
        }
    }
    I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0xff);
    global_modify_mode = global_modify_ptr = 0;
}

void events_catch()
{
    static uint8_t now_val = 0xff, prev_val = 0xff;
    static int prev_pin0 = 1, now_pin0;
    events_clear();
    /* Handle button events on blue panel */
    now_val = I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
    if (now_val != prev_val)
    {
        delay_ms(20);
        now_val = I2C0_ReadByte(TCA6424_I2CADDR, TCA6424_INPUT_PORT0);
        BUTTON_EVENT_TOGGLE = istriggered(now_val, prev_val, BUTTON_ID_TOGGLE);
        BUTTON_EVENT_MODIFY = istriggered(now_val, prev_val, BUTTON_ID_MODIFY);
        BUTTON_EVENT_CONFIRM = istriggered(now_val, prev_val, BUTTON_ID_CONFIRM);
        BUTTON_EVENT_ADD = istriggered(now_val, prev_val, BUTTON_ID_ADD);
        BUTTON_EVENT_DEC = istriggered(now_val, prev_val, BUTTON_ID_DEC);
        BUTTON_EVENT_ENABLE = istriggered(now_val, prev_val, BUTTON_ID_ENABLE);
        prev_val = now_val;
    }
    /* Handle button events on red panel */
    now_pin0 = GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_0);
    if (now_pin0 != prev_pin0)
    {
        delay_ms(5);
        if (!now_pin0)
            BUTTON_EVENT_USR0_PRESSED = 1;
        else
            BUTTON_EVENT_USR0_PRESSED = 0;
        prev_pin0 = now_pin0;
    }
}
void events_clear()
{
    BUTTON_EVENT_TOGGLE = 0;
    BUTTON_EVENT_MODIFY = 0;
    BUTTON_EVENT_CONFIRM = 0;
    BUTTON_EVENT_ADD = 0;
    BUTTON_EVENT_DEC = 0;
    BUTTON_EVENT_ENABLE = 0;
}

/* Turn on the buzzer. Rest when freq = 0 */
void buzzer_on(int freq, int time_ms)
{
    if (freq != 0)
    {
        PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, ui32SysClock / freq);
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, ui32SysClock / freq / 100);
        PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, true);
    }
    buzzer_last_time = time_ms;
    while (buzzer_last_time)
        ;
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
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
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & global_blink_mask);
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
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & global_blink_mask);
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
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & (global_blink_mask | 0x03));
        Delay(1000);
    }
}
void alarm_go_off(alarm_t *alarm, dgtclock_t *clock)
{
    pitch_t notes[7] = {C4, D4, E4, F4, G4, A4, B4};
    int i = 6;
    if (!alarm->enable || alarm->hour != clock->hour ||
        alarm->min != clock->min || alarm->sec != clock->sec)
        return;
    global_display_mode = 2;
    global_modify_mode = global_modify_ptr = 0;
    global_timeout = 5000;
    while (global_timeout && !BUTTON_EVENT_ENABLE)
    {
        events_catch();
        if (systick_500ms_status)
            alarm_display(alarm);
        else
            I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0x00);
        buzzer_on(notes[i], 100);
        i = (i - 1 + 7) % 7;
    }
    events_clear();
    global_timeout = 0;
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
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
        I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & (global_blink_mask | 0x03));
        Delay(1000);
    }
}

/*
    Corresponding to the startup_TM4C129.s vector table systick interrupt program name
*/
void SysTick_Handler(void)
{
    static uint32_t timestamp = 0, duration = 0;
    char buf[MAXLINE];
    timestamp++;

    /* Handle button counter on red panel */
    if (BUTTON_EVENT_USR0_PRESSED)
    {
        if (!duration)
        {
            sprintf(buf, "USR0_BUTTON pressed at %d.%03ds\n", timestamp / 1000, timestamp % 1000);
            UARTStringPut((byte *)buf);
        }
        duration++;
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
    }
    else
    {
        if (duration)
        {
            sprintf(buf, "USR0_BUTTON released at %d.%03ds\nDuration time: %d.%03ds\n\n",
                    timestamp / 1000, timestamp % 1000,
                    duration / 1000, duration % 1000);
            UARTStringPut((byte *)buf);
        }
        duration = 0;
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);
    }

    /* Whether run countdown */
    if (timer.enable)
    {
        if (!global_modify_mode || global_display_mode != 3)
            timer.millisec--;
        timer_update(&timer);
    }

    /* Handle timeout */
    if (global_timeout != 0)
        global_timeout--;
    if (buzzer_last_time != 0)
        buzzer_last_time--;

    if (systick_1000ms_counter != 0)
    {
        systick_1000ms_counter--;
    }
    else
    {
        systick_1000ms_counter = SYSTICK_FREQUENCY;
        systick_1000ms_status = 1;
        if (!global_modify_mode || global_display_mode != 0)
            clock.sec++;
        clock_update(&clock);
    }

    if (systick_500ms_counter != 0)
    {
        systick_500ms_counter--;
    }
    else
    {
        systick_500ms_counter = 500;
        systick_500ms_status ^= 1;
    }
    /* Update blink count */
    if (blink_500ms_counter != 0)
    {
        blink_500ms_counter--;
    }
    else
    {
        blink_500ms_counter = 500;
        update_blink_mask((uint8_t *)&global_blink_mask, global_modify_ptr);
    }
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
            argv[k][len] = '\0';
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
                global_display_mode = 0;
                UARTStringPut("Clock time set successfully\n");
            }
            else if (!strcasecmp(argv[1], "date"))
            {
                if (clock_set_date(&clock, zz, yy - 1, xx) != 0)
                {
                    UARTStringPut("Invalid date\n");
                    return -1;
                }
                global_display_mode = 1;
                UARTStringPut("Clock date set successfully\n");
            }
            else if (!strcasecmp(argv[1], "alarm"))
            {
                if (alarm_set(&alarm, zz, yy, xx) != 0)
                {
                    UARTStringPut("Invalid alarm time\n");
                    return -1;
                }
                // global_display_mode = 2;
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
                global_display_mode = 0;
                UARTStringPut("Display clock time\n");
            }
            else if (!strcasecmp(argv[1], "date"))
            {
                global_display_mode = 1;
                UARTStringPut("Display clock date\n");
            }
            else if (!strcasecmp(argv[1], "cdown"))
            {
                global_display_mode = 3;
                timer_enable(&timer);
                // UARTStringPut("Display and start countdown\n");
            }
            global_modify_mode = 0;
            global_modify_ptr = 0;
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
                UARTStringPut("Pause countdown\n");
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
    static char *argv[16] = {NULL}; // max 16 parameters
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
            delay_ms(5);
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
    //     UARTStringPut("\n");
    // }
    for (i = 0; i < argc; i++)
        free(argv[i]);

    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
    // while (GPIOPinRead(GPIO_PORTJ_BASE, GPIO_PIN_1) == 0)
    //     GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, GPIO_PIN_1);
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

bool istriggered(uint8_t keyval, uint8_t preval, int eventid)
{
    return !!((~keyval) & (1 << eventid)) && !((~preval) & (1 << eventid));
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

void delay_ms(int ms)
{
    global_timeout = ms;
    while (global_timeout)
        ;
}

void update_blink_mask(uint8_t *mask, int ptr)
{
    uint8_t tmp = *mask;
    switch (ptr)
    {
    case 0:
        tmp = 0xff;
        break;
    case 1:
        tmp ^= 0x0f;
        tmp |= ~0x0f;
        break;
    case 2:
        tmp ^= 0x30;
        tmp |= ~0x30;
        break;
    case 3:
        tmp ^= 0xc0;
        tmp |= ~0xc0;
        break;
    default:
        tmp = 0xff;
    }
    *mask = tmp;
}
void modify_value(int *target, int incr, int bound)
{
    int tmp = *target;
    tmp = (tmp + incr + bound) % bound;
    *target = tmp;
}

void led_show_info()
{
    uint8_t leds = 0x00;
    leds |= ((uint8_t)0x01 << global_display_mode) &
            (global_blink_mask == 0xff ? 0xff : 0x00);
    leds |= alarm.enable ? 0x40 : 0x00;
    leds |= (timer.enable && systick_500ms_status ? 0x80 : 0x00);
    I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, ~leds);
}
