


#include "includes.h"
#include "tasks.h"



//OS_TCBs
OS_TCB  TCB_Start;
CPU_STK STK_Start[APP_START_TASK_STK_SIZE];

OS_TCB TCB_LED1;
CPU_STK STK_LED1[APP_START_TASK_STK_SIZE];

//处理集中器/programmer发送过来的数据
OS_TCB TCB_Server;
CPU_STK STK_Server[APP_START_TASK_STK_SIZE];

//处理表 底层发送过来的数据
OS_TCB TCB_Slave;
CPU_STK STK_Slave[APP_START_TASK_STK_SIZE];

OS_TCB TCB_Send_Server;
CPU_STK STK_Send_Server[APP_START_TASK_STK_SIZE];

//处理表 底层发送过来的数据
OS_TCB TCB_Send_Slave;
CPU_STK STK_Send_Slave[APP_START_TASK_STK_SIZE];

//task deal the overload
OS_TCB TCB_OverLoad;
CPU_STK STK_OverLoad[APP_START_TASK_STK_SIZE];

//OS_MEMs
//receive the data from the ISR    put the data in the mem to deal buf
OS_MEM MEM_Buf;
uint8_t mem_buf[4][256];

//be used in the ISR   get the data from the usart*  post it to the deal task
OS_MEM MEM_ISR;
uint8_t mem_isr[30][4];

//OS_SEMs ;
OS_SEM SEM_Send_Server_485;  //通过485 往集中器/programmer发送数据
OS_SEM SEM_Send_Slave;  //给底层表具发送数据

//OS_Qs
OS_Q Q_Slave;      //存放通过485 MBUS 底层发送过来的数据      
OS_Q Q_Server;     //存放通过485  或小无线发送来的数据
OS_Q Q_Send_Slave;      //存放要发到底层的帧      
OS_Q Q_Send_Server;     //存放要发到集中器/Programmer的帧

//OS_FLAG
OS_FLAG_GRP FLAG_Event;

//OS_TMR
OS_TMR TMR_CJQTIMEOUT;    //打开采集器之后 10分钟超时 自动关闭通道

uint8_t cjqaddr[6] = {0x01,0x00,0x00,0x00,0x00,0x00};

void TaskStart(void *p_arg);
void TaskCreate(void);
void ObjCreate(void);


