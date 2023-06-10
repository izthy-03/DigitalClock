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
    pitch_t notes[MAXLINE];
    int notetime[MAXLINE];
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

/* Global already indicator */
volatile int global_already = 0;

/* Global flip indicator */
volatile int global_flip = 0;

/* Buzzer enable indicator*/
volatile int buzzer_enable;

/* Global button events */
volatile int BUTTON_EVENT_TOGGLE, BUTTON_EVENT_MODIFY, BUTTON_EVENT_CONFIRM;
volatile int BUTTON_EVENT_ADD, BUTTON_EVENT_DEC, BUTTON_EVENT_ENABLE;
volatile int BUTTON_EVENT_USR0_PRESSED, BUTTON_EVENT_FLIP;

/* Define systick software counter */
volatile uint16_t blink_500ms_counter, systick_500ms_counter, systick_1000ms_counter;
volatile uint8_t systick_10ms_status, systick_500ms_status, systick_1000ms_status;

/* Ten inner timers for use */
volatile int inner_timers[10];

extern uint8_t seg7[40];
extern uint8_t flp7[40];
extern uint32_t ui32SysClock;
extern uint32_t pui32NVData[64];

/* Function prototypes */
/* Inner timer functions*/
void inner_timer_start(int timerId, int timeoutMs);
bool inner_timer_status(int timerId);
void inner_timer_update(void);

/* Hibernation functions */
void hibernation_wakeup_init(dgtclock_t *clock, alarm_t *alarm, timer_t *timer);
void hibernation_data_store(dgtclock_t *clock, alarm_t *alarm, timer_t *timer);

/* Clock methods */
void clock_init(dgtclock_t *clock, int ss, int mm, int hh, int mday, int month, int year);
void clock_update(dgtclock_t *clock);
int clock_set_date(dgtclock_t *clock, int mday, int month, int year);
int clock_set_time(dgtclock_t *clock, int sec, int min, int hour);
void clock_get_date(dgtclock_t *clock, char *buf);
void clock_get_time(dgtclock_t *clock, char *buf);
void clock_display_date(dgtclock_t *clock);
void clock_display_time(dgtclock_t *clock);
void clock_button_increase(dgtclock_t *clock, int incr, int ptr);

/* Alarm methods */
void alarm_init(alarm_t *alarm, int sec, int min, int hour);
int alarm_set(alarm_t *alarm, int sec, int min, int hour);
void alarm_get(alarm_t *alarm, char *buf);
void alarm_display(alarm_t *alarm);
void alarm_go_off(alarm_t *alarm, dgtclock_t *clock);
void alarm_button_increase(alarm_t *alarm, int incr, int ptr);

/* Timer methods */
void timer_init(timer_t *timer, int millisec, int sec, int min);
void timer_update(timer_t *timer);
void timer_display(timer_t *timer);
void timer_go_off(timer_t *timer);
void timer_button_increase(timer_t *timer, int incr, int ptr);

/* Serial command functions */
int parse_command(char *cmd, int *argc, char *argv[]);
int execute_command(int argc, char *argv[]);

/*  Event functions */
void events_catch(void);
void events_clear(void);
void start_up(void);

/* Buzzer functions */
void buzzer_on(int freq, int time_ms);
void buzzer_off(void);
void buzzer_music_nonblocking(int len, pitch_t notes[], int time[], bool set);

/* Util functions */
void test(void);
bool istriggered(uint8_t keyval, uint8_t preval, int eventid);
int get_format_nums(char *buf, int *x, int *y, int *z);
void delay_ms(int ms);
void update_blink_mask(uint8_t *mask, int ptr);
void led_show_info(void);
void print_log(void);

/* Create global clock_t, alarm_t, timer_t instance */
dgtclock_t clock;
alarm_t alarm;
timer_t timer;

