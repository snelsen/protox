/**
  ******************************************************************************
  * @file    usbd_cdc_vcp.c
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    19-March-2012
  * @brief   Generic media access Layer.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED 
#pragma     data_alignment = 4 
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_vcp.h"
#include "usb_conf.h"
#include "fifo.h"
#include <stdio.h>

#define FIFO_BUFF_SIZE  (512)

fifo_t usbRxFifo;
fifo_t usbTxFifo;

static uint8_t inBuff[FIFO_BUFF_SIZE];
static uint8_t outBuff[FIFO_BUFF_SIZE];

extern uint8_t cdcTxBuff[CDC_DATA_MAX_PACKET_SIZE];
extern volatile uint32_t usbTxInProgress;

extern USB_OTG_CORE_HANDLE  USB_OTG_dev;

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
LINE_CODING linecoding =
  {
    115200, /* baud rate*/
    0x00,   /* stop bits-1*/
    0x00,   /* parity - none*/
    0x08    /* nb. of bits 8*/
  };

/* Private function prototypes -----------------------------------------------*/
static uint16_t VCP_Init     (void);
static uint16_t VCP_DeInit   (void);
static uint16_t VCP_Ctrl     (uint32_t Cmd, uint8_t* Buf, uint32_t Len);
uint16_t VCP_DataTx   (uint8_t* Buf, uint32_t Len);
static uint16_t VCP_DataRx   (uint8_t* Buf, uint32_t Len);

CDC_IF_Prop_TypeDef VCP_fops = 
{
  VCP_Init,
  VCP_DeInit,
  VCP_Ctrl,
  VCP_DataTx,
  VCP_DataRx
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  VCP_Init
  *         Initializes the Media on the STM32
  * @param  None
  * @retval Result of the opeartion (USBD_OK in all cases)
  */
static uint16_t VCP_Init(void)
{
  // Setup fifos
  fifoInit(&usbRxFifo, FIFO_BUFF_SIZE, inBuff);
  fifoInit(&usbTxFifo, FIFO_BUFF_SIZE, outBuff);
  return USBD_OK;
}

/**
  * @brief  VCP_DeInit
  *         DeInitializes the Media on the STM32
  * @param  None
  * @retval Result of the opeartion (USBD_OK in all cases)
  */
static uint16_t VCP_DeInit(void)
{
  return USBD_OK;
}


/**
  * @brief  VCP_Ctrl
  *         Manage the CDC class requests
  * @param  Cmd: Command code            
  * @param  Buf: Buffer containing command data (request parameters)
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the opeartion (USBD_OK in all cases)
  */
static uint16_t VCP_Ctrl (uint32_t Cmd, uint8_t* Buf, uint32_t Len)
{ 
  switch (Cmd)
  {
  case SEND_ENCAPSULATED_COMMAND:
    /* Not  needed for this driver */
    break;

  case GET_ENCAPSULATED_RESPONSE:
    /* Not  needed for this driver */
    break;

  case SET_COMM_FEATURE:
    /* Not  needed for this driver */
    break;

  case GET_COMM_FEATURE:
    /* Not  needed for this driver */
    break;

  case CLEAR_COMM_FEATURE:
    /* Not  needed for this driver */
    break;

  case SET_LINE_CODING:
    /* Not  needed for this driver */
    break;

  case GET_LINE_CODING:
    /* Not  needed for this driver */
    break;

  case SET_CONTROL_LINE_STATE:
    /* Not  needed for this driver */
    break;

  case SEND_BREAK:
    /* Not  needed for this driver */
    break;    
    
  default:
    break;
  }

  return USBD_OK;
}

/**
  * @brief  VCP_DataTx
  *         CDC received data to be send over USB IN endpoint are managed in 
  *         this function.
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the opeartion: USBD_OK if all operations are OK else VCP_FAIL
  */
uint16_t VCP_DataTx (uint8_t* Buf, uint32_t Len)
{
  uint32_t i = 0;
  while (i < Len) {
    fifoPush(&usbTxFifo, *(Buf + i++));
  }

  if(!usbTxInProgress) {
    uint32_t txLen = fifoSize(&usbTxFifo);
    uint8_t *pBuf = cdcTxBuff;

    if(txLen) {
      
      if(txLen > CDC_DATA_MAX_PACKET_SIZE) {
        txLen = CDC_DATA_MAX_PACKET_SIZE;
      }

      usbTxInProgress = 1;

      for(uint32_t x = 0; x < txLen; x++) {
        *pBuf++ = fifoPop(&usbTxFifo);
      }

      /* Prepare the available data buffer to be sent on IN endpoint */
      DCD_EP_Tx (&USB_OTG_dev,
                 CDC_IN_EP,
                 (uint8_t*)cdcTxBuff,
                 txLen);
    } else {
      usbTxInProgress = 0;
    } 
  }
  
  return USBD_OK;
}

/**
  * @brief  VCP_DataRx
  *         Data received over USB OUT endpoint are sent over CDC interface 
  *         through this function.
  *           
  *         @note
  *         This function will block any OUT packet reception on USB endpoint 
  *         untill exiting this function. If you exit this function before transfer
  *         is complete on CDC interface (ie. using DMA controller) it will result 
  *         in receiving more data while previous ones are still not sent.
  *                 
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the opeartion: USBD_OK if all operations are OK else VCP_FAIL
  */
static uint16_t VCP_DataRx(uint8_t* Buf, uint32_t Len) {
  for (uint32_t i = 0; i < Len; i++) {
    fifoPush(&usbRxFifo, *(Buf + i));
  }

  return USBD_OK;
}

//
// Retarget read/write to use usb!
// Found in share/gcc-arm-none-eabi/samples/src/retarget/retarget.c
//
int _write (int fd, char *ptr, int len)
{
  //
  // If planning on supporting both serial and usb-serial, check fd here!
  //
  VCP_DataTx((uint8_t *)ptr, len);
  return len;
}

int _read (int fd, char *ptr, int len)
{
  int readChars = 0;
  
  //
  // If planning on supporting both serial and usb-serial, check fd here!
  //
  while(fifoSize(&usbRxFifo) && len--) {
    *ptr++ = fifoPop(&usbRxFifo);
  }
  
  return readChars;
}

void _ttywrch(int ch) {
  VCP_DataTx((uint8_t *)&ch, 1);
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
