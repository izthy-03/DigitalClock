#include "headers.h"

void IO_initialize(void);

void Delay(uint32_t value);
void S800_GPIO_Init(void);
uint8_t I2C0_WriteByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t WriteData);
uint8_t I2C0_ReadByte(uint8_t DevAddr, uint8_t RegAddr);
void S800_I2C0_Init(void);
void S800_UART_Init(void);

void UARTStringPut(uint8_t *cMessage);
void UARTStringPutNonBlocking(const char *cMessage);
