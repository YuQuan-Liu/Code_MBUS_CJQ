

#include "os.h"
#include "stm32f10x_conf.h"
#include "tasks.h"
#include "lib_str.h"
#include "serial.h"
#include "frame_188.h"

//#include "stdlib.h"

extern OS_MEM MEM_Buf;
extern OS_MEM MEM_ISR;

extern OS_Q Q_Slave;      //存放通过485 MBUS 底层发送过来的数据      
extern OS_Q Q_Server;     //存放通过485  或小无线发送来的数据
extern OS_Q Q_Send_Slave;      //存放要发到底层的帧      
extern OS_Q Q_Send_Server;     //存放要发到集中器/Programmer的帧

uint8_t reading = 0;   //0 didn't reading meters    1  reading meters

void Task_Slave(void *p_arg){
  OS_ERR err;
  CPU_TS ts;
  
  uint8_t start_slave = 0;
  uint8_t frame_len = 0;      //the frame's length
  uint8_t data;         //the data get from the queue
  uint8_t *mem_ptr;    //the ptr get from the queue
  uint16_t msg_size;    //the message's size 
  
  uint8_t *buf = 0;   //the buf used put the data in 
  uint8_t *buf_;      //keep the buf's ptr  used to release the buf
  uint8_t frame_ok = 0;
  
  while(DEF_TRUE){
    //收到0x68之后  如果200ms 没有收到数据  就认为超时了
    mem_ptr = OSQPend(&Q_Slave,200,OS_OPT_PEND_BLOCKING,&msg_size,&ts,&err);
    
    if(err == OS_ERR_TIMEOUT){
      if(start_slave == 1){
        buf = buf_;
        start_slave = 0;
        frame_len = 0;
      }
      continue;
    }
    
    
    data = *mem_ptr;
    OSMemPut(&MEM_ISR,mem_ptr,&err);
    
    if(buf == 0){
      buf = OSMemGet(&MEM_Buf,
                     &err);
      if(err != OS_ERR_NONE){
        asm("NOP");
        //didn't get the buf
      }
      buf_ = buf;
      Mem_Set(buf_,0x00,256); //clear the buf
      
    }
    
    //it is the frame come from the meter
    if(start_slave == 0){
      if(data == 0x68){
        *buf++ = data;
        frame_len = 0;
        start_slave = 1;
      }
    }else{
      *buf++ = data;
      if((buf-buf_) == 11){
        frame_len = *(buf_+10)+13;
      }
      if(frame_len > 0 && (buf-buf_) >= frame_len){
        //if it is the end of the frame
        if(*(buf-1) == 0x16){
          //check the frame cs
          if(*(buf-2) == check_cs(buf_,frame_len-2)){
            //the frame is ok;  
            frame_ok = 1;
            buf_ = 0;
            buf = 0;
            start_slave = 0;
            frame_len = 0;
          }
        }else{
          buf = buf_;
          start_slave = 0;
          frame_len = 0;
        }
      }
    }
    
    if(frame_ok == 1){
      OSQPost(&Q_Send_Server,
              buf_,
              frame_len,
              OS_OPT_POST_FIFO,
              &err);
      frame_ok = 0;
    }
  }
}


uint8_t check_cs(uint8_t * start,uint16_t len){
  uint16_t i;
  uint8_t cs = 0;
  for(i = 0;i < len;i++){
    cs += *(start+i);
  }
  return cs;
}

void Task_Server(void *p_arg){
  OS_ERR err;
  CPU_TS ts;
  
  uint8_t start_server = 0;
  uint8_t frame_len = 0;      //the frame's length
  uint8_t data;         //the data get from the queue
  uint8_t *mem_ptr;    //the ptr get from the queue
  uint16_t msg_size;    //the message's size 
  
  uint8_t *buf = 0;   //the buf used put the data in 
  uint8_t *buf_;      //keep the buf's ptr  used to release the buf
  uint8_t frame_ok = 0;
  
  while(DEF_TRUE){
    //收到0x68之后  如果200ms 没有收到数据  就认为超时了
    mem_ptr = OSQPend(&Q_Server,200,OS_OPT_PEND_BLOCKING,&msg_size,&ts,&err);
    
    if(err == OS_ERR_TIMEOUT){
      if(start_server == 1){
        buf = buf_;
        start_server = 0;
        frame_len = 0;
      }
      continue;
    }
    
    
    data = *mem_ptr;
    OSMemPut(&MEM_ISR,mem_ptr,&err);
    
    if(buf == 0){
      buf = OSMemGet(&MEM_Buf,
                     &err);
      if(err != OS_ERR_NONE){
        asm("NOP");
        //didn't get the buf
      }
      buf_ = buf;
      Mem_Set(buf_,0x00,256); //clear the buf
      
    }
    
    //it is the frame come from the meter
    if(start_server == 0){
      if(data == 0x68){
        *buf++ = data;
        frame_len = 0;
        start_server = 1;
      }
    }else{
      *buf++ = data;
      if((buf-buf_) == 11){
        frame_len = *(buf_+10)+13;
      }
      if(frame_len > 0 && (buf-buf_) >= frame_len){
        //if it is the end of the frame
        if(*(buf-1) == 0x16){
          //check the frame cs
          if(*(buf-2) == check_cs(buf_,frame_len-2)){
            //the frame is ok;  
            frame_ok = 1;
            buf_ = 0;
            buf = 0;
            start_server = 0;
            frame_len = 0;
          }
        }else{
          buf = buf_;
          start_server = 0;
          frame_len = 0;
        }
      }
    }
    
    if(frame_ok == 1){
      //check the Frame
      //如果是抄表帧  post to Q_Send_Slave
      //如果是配置帧(设置读取采集器地址 开启关闭通道)  响应处理   将响应结果post to Q_Send_Server
      
      
      frame_len = 0;
    }
  }
}