int main(void)
{
    char buf[MAXLINE];
    int incr = 0;

    IO_initialize();
    start_up();
    hibernation_wakeup_init(&clock, &alarm, &timer);
    global_already = 1;

    clock_get_date(&clock, buf);
    UARTStringPut((byte *)buf);
    clock_get_time(&clock, buf);
    UARTStringPut((byte *)buf);

    while (1)
    {
        /* Catch button events */
        events_catch();

        led_show_info();
        alarm_go_off(&alarm, &clock);
        timer_go_off(&timer);
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
            {
                global_modify_mode = 1;
                global_modify_ptr = 1;
                update_blink_mask((uint8_t *)&global_blink_mask, global_modify_ptr);
            }
            continue;
        }
        if (BUTTON_EVENT_FLIP)
        {
            global_flip ^= 1;
            update_blink_mask((uint8_t *)&global_blink_mask, global_modify_ptr);
            BUTTON_EVENT_FLIP = 0;
        }

        if (BUTTON_EVENT_ADD)
            incr = 1;
        else if (BUTTON_EVENT_DEC)
            incr = -1;

        /* Display */
        switch (global_display_mode)
        {
        /* Time mode */
        case 0:
            clock_display_time(&clock);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                    clock_button_increase(&clock, incr, global_modify_ptr);
            }
            break;

        /* Date mode */
        case 1:
            clock_display_date(&clock);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                    clock_button_increase(&clock, incr, global_modify_ptr + 3);
            }
            break;

        /* Alarm mode */
        case 2:
            alarm_display(&alarm);
            if (global_modify_mode)
            {
                if (BUTTON_EVENT_ADD || BUTTON_EVENT_DEC)
                    alarm_button_increase(&alarm, incr, global_modify_ptr);
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
                    timer_button_increase(&timer, incr, global_modify_ptr);
            }

            if (BUTTON_EVENT_ENABLE)
            {
                if (0 == timer.millisec + timer.sec + timer.min)
                    break;
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

/* Wake from hibernation. Read data from memory */
void hibernation_wakeup_init(dgtclock_t *clock, alarm_t *alarm, timer_t *timer)
{
    HibernateDataGet(pui32NVData, 16);
    print_log();
    if (pui32NVData[HBN_VERIFY] != HBN_CODE_VERIFY)
    {
        pui32NVData[HBN_VERIFY] = HBN_CODE_VERIFY;
        pui32NVData[HBN_RTC] = HibernateRTCGet();
        clock_init(clock, 59, 00, 8, 11, 5, 2023);
        alarm_init(alarm, 3, 0, 8);
        timer_init(timer, 233, 13, 0);
        HibernateRTCSet(0);
        // hibernation_data_store(clock, alarm, timer);
    }
    else
    {
        memcpy(clock, pui32NVData + HBN_CLOCK, sizeof(int) * 6);
        memcpy(alarm, pui32NVData + HBN_ALARM, sizeof(int) * 3);
        memcpy(timer, pui32NVData + HBN_TIMER, sizeof(int) * 3);
        clock_init(clock, clock->sec, clock->min, clock->hour,
                   clock->mday, clock->month, clock->year);
        clock->sec += HibernateRTCGet() - HibernateRTCMatchGet(0);
        clock_update(clock);
    }
}

void hibernation_data_store(dgtclock_t *clock, alarm_t *alarm, timer_t *timer)
{
    pui32NVData[HBN_VERIFY] = HBN_CODE_VERIFY;
    pui32NVData[HBN_RTC] = HibernateRTCGet();

    memcpy(pui32NVData + HBN_CLOCK, clock, sizeof(int) * 6);
    memcpy(pui32NVData + HBN_ALARM, alarm, sizeof(int) * 3);
    memcpy(pui32NVData + HBN_TIMER, timer, sizeof(int) * 3);
    HibernateDataSet(pui32NVData, 16);
}

void start_up()
{
    pitch_t notes[14] = {C4, D4, E4, F4, G4, A4, B4, C5, D5, E5, F5, G5, A5, B5};
    int note_time[14] = {400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400};
    int student_id[8] = {2, 1, 9, 1, 1, 1, 0, 1};
    int i;
    uint8_t mask = 0x80;

    buzzer_music_nonblocking(14, notes, note_time, 1);

    global_modify_mode = global_modify_ptr = 1;
    inner_timer_start(INNERTIMER_GENERAL, 3000);
    while (inner_timer_status(INNERTIMER_GENERAL))
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

    UARTStringPut("===================================================================\n");
    UARTStringPut("                                                                   \n");
    UARTStringPut("     o8o                 .   oooo\n");
    UARTStringPut("     \"\"'               .o8   `888\n");
    UARTStringPut("    oooo    oooooooo .o888oo  888 .oo.   oooo    ooo\n");
    UARTStringPut("    `888   d'\"\"7d8P    888    888P\"Y88b   `88.  .8'\n");
    UARTStringPut("     888     .d8P'     888    888   888    `88..8'\n");
    UARTStringPut("     888   .d8P'  .P   888 .  888   888     `888'\n");
    UARTStringPut("    o888o d8888888P    \"888\" o888o o888o     .8'\n");
    UARTStringPut("                                         .o..P'\n");
    UARTStringPut("                                         `Y8P'\n");
    UARTStringPut("                                                                   \n");
    UARTStringPut("===================================================================\n");

    // UARTStringPut("                                                                   \n");
    // UARTStringPut("                                                                   \n");
    // UARTStringPut("                                                                   \n");
    // UARTStringPut("                                                                   \n");
}

void events_catch()
{
    static uint8_t now_val = 0xff, prev_val = 0xff;
    static int prev_pin0 = 1, now_pin0;
    if (!global_already)
        return;
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
        BUTTON_EVENT_FLIP = istriggered(now_val, prev_val, BUTTON_ID_FLIP);
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

    inner_timer_start(INNERTIMER_BUZZER, time_ms);
    while (inner_timer_status(INNERTIMER_BUZZER))
        ;
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
}
/* Turn off the buzzer */
void buzzer_off()
{
    buzzer_enable = 0;
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
}
/* Concurrent music player */
void buzzer_music_nonblocking(int len, pitch_t notes[], int time[], bool set)
{
    static music_t music;
    static int i;
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
    if (set)
    {
        music.notelen = len;
        for (i = 0; i < len; i++)
        {
            music.notes[i] = notes[i];
            music.notetime[i] = time[i];
        }
        i = 0;
        buzzer_enable = 1;
    }
    else
    {
        if (i >= music.notelen)
        {
            buzzer_enable = 0;
            return;
        }
        if (music.notes[i])
        {
            PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, ui32SysClock / music.notes[i]);
            PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, ui32SysClock / music.notes[i] / 500);
            PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, true);
        }
        inner_timer_start(INNERTIMER_BUZZER, music.notetime[i]);
        i++;
    }
}