int main (void){
  OS_ERR err;
  CPU_IntDis();
  
  OSInit(&err);
  
  OSTaskCreate((OS_TCB  *)&TCB_Start,
               (CPU_CHAR *)"START",
               (OS_TASK_PTR )TaskStart,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO,
               (CPU_STK *)&STK_Start[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
  
  OSStart(&err);
}

void TaskStart(void *p_arg){
  CPU_INT32U cnts;
  CPU_INT32U cpu_clk_freq;
  OS_ERR err;
  uint8_t i = 0;  //get the cjqaddr
  //uint32_t flashid;
  
  BSP_Init();
  CPU_Init();
  
  cpu_clk_freq = BSP_CPU_ClkFreq();
  cnts = cpu_clk_freq / (CPU_INT32U)OS_CFG_TICK_RATE_HZ;
  OS_CPU_SysTickInit(cnts);
  /*
  while(DEF_TRUE){
    //check the w25x16 是否存在
    flashid = sFLASH_ReadID();
    if(FLASH_ID == flashid){
      break;
    }
    OSTimeDly(100,
              OS_OPT_TIME_DLY,
              &err);
  }
  */
  TaskCreate();
  ObjCreate();
  
  if(*(u8 *)0x800FC00 != 0xFF){
    for(i = 0;i<6;i++){
      cjqaddr[i] = *(u8 *)(0x800FC00+i);
    }
  }
  
  //Open the IWDG;
  //BSP_IWDG_Init();
  
  while(DEF_TRUE){
    /* Reload IWDG counter */
    IWDG_ReloadCounter();
    
    //LED1
    GPIO_SetBits(GPIOB,GPIO_Pin_0);
    OSTimeDly(1000,
                  OS_OPT_TIME_DLY,
                  &err);
    GPIO_ResetBits(GPIOB,GPIO_Pin_0);
    OSTimeDly(1000,
                  OS_OPT_TIME_DLY,
                  &err);
  }
}

void TaskCreate(void){
  OS_ERR err;
  
  /*the data come from slave */
  OSTaskCreate((OS_TCB  *)&TCB_Slave,
               (CPU_CHAR *)"U2",
               (OS_TASK_PTR )Task_Slave,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO + 1,
               (CPU_STK *)&STK_Slave[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
  /*the data come from the server */
  OSTaskCreate((OS_TCB  *)&TCB_Server,
               (CPU_CHAR *)"U1_3",
               (OS_TASK_PTR )Task_Server,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO + 2,
               (CPU_STK *)&STK_Server[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
  //OS_CFG_TICK_TASK_PRIO == 6 = APP_START_TASK_PRIO + 3
  
  OSTaskCreate((OS_TCB  *)&TCB_Send_Server,
               (CPU_CHAR *)"",
               (OS_TASK_PTR )Task_Send_Server,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO + 4,
               (CPU_STK *)&STK_Send_Server[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
  
  OSTaskCreate((OS_TCB  *)&TCB_Send_Slave,
               (CPU_CHAR *)"",
               (OS_TASK_PTR )Task_Send_Slave,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO + 5,
               (CPU_STK *)&STK_Send_Slave[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
  
  //blink the led 1
  /**/
  OSTaskCreate((OS_TCB  *)&TCB_LED1,
               (CPU_CHAR *)"LED1",
               (OS_TASK_PTR )Task_LED1,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO + 8,
               (CPU_STK *)&STK_LED1[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
  
  //overload
  /**/
  OSTaskCreate((OS_TCB  *)&TCB_OverLoad,
               (CPU_CHAR *)"OverLoad",
               (OS_TASK_PTR )Task_OverLoad,
               (void *) 0,
               (OS_PRIO )APP_START_TASK_PRIO + 9,
               (CPU_STK *)&STK_OverLoad[0],
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE/10,
               (CPU_STK_SIZE)APP_START_TASK_STK_SIZE,
               (OS_MSG_QTY) 0u,
               (OS_TICK) 0u,
               (void *) 0,
               (OS_OPT) (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR *)&err);
}

void ObjCreate(void){
  OS_ERR err;
  
  //OS_MEM
  OSMemCreate((OS_MEM *)&MEM_Buf,
              (CPU_CHAR *)"",
              (void *)&mem_buf[0][0],
              (OS_MEM_QTY)4,
              (OS_MEM_SIZE)256,
              (OS_ERR *)&err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  OSMemCreate((OS_MEM *)&MEM_ISR,
              (CPU_CHAR *)"",
              (void *)&mem_isr[0][0],
              (OS_MEM_QTY)30,
              (OS_MEM_SIZE)4,
              (OS_ERR *)&err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  //OS_SEM
  OSSemCreate(&SEM_Send_Slave,
              "",
              0,
              &err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  OSSemCreate(&SEM_Send_Server_485,
              "",
              0,
              &err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  //OS_Qs
  OSQCreate(&Q_Slave,
            "slave",
            5,
            &err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  OSQCreate(&Q_Server,
            "read",
            5,
            &err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  OSQCreate(&Q_Send_Slave,
            "",
            5,
            &err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  OSQCreate(&Q_Send_Server,
            "",
            5,
            &err);
  if(err != OS_ERR_NONE){
    return;
  }
  
  //OS_FLAGS
  OSFlagCreate(&FLAG_Event,
               "",
               (OS_FLAGS)0,
               &err);
  
  //OS_TMRs
  OSTmrCreate(&TMR_CJQTIMEOUT,
              "",
              6000,
              0,
              OS_OPT_TMR_ONE_SHOT,
              cjq_timeout,
              0,
              &err);
  
}



