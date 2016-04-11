

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

extern OS_TMR TMR_CJQTIMEOUT;    //打开采集器之后 10分钟超时 自动关闭通道

extern uint8_t cjqaddr[6];

uint8_t reading = 0;   //0 didn't reading meters    1  reading meters
uint8_t cjq_isopen = 0;  //0~不给表发送数据  1~给表发送数据
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
      if(err == OS_ERR_NONE){
        //get the buf
        buf_ = buf;
        Mem_Set(buf_,0x00,256); //clear the buf
      }else{
        //didn't get the buf
        asm("NOP");
        continue;
      }
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
          }else{
            buf = buf_;
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
      buf_ = 0;
      buf = 0;
      start_slave = 0;
      frame_len = 0;
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

uint8_t radioaddr[6] = {0xAA,0xAA,0xAA,0xAA,0xAA,0xAA};
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
      if(err == OS_ERR_NONE){
        //get the buf
        buf_ = buf;
        Mem_Set(buf_,0x00,256); //clear the buf
      }else{
        //didn't get the buf
        asm("NOP");
        continue;
      }
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
          }else{
            buf = buf_;
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
      
      if(buf_[1] == 0x10){
        if(cjq_isopen > 0){
          //要发给表的
          OSQPost(&Q_Send_Slave,
                  buf_,
                  frame_len,
                  OS_OPT_POST_FIFO,
                  &err);
          buf = 0;
          buf_ = 0;
          start_server = 0;
          frame_len = 0;
        }else{
          //发送给别的采集器的
          //继续接收
          buf = buf_;
          start_server = 0;
          frame_len = 0;
        }
      }else{
        if(buf_[1] == 0xA0){
          //发给采集器  需要采集器处理的
          //判断采集器地址
          if(Mem_Cmp(radioaddr,buf_+2,6) || Mem_Cmp(cjqaddr+1,buf_+3,5)){
            switch(buf_[9]){
            case CTR_READADDR:
              //读地址
              readaddr(buf_);
              buf = 0;
              buf_ = 0;
              start_server = 0;
              frame_len = 0;
              break;
            case CTR_WRITEADDR:
              //写地址
              writeaddr(buf_);
              buf = 0;
              buf_ = 0;
              start_server = 0;
              frame_len = 0;
              break;
            case CTR_WRITEDATA:
              //开关通道
              writedata(buf_);
              buf = 0;
              buf_ = 0;
              start_server = 0;
              frame_len = 0;
              break;
            default:
              //继续接收
              buf = buf_;
              start_server = 0;
              frame_len = 0;
              break;
            }
          }else{
            //继续接收
            buf = buf_;
            start_server = 0;
            frame_len = 0;
          }
        }else{
          //继续接收
          buf = buf_;
          start_server = 0;
          frame_len = 0;
        }
      }
      frame_ok = 0;
    }
  }
}

void writeaddr(uint8_t *frame){
  uint8_t i = 0;
  OS_ERR err;
  
  if(frame[11] == DATAFLAG_WA_L && frame[12] == DATAFLAG_WA_H){
    FLASH_UnlockBank1();
    FLASH_ErasePage(0x800FC00);
    FLASH_ProgramHalfWord(0x800FC00,*(uint16_t *)(frame+14));
    FLASH_ProgramHalfWord(0x800FC02,*(uint16_t *)(frame+16));
    FLASH_ProgramHalfWord(0x800FC04,*(uint16_t *)(frame+18));
    FLASH_LockBank1();
    
    for(i = 0;i<6;i++){
      cjqaddr[i] = *(u8 *)(0x800FC00+i);
      frame[2+i] = *(u8 *)(0x800FC00+i);
    }
    frame[8] = 0x00;
    frame[9] = CTR_ACKWA;
    frame[10] = 0x03;
    frame[14] = check_cs(frame,14);
    frame[15] = 0x16;
    OSQPost(&Q_Send_Server,
              frame,
              16,
              OS_OPT_POST_FIFO,
              &err);
    
  }else{
    //不是要的处理
    //将BUF 还回去   
    OSMemPut(&MEM_Buf,frame,&err);
  }
}

void readaddr(uint8_t *frame){
  uint8_t i = 0;
  OS_ERR err;
  if(frame[11] == DATAFLAG_RA_L && frame[12] == DATAFLAG_RA_H){
    frame[9] = CTR_ACKADDR;
    for(i = 0;i<6;i++){
      frame[2+i] = cjqaddr[i];
    }
    frame[14] = check_cs(frame,14);
    frame[15] = 0x16;
    OSQPost(&Q_Send_Server,
              frame,
              16,
              OS_OPT_POST_FIFO,
              &err);
  }else{
    //不是要的处理
    //将BUF 还回去   
    OSMemPut(&MEM_Buf,frame,&err);
  }
}