/* ================================================================
 * Clock methods
 * ================================================================ */
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
    while (clock->yday >= 365 + clock->isleap)
    {
        clock->year += 1;
        clock->yday -= 365 + clock->isleap;
        clock->mday = clock->yday + 1;
        clock->month = 0;
        clock->isleap = (clock->year % 100 && clock->year % 4 == 0) ||
                        (clock->year % 400 == 0);
        clock->days[MONTH_FEB] = 28 + clock->isleap;
    }
    while (clock->mday > clock->days[clock->month])
    {
        clock->mday -= clock->days[clock->month];
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
    if (year > 9999 || year < 0 || month > MONTH_DEC || month < 0)
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
    if (!global_flip)
    {
        for (i = 0; i < 8; i++, mask >>= 1)
        {
            seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & global_blink_mask);
            Delay(1000);
        }
    }
    else
    {
        mask = 0x01;
        for (i = 0; i < 8; i++, mask <<= 1)
        {
            seg_dot = (i == 1 || i == 3) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (flp7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & global_blink_mask);
            Delay(1000);
        }
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
    if (!global_flip)
    {
        for (i = 0; i < 6; i++, mask >>= 1)
        {
            seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & global_blink_mask);
            Delay(1000);
        }
    }
    else
    {
        for (i = 0, mask = 0x01; i < 6; i++, mask <<= 1)
        {
            seg_dot = (i == 1 || i == 3) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (flp7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & global_blink_mask);
            Delay(1000);
        }
    }
}
/* modify clock value with incr caused by button press
 * clock - clock to modify
 *  incr - +1 or -1, correspond to button_add or button_dec
 *   ptr - which number to modify, 1: hour, 2: min, 3: sec,
 *         4: year, 5: month, 6:mday, 0: undefined
 */