void power_cmd(FunctionalState NewState){
  if(NewState != DISABLE){
    //打开电源
    mbus_power(ENABLE);
    relay_485(ENABLE);
  }else{
    //关闭电源
    mbus_power(DISABLE);
    relay_485(DISABLE);
  }
}

uint8_t mbus_power(FunctionalState NewState){
  /**/
  OS_ERR err;
  if(NewState != DISABLE){
    
    GPIO_SetBits(GPIOC,GPIO_Pin_13);
    OSTimeDly(1500,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOC,GPIO_Pin_13);
  }
  return 1;
}

uint8_t relay_485(FunctionalState NewState){
  /**/
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOA,GPIO_Pin_6);
   OSTimeDly(600,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOA,GPIO_Pin_6);
  }
  return 1;
}

uint8_t relay_1(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOB,GPIO_Pin_2);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOB,GPIO_Pin_2);
  }
  return 1;
}
uint8_t relay_2(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOB,GPIO_Pin_1);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOB,GPIO_Pin_1);
  }
  return 1;
}
uint8_t relay_3(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOB,GPIO_Pin_0);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOB,GPIO_Pin_0);
  }
  return 1;
}
uint8_t relay_4(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOA,GPIO_Pin_7);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOA,GPIO_Pin_7);
  }
  return 1;
}

uint8_t cjq_open(uint8_t road){
  cjq_close();  //先关闭所有的通道
  switch (road){
  case 1:
    relay_1(ENABLE);
    break;
  case 2:
    relay_2(ENABLE);
    break;
  case 3:
    relay_3(ENABLE);
    break;
  case 4:
    relay_4(ENABLE);
    break;
        
  }
  return 1;
}

uint8_t cjq_close(){
  relay_1(DISABLE);
  relay_2(DISABLE);
  relay_3(DISABLE);
  relay_4(DISABLE);
  return 1;
}

void Task_Send_Server(void *p_arg){
  OS_ERR err;
  CPU_TS ts;
  
  uint8_t *mem_ptr;    //the ptr get from the queue
  uint16_t msg_size;    //the message's size 
  
  while(DEF_TRUE){
    mem_ptr = OSQPend(&Q_Send_Server,0,OS_OPT_PEND_BLOCKING,&msg_size,&ts,&err);
    Send_Server_485(mem_ptr,msg_size);
    OSMemPut(&MEM_Buf,mem_ptr,&err);
  }
}

void Task_Send_Slave(void *p_arg){
  OS_ERR err;
  CPU_TS ts;
  
  uint8_t *mem_ptr;    //the ptr get from the queue
  uint16_t msg_size;    //the message's size 
  
  while(DEF_TRUE){
    mem_ptr = OSQPend(&Q_Send_Slave,0,OS_OPT_PEND_BLOCKING,&msg_size,&ts,&err);
    Send_Slave(mem_ptr,msg_size);
    OSMemPut(&MEM_Buf,mem_ptr,&err);
  }
}

void Task_LED1(void *p_arg){
  OS_ERR err;
  
  while(DEF_TRUE){
    if(reading == 0){
      GPIO_SetBits(GPIOB,GPIO_Pin_6);
      OSTimeDly(1000,
                    OS_OPT_TIME_DLY,
                    &err);
      GPIO_ResetBits(GPIOB,GPIO_Pin_6);
      OSTimeDly(1000,
                    OS_OPT_TIME_DLY,
                    &err);
    }else{
      GPIO_SetBits(GPIOB,GPIO_Pin_6);
      OSTimeDly(300,
                    OS_OPT_TIME_DLY,
                    &err);
      GPIO_ResetBits(GPIOB,GPIO_Pin_6);
      OSTimeDly(300,
                    OS_OPT_TIME_DLY,
                    &err);
    }
  }
}



extern OS_FLAG_GRP FLAG_Event;
void Task_OverLoad(void *p_arg){
  OS_ERR err;
  CPU_TS ts;
  
  while(DEF_TRUE){
    
    OSFlagPend(&FLAG_Event,
                OVERLOAD,
                0,
                OS_OPT_PEND_FLAG_SET_ANY + OS_OPT_PEND_FLAG_CONSUME+OS_OPT_PEND_BLOCKING,
                &ts,
                &err);
    
    OSTimeDly(500,
                  OS_OPT_TIME_DLY,
                  &err);
    if(!GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)){
      //enable the beep
      GPIO_SetBits(GPIOA,GPIO_Pin_15);
      //disable the mbus power
      GPIO_ResetBits(GPIOC,GPIO_Pin_13);
      //Light the LED3
      
      while(DEF_TRUE){
        GPIO_SetBits(GPIOB,GPIO_Pin_9);
        OSTimeDly(100,
                  OS_OPT_TIME_DLY,
                  &err);
        GPIO_ResetBits(GPIOB,GPIO_Pin_9);
        OSTimeDly(100,
                  OS_OPT_TIME_DLY,
                  &err);
      }
    }
    
  }
}