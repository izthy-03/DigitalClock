#ifndef _INITIALIZE_H
#define _INITIALIZE_H

#include "headers.h"

extern uint32_t ui32SysClock, ui32IntPriorityGroup, ui32IntPriorityMask;
extern uint32_t ui32IntPrioritySystick, ui32IntPriorityUart0;

extern uint8_t seg7[40];
extern uint8_t uart_receive_char;

extern uint32_t ui32Status;
extern uint32_t pui32NVData[64];

void IO_initialize(void);
void PWM_Init(void);
void Delay(uint32_t value);
void S800_GPIO_Init(void);
uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData);
uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr);
void S800_I2C0_Init(void);
void S800_UART_Init(void);
void Hibernation_Init(void);

void UARTStringPut(uint8_t *cMessage);
void UARTStringPutNonBlocking(const char *cMessage);

#endif
