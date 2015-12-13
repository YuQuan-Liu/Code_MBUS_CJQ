

#include "os.h"
#include "stm32f10x_conf.h"
#include "tasks.h"
#include "lib_str.h"
#include "serial.h"
#include "frame_188.h"

//#include "stdlib.h"

extern OS_MEM MEM_Buf;
extern OS_MEM MEM_ISR;

extern OS_Q Q_Slave;      //���ͨ��485 MBUS �ײ㷢�͹���������      
extern OS_Q Q_Server;     //���ͨ��485  ��С���߷�����������
extern OS_Q Q_Send_Slave;      //���Ҫ�����ײ��֡      
extern OS_Q Q_Send_Server;     //���Ҫ����������/Programmer��֡

extern OS_TMR TMR_CJQTIMEOUT;    //�򿪲ɼ���֮�� 10���ӳ�ʱ �Զ��ر�ͨ��

extern uint8_t cjqaddr[6];

uint8_t reading = 0;   //0 didn't reading meters    1  reading meters
uint8_t cjq_isopen = 0;  //0~��������������  1~������������
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
    //�յ�0x68֮��  ���200ms û���յ�����  ����Ϊ��ʱ��
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
    //�յ�0x68֮��  ���200ms û���յ�����  ����Ϊ��ʱ��
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
      //����ǳ���֡  post to Q_Send_Slave
      //���������֡(���ö�ȡ�ɼ�����ַ �����ر�ͨ��)  ��Ӧ����   ����Ӧ���post to Q_Send_Server
      
      if(buf_[1] == 0x10){
        if(cjq_isopen == 1){
          //Ҫ��������
          OSQPost(&Q_Send_Slave,
                  buf_,
                  frame_len,
                  OS_OPT_POST_FIFO,
                  &err);
        }else{
          //���͸���Ĳɼ�����
          //��������
          buf = buf_;
          start_server = 0;
          frame_len = 0;
        }
      }else{
        if(buf_[1] == 0xA0){
          //�����ɼ���  ��Ҫ�ɼ���������
          //�жϲɼ�����ַ
          if(Mem_Cmp(radioaddr,buf_+2,6) || Mem_Cmp(cjqaddr,buf_+2,6)){
            switch(buf_[9]){
            case CTR_READADDR:
              //����ַ
              readaddr(buf_);
              break;
            case CTR_WRITEADDR:
              //д��ַ
              writeaddr(buf_);
              break;
            case CTR_WRITEDATA:
              //����ͨ��
              writedata(buf_);
              break;
            default:
              //��������
              buf = buf_;
              start_server = 0;
              frame_len = 0;
              break;
            }
          }else{
            //��������
            buf = buf_;
            start_server = 0;
            frame_len = 0;
          }
        }else{
          //��������
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
      cjqaddr[i] = *(u32 *)(0x800FC00+i);
      frame[2+i] = *(u32 *)(0x800FC00+i);
    }
    
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
    //����Ҫ�Ĵ���
    //��BUF ����ȥ   
    OSMemPut(&MEM_Buf,frame,&err);
  }
}

void readaddr(uint8_t *frame){
  uint8_t i = 0;
  OS_ERR err;
  if(frame[11] == DATAFLAG_RA_L && frame[12] == DATAFLAG_RA_H){
    frame[9] = CTR_ACKADDR;
    for(i = 0;i<6;i++){
      frame[2+i] = *(u32 *)(0x800FC00+i);
    }
    frame[14] = check_cs(frame,14);
    frame[15] = 0x16;
    OSQPost(&Q_Send_Server,
              frame,
              16,
              OS_OPT_POST_FIFO,
              &err);
  }else{
    //����Ҫ�Ĵ���
    //��BUF ����ȥ   
    OSMemPut(&MEM_Buf,frame,&err);
  }
}

void writedata(uint8_t *frame){
  uint8_t i = 0;
  OS_ERR err;
  if(frame[11] == DATAFLAG_WV_L && frame[12] == DATAFLAG_WV_H){
    if(frame[14] == OPEN_VALVE){
      //open cjq
      cjq_open(frame[2]);
      frame[14] = 0x00;
    }else{
      //close cjq
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
    //����Ҫ�Ĵ���
    //��BUF ����ȥ   
    OSMemPut(&MEM_Buf,frame,&err);
  }
}

void power_cmd(FunctionalState NewState){
  if(NewState != DISABLE){
    //�򿪵�Դ
    mbus_power(ENABLE);
    relay_485(ENABLE);
  }else{
    //�رյ�Դ
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
  OS_ERR err;
  cjq_close();  //�ȹر����е�ͨ��
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
  cjq_isopen = 1;
  OSTmrStart(&TMR_CJQTIMEOUT,&err);
  return 1;
}

uint8_t cjq_close(){
  relay_1(DISABLE);
  relay_2(DISABLE);
  relay_3(DISABLE);
  relay_4(DISABLE);
  cjq_isopen = 0;
  return 1;
}

void cjq_timeout(void *p_tmr,void *p_arg){
  //�رյ�Դ
  mbus_power(DISABLE);
  relay_485(DISABLE);
  //�ر�ͨ��
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