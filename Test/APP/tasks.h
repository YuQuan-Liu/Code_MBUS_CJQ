
#ifndef TASK_H
#define TASK_H


/*
如果底层有采集器  底层采集器使用透传模式
集中器直接发送抄表指令
cjq_open   cjq_close  只对4路MBUS继电器进行控制
*/
uint8_t cjq_open(uint8_t road);
uint8_t cjq_close(void);

void writedata(uint8_t *frame);  //写数据  主要是开关通道
void readaddr(uint8_t *frame);//读地址
void writeaddr(uint8_t *frame);//写地址

uint8_t power_cmd(FunctionalState NewState); //使能MBUS（38V） 和 485（12V）电源

uint8_t relay_1(FunctionalState NewState);
uint8_t relay_2(FunctionalState NewState);
uint8_t relay_3(FunctionalState NewState);
uint8_t relay_4(FunctionalState NewState);


void cjq_timeout(void *p_tmr,void *p_arg);  //TMR_CJQTIMEOUT  超时回调函数


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