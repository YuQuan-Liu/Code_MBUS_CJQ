
#ifndef TASK_H
#define TASK_H


/*
����ײ��вɼ���  �ײ�ɼ���ʹ��͸��ģʽ
������ֱ�ӷ��ͳ���ָ��
cjq_open   cjq_close  ֻ��4·MBUS�̵������п���
*/
uint8_t cjq_open(uint8_t road);
uint8_t cjq_close(void);

void writedata(uint8_t *frame);  //д����  ��Ҫ�ǿ���ͨ��
void readaddr(uint8_t *frame);//����ַ
void writeaddr(uint8_t *frame);//д��ַ

uint8_t power_cmd(FunctionalState NewState); //ʹ��MBUS��38V�� �� 485��12V����Դ

uint8_t relay_1(FunctionalState NewState);
uint8_t relay_2(FunctionalState NewState);
uint8_t relay_3(FunctionalState NewState);
uint8_t relay_4(FunctionalState NewState);


void cjq_timeout(void *p_tmr,void *p_arg);  //TMR_CJQTIMEOUT  ��ʱ�ص�����


//tasks
void Task_Slave(void *p_arg);
void Task_Server(void *p_arg);
void Task_Send_Server(void *p_arg);
void Task_Send_Slave(void *p_arg);
void Task_LED1(void *p_arg);
void Task_OverLoad(void *p_arg);


uint8_t check_cs(uint8_t * start,uint16_t len);  //add the cs and return cs
uint8_t check_frame(uint8_t * start);

#endif