void clock_button_increase(dgtclock_t *clock, int incr, int ptr)
{
    switch (ptr)
    {
    case 1:
        clock->hour = (clock->hour + incr + 24) % 24;
        break;
    case 2:
        clock->min = (clock->min + incr + 60) % 60;
        break;
    case 3:
        clock->sec = (clock->sec + incr + 60) % 60;
        break;
    case 4:
        clock->year = (clock->year + incr + 10000) % 10000;
        clock_init(clock, clock->sec, clock->min, clock->hour,
                   clock->mday, clock->month, clock->year);
        break;
    case 5:
        clock->month = (clock->month + incr + 12) % 12;
        clock_init(clock, clock->sec, clock->min, clock->hour,
                   clock->mday, clock->month, clock->year);
        break;
    case 6:
        clock->mday = (clock->mday - 1 + incr + clock->days[clock->month]) % clock->days[clock->month];
        clock->mday = clock->mday + 1;
        clock_init(clock, clock->sec, clock->min, clock->hour,
                   clock->mday, clock->month, clock->year);
        break;
    default:
        break;
    }
}

/* ================================================================
 * Alarm methods
 * ================================================================ */
void alarm_init(alarm_t *alarm, int sec, int min, int hour)
{
    alarm->enable = false;
    alarm->hour = hour;
    alarm->min = min;
    alarm->sec = sec;
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
    uint8_t mask, seg_dot = 0x00;
    bits[0] = alarm->sec % 10, bits[1] = alarm->sec / 10;
    bits[2] = alarm->min % 10, bits[3] = alarm->min / 10;
    bits[4] = alarm->hour % 10, bits[5] = alarm->hour / 10;
    bits[6] = 'L' - 'A' + 10, bits[7] = 'A' - 'A' + 10;
    if (!global_flip)
    {
        for (i = 0, mask = 0x80; i < 8; i++, mask >>= 1)
        {
            seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & (global_blink_mask | 0x03));
            Delay(1000);
        }
    }
    else
    {
        for (i = 0, mask = 0x01; i < 8; i++, mask <<= 1)
        {
            seg_dot = (i == 1 || i == 3) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (flp7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & (global_blink_mask | 0x03));
            Delay(1000);
        }
    }
}
void alarm_go_off(alarm_t *alarm, dgtclock_t *clock)
{
    pitch_t notes[7] = {C4, D4, E4, F4, G4, A4, B4};
    int ntime[7] = {400, 400, 400, 400, 400, 400, 400};

    if (!alarm->enable || alarm->hour != clock->hour ||
        alarm->min != clock->min || alarm->sec != clock->sec)
        return;

    global_display_mode = 2;
    global_modify_mode = global_modify_ptr = 0;
    inner_timer_start(INNERTIMER_ALARM, 10000);
    while (inner_timer_status(INNERTIMER_ALARM) && !BUTTON_EVENT_ENABLE)
    {
        events_catch();
        if (systick_500ms_status)
            alarm_display(alarm);
        else
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        if (!buzzer_enable)
            buzzer_music_nonblocking(7, notes, ntime, 1);
    }
    events_clear();
    buzzer_off();
    global_display_mode = 0;
}
/* modify alarm value with incr caused by button press
 * alarm - alarm to modify
 *  incr - +1 or -1, correspond to button_add or button_dec
 *   ptr - which number to modify, 1: hour, 2: min, 3: sec, 0: undefined
 */
void alarm_button_increase(alarm_t *alarm, int incr, int ptr)
{
    switch (ptr)
    {
    case 1:
        alarm->hour = (alarm->hour + incr + 24) % 24;
        break;
    case 2:
        alarm->min = (alarm->min + incr + 60) % 60;
        break;
    case 3:
        alarm->sec = (alarm->sec + incr + 60) % 60;
        break;
    default:
        break;
    }
}

/* ================================================================
 * Timer methods
 * ================================================================ */
