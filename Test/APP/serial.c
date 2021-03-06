
#include "stm32f10x_conf.h"
#include "os.h"
#include "serial.h"


extern OS_MEM MEM_ISR;

extern OS_Q Q_Server;     //存放通过485  或小无线发送来的数据
extern OS_SEM SEM_Send_Server_485;
extern OS_SEM SEM_Send_Server_RF;

//小无线 from gprs
void USART1_Handler(void){
  OS_ERR err;
  uint8_t rx_byte;
  uint8_t *mem_ptr;
  
  //receive the byte
  if(USART_GetFlagStatus(USART1,USART_FLAG_RXNE)){
    rx_byte = USART_ReceiveData(USART1);
    mem_ptr = OSMemGet(&MEM_ISR,&err);
    if(err == OS_ERR_NONE){
      *mem_ptr = rx_byte;
      OSQPost((OS_Q *)&Q_Server,
              (void *)mem_ptr,
              1,
              OS_OPT_POST_FIFO,
              &err);
      
      if(err != OS_ERR_NONE){
        //没有放进队列  放回MEMPool
        OSMemPut(&MEM_ISR,mem_ptr,&err);
      }
    }else{
      asm("NOP");
    }
  }
  
  //send the data
  if(USART_GetFlagStatus(USART1,USART_FLAG_TC)){
    
    USART_ClearITPendingBit(USART1,USART_IT_TC);
    OSSemPost(&SEM_Send_Server_RF,
              OS_OPT_POST_1,
              &err);
    
    if(err != OS_ERR_NONE){
      asm("NOP");
    }
  }
}

//485 from gprs/programmer
void USART3_Handler(void){
  OS_ERR err;
  uint8_t rx_byte;
  uint8_t *mem_ptr;
  
  //receive the byte
  if(USART_GetFlagStatus(USART3,USART_FLAG_RXNE)){
    rx_byte = USART_ReceiveData(USART3);
    mem_ptr = OSMemGet(&MEM_ISR,&err);
    if(err == OS_ERR_NONE){
      *mem_ptr = rx_byte;
      OSQPost((OS_Q *)&Q_Server,
              (void *)mem_ptr,
              1,
              OS_OPT_POST_FIFO,
              &err);
      
      if(err != OS_ERR_NONE){
        //没有放进队列  放回MEMPool
        OSMemPut(&MEM_ISR,mem_ptr,&err);
      }
    }else{
      asm("NOP");
    }
  }
  
  //send the data
  if(USART_GetFlagStatus(USART3,USART_FLAG_TC)){
    
    USART_ClearITPendingBit(USART3,USART_IT_TC);
    OSSemPost(&SEM_Send_Server_485,
              OS_OPT_POST_1,
              &err);
    
    if(err != OS_ERR_NONE){
      asm("NOP");
    }
  }
  
}

extern OS_Q Q_Slave;   //存放通过485 MBUS 发送过来的数据
extern OS_SEM SEM_Send_Slave;
//mbus  485
void USART2_Handler(void){
  OS_ERR err;
  uint8_t rx_byte;
  uint8_t *mem_ptr;
  
  //receive the byte
  if(USART_GetFlagStatus(USART2,USART_FLAG_RXNE) && USART_GetITStatus(USART2,USART_IT_RXNE)){
    rx_byte = USART_ReceiveData(USART2);
    mem_ptr = OSMemGet(&MEM_ISR,&err);
    if(err == OS_ERR_NONE){
      *mem_ptr = rx_byte;
      OSQPost((OS_Q *)&Q_Slave,
              (void *)mem_ptr,
              1,
              OS_OPT_POST_FIFO,
              &err);
      
      if(err != OS_ERR_NONE){
        //没有放进队列  放回MEMPool
        OSMemPut(&MEM_ISR,mem_ptr,&err);
      }
    }else{
      asm("NOP");
    }
  }
  
  //send the data
  if(USART_GetFlagStatus(USART2,USART_FLAG_TC)){
    
    USART_ClearITPendingBit(USART2,USART_IT_TC);
    OSSemPost(&SEM_Send_Slave,
              OS_OPT_POST_1,
              &err);
    
    if(err != OS_ERR_NONE){
      asm("NOP");
    }
  }
  
}

