#include "initialize.h"

uint32_t ui32SysClock, ui32IntPriorityGroup, ui32IntPriorityMask;
uint32_t ui32IntPrioritySystick, ui32IntPriorityUart0;

uint8_t seg7[40] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x58, 0x5e, 0x079, 0x71, 0x5c};
uint8_t flp7[40] = {0x3f, 0x30, 0x5b, 0x79, 0x74, 0x6d, 0x6f, 0x38, 0x7f, 0x7d, 0x7e, 0x67, 0x43, 0x73, 0x4f, 0x4e};
uint8_t uart_receive_char;

uint32_t ui32Status;
uint32_t pui32NVData[64];

void IO_initialize()
{
    seg7['L' - 'A' + 10] = 0x38;
    flp7['L' - 'A' + 10] = 0x07;
    // use internal 16M oscillator, PIOSC
    // ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ |SYSCTL_OSC_INT |SYSCTL_USE_OSC), 16000000);
    // ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ |SYSCTL_OSC_INT |SYSCTL_USE_OSC), 8000000);
    // use external 25M oscillator, MOSC
    // ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN |SYSCTL_USE_OSC), 25000000);

    // use external 25M oscillator and PLL to 120M
    // ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |SYSCTL_CFG_VCO_480), 120000000);;
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_16MHZ | SYSCTL_OSC_INT | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 20000000);

    SysTickPeriodSet(ui32SysClock / SYSTICK_FREQUENCY);
    SysTickEnable();
    SysTickIntEnable(); // Enable Systick interrupt

    S800_GPIO_Init();
    S800_I2C0_Init();
    S800_UART_Init();

    Hibernation_Init();

    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT); // Enable UART0 RX,TX interrupt
    IntMasterEnable();
    ui32IntPriorityMask = IntPriorityMaskGet();
    IntPriorityGroupingSet(3); // Set all priority to pre-emtption priority

    // IntPrioritySet(INT_UART0, 3);         // Set INT_UART0 to highest priority
    // IntPrioritySet(FAULT_SYSTICK, 0x0e0); // Set INT_SYSTICK to lowest priority

    IntPrioritySet(INT_UART0, 0x0e0); // Set INT_UART0 to lowest priority
    IntPrioritySet(FAULT_SYSTICK, 3); // Set INT_SYSTICK to highest priority

    ui32IntPriorityGroup = IntPriorityGroupingGet();

    ui32IntPriorityUart0 = IntPriorityGet(INT_UART0);
    ui32IntPrioritySystick = IntPriorityGet(FAULT_SYSTICK);

    PWM_Init();
}

void Delay(uint32_t value)
{
    uint32_t ui32Loop;
    for (ui32Loop = 0; ui32Loop < value; ui32Loop++)
    {
    };
}

void UARTStringPut(uint8_t *cMessage)
{
    while (*cMessage != '\0')
        UARTCharPut(UART0_BASE, *(cMessage++));
}
void UARTStringPutNonBlocking(const char *cMessage)
{
    while (*cMessage != '\0')
        UARTCharPutNonBlocking(UART0_BASE, *(cMessage++));
}

void S800_UART_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); // Enable PortA
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA))
        ; // Wait for the GPIO moduleA ready

    GPIOPinConfigure(GPIO_PA0_U0RX); // Set GPIO A0 and A1 as UART pins.
    GPIOPinConfigure(GPIO_PA1_U0TX);

    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure the UART for 115,200, 8-N-1 operation.
    UARTConfigSetExpClk(UART0_BASE, ui32SysClock, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    UARTStringPut((uint8_t *)"\r\nDigital clock is starting...\r\n");
    UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX2_8, UART_FIFO_RX7_8);
}
void S800_GPIO_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); // Enable PortF
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
        ;                                        // Wait for the GPIO moduleF ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ); // Enable PortJ
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOJ))
        ;                                        // Wait for the GPIO moduleJ ready
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION); // Enable PortN
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPION))
        ; // Wait for the GPIO moduleN ready

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0); // Set PF0 as Output pin
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0); // Set PN0 as Output pin
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1); // Set PN1 as Output pin

    GPIOPinTypeGPIOInput(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1); // Set the PJ0,PJ1 as input pin
    GPIOPadConfigSet(GPIO_PORTJ_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void S800_I2C0_Init(void)
{
    uint8_t result;
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    I2CMasterInitExpClk(I2C0_BASE, ui32SysClock, true); // config I2C0 400k
    I2CMasterEnable(I2C0_BASE);

    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT0, 0x0ff); // config port 0 as input
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT1, 0x0);   // config port 1 as output
    result = I2C0_WriteByte(TCA6424_I2CADDR, TCA6424_CONFIG_PORT2, 0x0);   // config port 2 as output

    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_CONFIG, 0x00);  // config port as output
    result = I2C0_WriteByte(PCA9557_I2CADDR, PCA9557_OUTPUT, 0x0ff); // turn off the LED1-8
}

uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData)
{
    uint8_t rop;
    while (I2CMasterBusy(I2C0_BASE))
    {
    };
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    while (I2CMasterBusy(I2C0_BASE))
    {
    };
    rop = (uint8_t)I2CMasterErr(I2C0_BASE);

    I2CMasterDataPut(I2C0_BASE, WriteData);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    while (I2CMasterBusy(I2C0_BASE))
    {
    };

    rop = (uint8_t)I2CMasterErr(I2C0_BASE);
    return rop;
}

uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr)
{
    uint8_t value, rop;
    while (I2CMasterBusy(I2C0_BASE))
    {
    };
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, false);
    I2CMasterDataPut(I2C0_BASE, RegAddr);
    //	I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_SEND);
    while (I2CMasterBusBusy(I2C0_BASE))
        ;
    rop = (uint8_t)I2CMasterErr(I2C0_BASE);
    Delay(1000);
    // receive data
    I2CMasterSlaveAddrSet(I2C0_BASE, DevAddr, true);
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
    while (I2CMasterBusBusy(I2C0_BASE))
        ;
    value = I2CMasterDataGet(I2C0_BASE);
    Delay(1000);
    return value;
}

void PWM_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK); // Enable PortK
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOK))
        ; // Wait for the GPIO moduleK ready

    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_5); // Set PK5 as Output pin

    GPIOPadConfigSet(GPIO_PORTK_BASE, GPIO_PIN_5, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU);

    GPIOPinConfigure(GPIO_PK5_M0PWM7);

    GPIOPinTypePWM(GPIO_PORTK_BASE, GPIO_PIN_5);
    PWMGenConfigure(PWM0_BASE, PWM_GEN_3, PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_NO_SYNC);

    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, 4000);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, 4000 / 4);
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, true);
    PWMGenEnable(PWM0_BASE, PWM_GEN_3);
    PWMOutputState(PWM0_BASE, PWM_OUT_7_BIT, false);
}

void Hibernation_Init()
{
    int i;
    char buf[MAXLINE];
    //
    // Need to enable the hibernation peripheral after wake/reset, before using
    // it.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);

    //
    // Wait for the Hibernate module to be ready.
    //
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_HIBERNATE))
    {
    }
    //
    // Enable clocking to the Hibernation module.
    //
    HibernateEnableExpClk(SysCtlClockGet());
    //
    // User-implemented delay here to allow crystal to power up and stabilize.
    //

    //
    // Configure the clock source for Hibernation module and enable the RTC
    // feature.
    //
    HibernateClockConfig(HIBERNATE_OSC_LOWDRIVE);

    HibernateLowBatSet(HIBERNATE_LOW_BAT_DETECT | HIBERNATE_LOW_BAT_2_3V);
    sprintf(buf, "LowBat = 0x%x\n", HibernateLowBatGet());
    UARTStringPut(buf);

    HibernateRTCEnable();
    //
    // Set the RTC to 0 or an initial value. The RTC can be set once when the
    // system is initialized after the cold startup and then left to run. Or
    // it can be initialized before every hibernate.
    //
    // if (HibernateRTCMatchGet(0) != 21911101)
    // {
    //     HibernateRTCSet(0);
    //     HibernateRTCMatchSet(0, 21911101);
    // }
    sprintf(buf, "RTC = %d\n", HibernateRTCGet());
    UARTStringPut(buf);
    sprintf(buf, "RTCMatch = %d\n", HibernateRTCMatchGet(0));
    UARTStringPut(buf);
    //
    // Set the match 0 register for 30 seconds from now.
    //
    // HibernateRTCMatchSet(0, HibernateRTCGet() + 30);
    //
    // Clear any pending status.
    //
    ui32Status = HibernateIntStatus(0);
    HibernateIntClear(ui32Status);
}