void writedata(uint8_t *frame){
  uint8_t i = 0;
  OS_ERR err;
  if(frame[11] == DATAFLAG_WV_L && frame[12] == DATAFLAG_WV_H){
    if(frame[14] == OPEN_VALVE){
      //open cjq
      power_cmd(ENABLE);  //打开电源
      cjq_open(frame[2]);
      frame[14] = 0x00;
    }else{
      //close cjq
      power_cmd(DISABLE);   //关闭电源
      cjq_close();
      frame[14] = 0x02;
    }
    
    //write the valve control status 
    frame[9] = CTR_ACKWD;
    frame[10] = 0x05;
    
    //frame[14] = st[0];
    frame[15] = 0x00;
    
    frame[16] = check_cs(frame,16);
    frame[17] = 0x16;
    OSQPost(&Q_Send_Server,
              frame,
              18,
              OS_OPT_POST_FIFO,
              &err);
  }else{
    //不是要的处理
    //将BUF 还回去   
    OSMemPut(&MEM_Buf,frame,&err);
  }
}

uint8_t power_cmd(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
    //打开电源
    GPIO_SetBits(GPIOA,GPIO_Pin_0);
    GPIO_SetBits(GPIOB,GPIO_Pin_1);
    OSTimeDly(1000,
              OS_OPT_TIME_DLY,
              &err);
  }else{
    //关闭电源
    GPIO_ResetBits(GPIOA,GPIO_Pin_0);
    GPIO_ResetBits(GPIOB,GPIO_Pin_1);
  }
}

uint8_t relay_1(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOA,GPIO_Pin_15);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOA,GPIO_Pin_15);
  }
  return 1;
}
uint8_t relay_2(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOB,GPIO_Pin_3);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOB,GPIO_Pin_3);
  }
  return 1;
}
uint8_t relay_3(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOB,GPIO_Pin_4);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOB,GPIO_Pin_4);
  }
  return 1;
}
uint8_t relay_4(FunctionalState NewState){
  OS_ERR err;
  if(NewState != DISABLE){
   GPIO_SetBits(GPIOB,GPIO_Pin_5);
   OSTimeDly(3000,
                  OS_OPT_TIME_DLY,
                  &err);
  }else{
    GPIO_ResetBits(GPIOB,GPIO_Pin_5);
  }
  return 1;
}

uint8_t cjq_open(uint8_t road){
  OS_ERR err;
  if(cjq_isopen == road){
    OSTmrStart(&TMR_CJQTIMEOUT,&err);
  }else{
    cjq_close();  //先关闭所有的通道
    switch (road){
    case 1:
      relay_1(ENABLE);
      cjq_isopen = 1;
      break;
    case 2:
      relay_2(ENABLE);
      cjq_isopen = 2;
      break;
    case 3:
      relay_3(ENABLE);
      cjq_isopen = 3;
      break;
    case 4:
      relay_4(ENABLE);
      cjq_isopen = 4;
      break;
    }
    OSTmrStart(&TMR_CJQTIMEOUT,&err);
  }
  return 1;
}

uint8_t cjq_close(){
  OS_ERR err;
  relay_1(DISABLE);
  relay_2(DISABLE);
  relay_3(DISABLE);
  relay_4(DISABLE);
  cjq_isopen = 0;
  OSTmrStop(&TMR_CJQTIMEOUT,OS_OPT_TMR_NONE,0,&err);
  return 1;
}

void cjq_timeout(void *p_tmr,void *p_arg){
  //关闭电源
  power_cmd(DISABLE);
  //关闭通道
  cjq_close();
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
  
  //LED2
  while(DEF_TRUE){
    if(cjq_isopen == 0){
      GPIO_SetBits(GPIOA,GPIO_Pin_6);
      OSTimeDly(1000,
                    OS_OPT_TIME_DLY,
                    &err);
      GPIO_ResetBits(GPIOA,GPIO_Pin_6);
      OSTimeDly(1000,
                    OS_OPT_TIME_DLY,
                    &err);
    }else{
      GPIO_SetBits(GPIOA,GPIO_Pin_6);
      OSTimeDly(300,
                    OS_OPT_TIME_DLY,
                    &err);
      GPIO_ResetBits(GPIOA,GPIO_Pin_6);
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
  uint16_t cnt = 0;
  while(DEF_TRUE){
    
    OSFlagPend(&FLAG_Event,
                OVERLOAD,
                0,
                OS_OPT_PEND_FLAG_SET_ANY + OS_OPT_PEND_FLAG_CONSUME+OS_OPT_PEND_BLOCKING,
                &ts,
                &err);
    
    OSTimeDly(200,
                  OS_OPT_TIME_DLY,
                  &err);
    if(!GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_12)){
      //enable the beep
      GPIO_SetBits(GPIOA,GPIO_Pin_1);
      //disable the mbus power
      power_cmd(DISABLE);
      //Light the LED3
      
      //蜂鸣器响20s
      while(cnt < 100){
        GPIO_SetBits(GPIOA,GPIO_Pin_7);
        OSTimeDly(100,
                  OS_OPT_TIME_DLY,
                  &err);
        GPIO_ResetBits(GPIOA,GPIO_Pin_7);
        OSTimeDly(100,
                  OS_OPT_TIME_DLY,
                  &err);
        cnt++;
      }
      cnt = 0;
    }
    
  }
}