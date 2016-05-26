

#ifndef SERIAL_H
#define SERIAL_H


void USART1_Handler(void);
void USART2_Handler(void);
void USART3_Handler(void);



ErrorStatus Send_Slave(uint8_t * data,uint16_t count);  //mbus  485 USART2
ErrorStatus Send_Server_485(uint8_t * data,uint16_t count);  //�ɼ������ϲ�ͨ��485��USART3��ͨ��
ErrorStatus Send_Server_RF(uint8_t * data,uint16_t count);//�ɼ������ϲ�ͨ������RF��USART1��ͨ��
/*
����
*/
#define OVERLOAD (OS_FLAGS)0x0001

void OverLoad(void);

#endif