//mbus  485 USART2
ErrorStatus Send_Slave(uint8_t * data,uint16_t count){
  uint16_t i;
  CPU_TS ts;
  OS_ERR err;
  
  GPIO_SetBits(GPIOA,GPIO_Pin_4);  //底层表为485时  打开485的发送
  USART_ITConfig(USART2,USART_IT_TC,ENABLE);
  USART_ITConfig(USART2,USART_IT_RXNE,DISABLE);
  for(i = 0;i < 4;i++){
    err = OS_ERR_NONE;
    OSSemPend(&SEM_Send_Slave,
              500,
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    
    USART_SendData(USART2,0xFE);
  }
  
  for(i = 0;i < count;i++){
    err = OS_ERR_NONE;
    OSSemPend(&SEM_Send_Slave,
              500,
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    
    USART_SendData(USART2,*(data+i));
  }
  
  USART_ITConfig(USART2,USART_IT_TC,DISABLE);
  while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
  USART_GetFlagStatus(USART2,USART_FLAG_RXNE);
  USART_ReceiveData(USART2);
  USART_ITConfig(USART2,USART_IT_RXNE,ENABLE);
  GPIO_ResetBits(GPIOA,GPIO_Pin_4);  //关闭485的发送  恢复到接收
  
  return SUCCESS;
}

ErrorStatus Send_Server_485(uint8_t * data,uint16_t count){
  uint16_t i;
  CPU_TS ts;
  OS_ERR err;
  //send to 485
  GPIO_SetBits(GPIOB,GPIO_Pin_2);
  USART_ITConfig(USART3,USART_IT_TC,ENABLE);
  
  for(i = 0;i < 4;i++){
    err = OS_ERR_NONE;
    OSSemPend(&SEM_Send_Server_485,
              500,
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    USART_SendData(USART3,0xFE);
  }
  
  for(i = 0;i < count;i++){
    err = OS_ERR_NONE;
    OSSemPend(&SEM_Send_Server_485,
              500,
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    USART_SendData(USART3,*(data+i));
  }
  
  
  USART_ITConfig(USART3,USART_IT_TC,DISABLE);
  while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
  
  GPIO_ResetBits(GPIOB,GPIO_Pin_2);
  
  return SUCCESS;
}

ErrorStatus Send_Server_RF(uint8_t * data,uint16_t count){
  uint16_t i;
  CPU_TS ts;
  OS_ERR err;
  //send to RF
  USART_ITConfig(USART1,USART_IT_TC,ENABLE);
  
  for(i = 0;i < 4;i++){
    err = OS_ERR_NONE;
    OSSemPend(&SEM_Send_Server_RF,
              500,
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    USART_SendData(USART1,0xFE);
  }
  
  for(i = 0;i < count;i++){
    err = OS_ERR_NONE;
    OSSemPend(&SEM_Send_Server_RF,
              500,
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    USART_SendData(USART1,*(data+i));
  }
  
  
  USART_ITConfig(USART1,USART_IT_TC,DISABLE);
  while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
  
  return SUCCESS;
}


extern OS_FLAG_GRP FLAG_Event;
void OverLoad(void){
  OS_ERR err;
  if(EXTI_GetITStatus(EXTI_Line12) != RESET)
  {
    OSFlagPost(&FLAG_Event,
               OVERLOAD,
               OS_OPT_POST_FLAG_SET,
               &err);
    
    /* Clear the  EXTI line 0 pending bit */
    EXTI_ClearITPendingBit(EXTI_Line12);
  }
}