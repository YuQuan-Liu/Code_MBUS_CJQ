

#ifndef SERIAL_H
#define SERIAL_H


void USART1_Handler(void);
void USART2_Handler(void);
void USART3_Handler(void);



ErrorStatus Send_Slave(uint8_t * data,uint16_t count);  //mbus  485 USART2
ErrorStatus Send_Server_485(uint8_t * data,uint16_t count);  //采集器与上层通过485（USART3）通信
ErrorStatus Send_Server_RF(uint8_t * data,uint16_t count);//采集器与上层通过无线RF（USART1）通信
/*
过载
*/
#define OVERLOAD (OS_FLAGS)0x0001

void OverLoad(void);

#endif