void timer_init(timer_t *timer, int millisec, int sec, int min)
{
    timer->enable = false;
    timer->millisec = millisec;
    timer->sec = sec;
    timer->min = min;
}
/* Update timer when counting down */
void timer_update(timer_t *timer)
{
    if (timer->millisec + timer->sec + timer->min == 0)
        return;
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
        timer->millisec = 0;
        timer->sec = 0;
        timer->min = 0;
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
    if (!global_flip)
    {
        for (i = 0; i < 8; i++, mask >>= 1)
        {
            seg_dot = (i == 2 || i == 4) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (seg7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & (global_blink_mask | 0x03));
            Delay(1000);
        }
    }
    else
    {
        for (i = 0, mask = 0x01; i < 8; i++, mask <<= 1)
        {
            seg_dot = (i == 1 || i == 3) ? 0x80 : 0x00;
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT1, (flp7[bits[i]] | seg_dot));
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, mask & (global_blink_mask | 0x03));
            Delay(1000);
        }
    }
}
/* modify timer value with incr caused by button press
 * timer - timer to modify
 *  incr - +1 or -1, correspond to button_add or button_dec
 *   ptr - which number to modify, 1: min, 2: sec, 3: millisec, 0: undefined
 */
void timer_button_increase(timer_t *timer, int incr, int ptr)
{
    switch (ptr)
    {
    case 1:
        timer->min = (timer->min + incr + 60) % 60;
        break;
    case 2:
        timer->sec = (timer->sec + incr + 60) % 60;
        break;
    case 3:
        timer->millisec = (timer->millisec + incr * 10 + 1000) % 1000;
        break;
    default:
        break;
    }
}
void timer_go_off(timer_t *timer)
{
    pitch_t notes[7] = {C4, D4, E4, F4, G4, A4, B4};
    int ntime[7] = {100, 100, 100, 100, 100, 100, 100};

    if (!timer->enable || timer->millisec || timer->sec || timer->min)
        return;
    timer->enable = false;
    UARTStringPut("\n>>> Time is up!\n");

    global_display_mode = 3;
    global_modify_mode = global_modify_ptr = 0;
    inner_timer_start(INNERTIMER_TIMER, 5000);

    while (inner_timer_status(INNERTIMER_TIMER) && !BUTTON_EVENT_ENABLE)
    {
        events_catch();
        if (systick_500ms_status)
            timer_display(timer);
        else
            I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_OUTPUT_PORT2, 0x00);
        if (!buzzer_enable)
            buzzer_music_nonblocking(7, notes, ntime, 1);
    }
    events_clear();
    buzzer_off();
    global_display_mode = 0;
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
    inner_timer_update();

    if (buzzer_enable && !inner_timer_status(INNERTIMER_BUZZER))
        buzzer_music_nonblocking(0, NULL, NULL, 0);

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

        if (global_already)
        {
            HibernateRTCMatchSet(0, HibernateRTCGet());
            if (clock.sec % 2 == 0)
            {
                hibernation_data_store(&clock, &alarm, &timer);
                // print_log();
            }
        }
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
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, 0);
}

/* Util functions */

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
    else if (strstr(buf, "/"))
        delim = '/';
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
    inner_timer_start(INNERTIMER_GENERAL, ms);
    while (inner_timer_status(INNERTIMER_GENERAL))
        ;
}

void update_blink_mask(uint8_t *mask, int ptr)
{
    uint8_t tmp = *mask;
    if (!global_flip)
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
    else
        switch (ptr)
        {
        case 0:
            tmp = 0xff;
            break;
        case 1:
            tmp ^= 0xf0;
            tmp |= ~0xf0;
            break;
        case 2:
            tmp ^= 0x0c;
            tmp |= ~0x0c;
            break;
        case 3:
            tmp ^= 0x03;
            tmp |= ~0x03;
            break;
        default:
            tmp = 0xff;
        }
    *mask = tmp;
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
void print_log()
{
    int i, tmp[16];
    char buf[MAXLINE];
    UARTStringPut("\n[log] \n");
    HibernateDataGet((uint32_t *)tmp, 16);
    for (i = 0; i < 16; i++)
    {
        sprintf(buf, "p[%d] = %d\n", i, tmp[i]);
        UARTStringPut((byte *)buf);
    }
}
/* Inner timer methods */
void inner_timer_start(int timerId, int timeoutMs)
{
    inner_timers[timerId] = timeoutMs;
}
bool inner_timer_status(int timerId)
{
    return !!inner_timers[timerId];
}
void inner_timer_update()
{
    int i;
    for (i = 0; i < 10; i++)
        inner_timers[i] ? inner_timers[i]-- : 0;
}