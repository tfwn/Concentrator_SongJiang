/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : SerialPort.c
* Description  : �˺������ڴ���5�����ں�1��Usb�˿ڵ������շ�
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/16/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define SERIALPORT_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Main.h"
#include "SerialPort.h"
#include "Gprs.h"
#include "Led.h"
#include "Usb_lib.h"

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Serialport_RxFilter
* Decription   : �˿ڽ������ݴ�����
* Input        : PortNo-���ں�,Ch-���յ��ֽ�
* Output       : ��
* Others       : ��
************************************************************************************************/
void Serialport_RxFilter(PORT_NO PortNo)
{
    uint16 i, j;
    PORT_BUF_FORMAT *bufPtr;

    bufPtr = (PORT_BUF_FORMAT *)(SerialPort.Port[PortNo].RxMemPtr);
    switch (PortNo) {
        case Usart_Debug:
        case Usart_Rf:
        case Uart_Gprs:
        case Uart_Rs485:
            if (bufPtr->Length >= 2 &&
                SYNCWORD1 == bufPtr->Buffer[bufPtr->Length - 2] &&
                SYNCWORD2 == bufPtr->Buffer[bufPtr->Length - 1]) {
                bufPtr->Length = 0;
                bufPtr->Property.FilterDone = TRUE;
            }
            break;
        case Usb_Port:
            i = (bufPtr->Length > USB_PKG_SIZE) ? (bufPtr->Length - USB_PKG_SIZE - 1) : 0;
            for ( ; i < bufPtr->Length - 1; i++) {
                if (SYNCWORD1 == bufPtr->Buffer[i] &&
                    SYNCWORD2 == bufPtr->Buffer[i + 1]) {
                    i += 2;
                    bufPtr->Property.FilterDone = TRUE;
                    break;
                }
            }
            if (TRUE == bufPtr->Property.FilterDone) {
                for (j = 0; i < bufPtr->Length; i++, j++) {
                    bufPtr->Buffer[j] = bufPtr->Buffer[i];
                }
                bufPtr->Length = j;
            }
            break;
        default:
            break;
    }
}

/************************************************************************************************
* Function Name: SerialPort_DelayProc
* Decription   : ���ж˿ڽ���������ʱ����
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void SerialPort_DelayProc(void)
{
    uint8 err;
    PORT_NO portNo;
    SERIALPORT_PORT *portPtr;

    for (portNo = Port_Start; portNo < Port_Total; portNo++) {
        portPtr = &SerialPort.Port[portNo];

        // �������ݳ�ʱ����
        if (SERIALPORT_TIMER_STOP == portPtr->RxTimer) {
        } else if (portPtr->RxTimer > 0) {
            portPtr->RxTimer--;
        } else {
            if (OS_ERR_NONE == OSMboxPost(portPtr->MboxRx, portPtr->RxMemPtr)) {
                portPtr->RxTimer = SERIALPORT_TIMER_STOP;
                OSFlagPost(GlobalEventFlag, (OS_FLAGS)(1 << portNo), OS_FLAG_SET, &err);
                portPtr->RxMemPtr = (void *)0;
            } else if (portPtr->MboxPostTime++ > MBOX_POST_MAX_TIME) {
                OSMemPut(LargeMemoryPtr, portPtr->RxMemPtr);
                portPtr->RxTimer = SERIALPORT_TIMER_STOP;
                portPtr->RxMemPtr = (void *)0;
            } else {
                portPtr->RxTimer = 20;
            }
        }

        // �������ݳ�ʱ����
        if (SERIALPORT_TIMER_STOP == portPtr->TxTimer) {
        } else if (portPtr->TxTimer > 0) {
            portPtr->TxTimer--;
        } else {
            if ((void *)0 != portPtr->TxMemPtr) {
                OSMemPut(LargeMemoryPtr, portPtr->TxMemPtr);
                portPtr->TxMemPtr = (void *)0;
            }
            portPtr->TxTimer = SERIALPORT_TIMER_STOP;
            if (Uart_Rs485 == portNo) {
                RS485_RX_ON();
            }
        }
    }
}

/************************************************************************************************
* Function Name: Uart_RxProc
* Decription   : Uart�������ݴ�����
* Input        : UartNo-���ں�,Ch-���յ��ֽ�
* Output       : ��
* Others       : ��
************************************************************************************************/
void Uart_RxProc(PORT_NO UartNo, uint8 Ch)
{
    uint8 err;
    SERIALPORT_PORT *portPtr = &SerialPort.Port[UartNo];
    PORT_BUF_FORMAT *bufPtr;

    if (UartNo >= Uart_Total || OSRunning == OS_FALSE) {
        return;
    }

    // ���û�н��ջ�����,������һ�����ջ�����
    if ((void *)0 == portPtr->RxMemPtr) {
        portPtr->RxMemPtr = OSMemGet(LargeMemoryPtr, &err);
        if (OS_ERR_NONE == err) {
            bufPtr = (PORT_BUF_FORMAT *)(portPtr->RxMemPtr);
            bufPtr->Length = 0;
            bufPtr->Property.PortNo = UartNo;
            bufPtr->Property.FilterDone = FALSE;
            portPtr->MboxPostTime = 0;
        }
    }

    // �н��ջ�������ֱ�ӽ�������
    if ((void *)0 != portPtr->RxMemPtr) {
        bufPtr = (PORT_BUF_FORMAT *)(portPtr->RxMemPtr);
        if (bufPtr->Length < MEM_LARGE_BLOCK_LEN - 3) {
            bufPtr->Buffer[bufPtr->Length++] = Ch;
            if (FALSE == bufPtr->Property.FilterDone) {
                Serialport_RxFilter(UartNo);
            }
        }
        portPtr->RxTimer = portPtr->RxOutTime;
    }
}

/************************************************************************************************
* Function Name: Uart_TxProc
* Decription   : Uart�������ݴ�����
* Input        : UartNo-���ں�,Ch-���͵��ֽ�ָ��
* Output       : True-������Ҫ����,False-���ݷ������
* Others       : ��
************************************************************************************************/
bool Uart_TxProc(PORT_NO UartNo, uint8 *Ch)
{
    SERIALPORT_PORT *portPtr = &SerialPort.Port[UartNo];
    PORT_BUF_FORMAT *bufPtr;

    if (UartNo >= Uart_Total || (void *)0 == portPtr->TxMemPtr) {
        return FALSE;
    }

    bufPtr = (PORT_BUF_FORMAT *)(portPtr->TxMemPtr);

    // ���ȫ�����ݶ��Ѿ��������,���ͷŷ��ͻ��������ڴ���
    if (portPtr->TxCounter >= bufPtr->Length) {
        OSMemPut(LargeMemoryPtr, portPtr->TxMemPtr);
        portPtr->TxMemPtr = (void *)0;
        portPtr->TxTimer = SERIALPORT_TIMER_STOP;
        return FALSE;
    } else {
        * Ch = bufPtr->Buffer[portPtr->TxCounter];
        portPtr->TxCounter++;
        portPtr->TxTimer = SERIALPORT_TX_MAX_TIME;
        return TRUE;
    }
}

/************************************************************************************************
* Function Name: Uart_IRQHandler
* Decription   : USART�����жϷ������
* Input        : USARTx-Uart�˿�,UartNo-������Ķ�Ӧ���ܺ���
* Output       : ��
* Others       : ��
************************************************************************************************/
void Uart_IRQHandler(USART_TypeDef *USARTx, PORT_NO UartNo)
{
    uint8 ch;

    OSIntEnter();
    Led_FlashTime(WORKSTATUS_LED, Delay50ms, Delay50ms, TRUE);
    if (USART_GetFlagStatus(USARTx, USART_FLAG_ORE) != RESET) {         // ע��!����ʹ��if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)���ж�
        USART_ReceiveData(USARTx);
    }

    if (USART_GetITStatus(USARTx, USART_IT_RXNE) != RESET) {            // ���ڽ����ж�,��������
        ch = USART_ReceiveData(USARTx);
        USART_ClearITPendingBit(USARTx, USART_IT_RXNE);
        Uart_RxProc(UartNo, ch);
    }

    if (USART_GetITStatus(USARTx, USART_IT_TXE) != RESET) {             // ���ݼĴ������ж�,���ڷ�������
        USART_ClearITPendingBit(USARTx, USART_IT_TXE);
        if (TRUE == Uart_TxProc(UartNo, &ch)) {
            USART_SendData(USARTx, ch);
        } else if (Uart_Rs485 != UartNo) {
            USART_ITConfig(USARTx, USART_IT_TXE, DISABLE);              // ���ͻ��������жϽ�ֹ
        }
    }

    if (USART_GetFlagStatus(USARTx, USART_FLAG_LBD) != RESET) {
        USART_ClearITPendingBit(USARTx, USART_IT_LBD);
    }

    if (USART_GetFlagStatus(USARTx, USART_FLAG_TC) != RESET) {
        USART_ClearITPendingBit(USARTx, USART_IT_TC);
        if (Uart_Rs485 == UartNo) {
            RS485_RX_ON();
            USART_ITConfig(USARTx, USART_IT_TXE, DISABLE);
        }
    }
    Led_FlashTime(WORKSTATUS_LED, Delay100ms, Delay3000ms, FALSE);
    OSIntExit();
}

/************************************************************************************************
* Function Name: USART1_IRQHandler
* Decription   : USART1�жϷ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void USART1_IRQHandler(void)
{
    Uart_IRQHandler(USART1, Usart_Debug);
}

/************************************************************************************************
* Function Name: USART2_IRQHandler
* Decription   : USART2�жϷ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void USART2_IRQHandler(void)
{
    Uart_IRQHandler(USART2, Usart_Ir);
}

/************************************************************************************************
* Function Name: USART3_IRQHandler
* Decription   : USART3�жϷ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void USART3_IRQHandler(void)
{
    Uart_IRQHandler(USART3, Usart_Rf);
}

/************************************************************************************************
* Function Name: UART4_IRQHandler
* Decription   : UART4�жϷ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void UART4_IRQHandler(void)
{
    Uart_IRQHandler(UART4, Uart_Rs485);
}

/************************************************************************************************
* Function Name: UART5_IRQHandler
* Decription   : UART5�жϷ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void UART5_IRQHandler(void)
{
    Uart_IRQHandler(UART5, Uart_Gprs);
}

/************************************************************************************************
* Function Name: Uart_ParameterInit
* Decription   : ���ڲ�����ʼ������
* Input        : UartNo-���ں�
*                Baudrate-������
* Output       : ��
* Others       : RxOutTime�����жϴ��ڽ��ս���,�����һ���ֽڽ�����Ϻ���ʱRxOutTimeʱ��,�����ʱ
*                ����û���յ��µ��ֽڼ���Ϊ���ݽ������
************************************************************************************************/
static void Uart_ParameterInit(PORT_NO UartNo, uint32 Baudrate)
{
    SERIALPORT_PORT *portPtr = &SerialPort.Port[UartNo];
    if (UartNo >= Uart_Total) {
        return;
    }

    portPtr->TxTimer = SERIALPORT_TIMER_STOP;
    portPtr->TxCounter = 0;
    portPtr->TxMemPtr = (void *)0;
    portPtr->MboxPostTime = 0;
    portPtr->RxTimer = SERIALPORT_TIMER_STOP;
    portPtr->RxOutTime = Baudrate > 9600 ? 10 : 20;
    portPtr->RxMemPtr = (void *)0;
    portPtr->MboxTx = (void *)0;
    portPtr->MboxRx = (void *)0;
}

/************************************************************************************************
* Function Name: Uart_Config
* Decription   : ���ڹ������ú���
* Input        : UartNo-���ں�
*                Parity-��ż����
*                DataBits-����λ
*                StopBits-ֹͣλ
*                Baudrate-������
* Output       : ��
* Others       : ��
************************************************************************************************/
static void Uart_Config(PORT_NO UartNo,
                         PARITY_TYPE Parity,
                         DATABITS_TYPE DataBits,
                         STOPBITS_TYPE StopBits,
                         uint32 Baudrate)
{
    USART_InitTypeDef USART_InitStruct;
    GPIO_InitTypeDef GPIO_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    switch (UartNo) {
        case Usart_Debug:
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);                      // Usart1����APB2ʱ�ӣ�����Usart����APB1ʱ��
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE); // ʹ��usart1ʱ��

            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;                                      // USART1_Tx PA9
            GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;                                // GPIO_Mode_AF_PP �����������,�޸�Ϊ���ÿ�©���
            GPIO_Init(GPIOA, &GPIO_InitStruct);
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;                                     // USART1_Rx PA10
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;                          // ��ͨ����ģʽ(����)
            GPIO_Init(GPIOA, &GPIO_InitStruct);

            USART_InitStruct.USART_BaudRate = Baudrate;                                 // ������
            USART_InitStruct.USART_WordLength = DataBits;                               // λ��
            USART_InitStruct.USART_StopBits = StopBits;                                 // ֹͣλ��
            USART_InitStruct.USART_Parity = Parity;                                     // ��żУ��
            USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
            USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;// ������Ϊ��
            USART_Init(USART1, &USART_InitStruct);                                      // ���ô��ڲ���

            NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;                              // Usart1�ж�����
            NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
            NVIC_InitStruct.NVIC_IRQChannelSubPriority = 3;
            NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
            NVIC_Init(&NVIC_InitStruct);

            USART_Cmd(USART1, ENABLE);
            USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);                              // ��ʹ�ܽ����ж�
            USART_ITConfig(USART1, USART_IT_TXE, DISABLE);                              // �Ƚ�ֹ�����ж�,����ʹ�ܷ����жϣ�����ܻ��ȷ�0x00

            break;

        case Usart_Ir:
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE); // ʹ��Usart2ʱ��

            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;                                      // USART2_Tx PA2
            GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;                                // �����������
            GPIO_Init(GPIOA, &GPIO_InitStruct);
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3;                                      // USART2_Rx PA3
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;                          // ��ͨ����ģʽ(����)
            GPIO_Init(GPIOA, &GPIO_InitStruct);

            USART_InitStruct.USART_BaudRate = Baudrate;                                 // ������
            USART_InitStruct.USART_WordLength = DataBits;                               // λ��
            USART_InitStruct.USART_StopBits = StopBits;                                 // ֹͣλ��
            USART_InitStruct.USART_Parity = Parity;                                     // ��żУ��
            USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
            USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;// ������Ϊ��
            USART_Init(USART2, &USART_InitStruct);                                      // ���ô��ڲ���

            NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;                              // Usart2�ж�����
            NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
            NVIC_InitStruct.NVIC_IRQChannelSubPriority = 5;
            NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
            NVIC_Init(&NVIC_InitStruct);

            USART_Cmd(USART2, ENABLE);
            USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);                              // ��ʹ�ܽ����ж�
            USART_ITConfig(USART2, USART_IT_TXE, DISABLE);                              // �Ƚ�ֹ�����ж�,����ʹ�ܷ����жϣ�����ܻ��ȷ�0x00

            break;

        case Usart_Rf:
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE); // ʹ��Usart3ʱ��

            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;                                     // USART3_Tx PB10
            GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_InitStruct.GPIO_Mode =  GPIO_Mode_AF_PP;                               // GPIO_Mode_AF_PP ����������� �޸�Ϊ���ÿ�©���
            GPIO_Init(GPIOB, &GPIO_InitStruct);
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;                                     // USART3_Rx PB11
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;                          // ��ͨ����ģʽ(����)
            GPIO_Init(GPIOB, &GPIO_InitStruct);

            USART_InitStruct.USART_BaudRate = Baudrate;                                 // ������
            USART_InitStruct.USART_WordLength = DataBits;                               // λ��
            USART_InitStruct.USART_StopBits = StopBits;                                 // ֹͣλ��
            USART_InitStruct.USART_Parity = Parity;                                     // ��żУ��
            USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
            USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;// ������Ϊ��
            USART_Init(USART3, &USART_InitStruct);                                      // ���ô��ڲ���

            NVIC_InitStruct.NVIC_IRQChannel = USART3_IRQn;                              // Usart3�ж�����
            NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
            NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
            NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
            NVIC_Init(&NVIC_InitStruct);

            USART_Cmd(USART3, ENABLE);
            USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);                              // ��ʹ�ܽ����ж�
            USART_ITConfig(USART3, USART_IT_TXE, DISABLE);                              // �Ƚ�ֹ�����ж�,����ʹ�ܷ����жϣ�����ܻ��ȷ�00

            break;

        case Uart_Rs485:
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE); // ʹ��Uart4ʱ��

            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;                                     // UART4_Tx PC10
            GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;                                // GPIO_Mode_AF_PP ����������� �޸�Ϊ���ÿ�©���
            GPIO_Init(GPIOC, &GPIO_InitStruct);
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;                                     // UART4_Rx PC11
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;                          // ��ͨ����ģʽ(����)
            GPIO_Init(GPIOC, &GPIO_InitStruct);

            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
            GPIO_Init(GPIOC, &GPIO_InitStruct);                                         // ����485_RE����
            RS485_RX_ON();

            USART_InitStruct.USART_BaudRate = Baudrate;                                 // ������
            USART_InitStruct.USART_WordLength = DataBits;                               // λ��
            USART_InitStruct.USART_StopBits = StopBits;                                 // ֹͣλ��
            USART_InitStruct.USART_Parity = Parity;                                     // ��żУ��
            USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
            USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;// ������Ϊ��
            USART_Init(UART4, &USART_InitStruct);                                       // ���ô��ڲ���

            NVIC_InitStruct.NVIC_IRQChannel = UART4_IRQn;                               // Uart4�ж�����
            NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
            NVIC_InitStruct.NVIC_IRQChannelSubPriority = 4;
            NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
            NVIC_Init(&NVIC_InitStruct);

            USART_Cmd(UART4, ENABLE);
            USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);                               // ��ʹ�ܽ����ж�
            USART_ITConfig(UART4, USART_IT_TXE, DISABLE);                               // �Ƚ�ֹ�����ж�,����ʹ�ܷ����жϣ�����ܻ��ȷ�00

            break;

        case Uart_Gprs:
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);                       // ʹ��Uart5ʱ��
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);

            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;                                     // UART5_Tx PC12
            GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;                                // GPIO_Mode_AF_PP �����������
            GPIO_Init(GPIOC, &GPIO_InitStruct);
            GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;                                      // UART5_Rx PD2
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;                          // ��ͨ����ģʽ(����)
            GPIO_Init(GPIOD, &GPIO_InitStruct);

            USART_InitStruct.USART_BaudRate = Baudrate;                                 // ������
            USART_InitStruct.USART_WordLength = DataBits;                               // λ��
            USART_InitStruct.USART_StopBits = StopBits;                                 // ֹͣλ��
            USART_InitStruct.USART_Parity = Parity;                                     // ��żУ��
            USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
            USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;// ������Ϊ��
            USART_Init(UART5, &USART_InitStruct);                                       // ���ô��ڲ���

            NVIC_InitStruct.NVIC_IRQChannel = UART5_IRQn;                               // Uart5�ж�����
            NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
            NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
            NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
            NVIC_Init(&NVIC_InitStruct);

            USART_Cmd(UART5, ENABLE);
            USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);                               // ��ʹ�ܽ����ж�
            USART_ITConfig(UART5, USART_IT_TXE, DISABLE);                               // �Ƚ�ֹ�����ж�,����ʹ�ܷ����жϣ�����ܻ��ȷ�00

            break;

        default:

            break;
    }

    // Uart��������
    Uart_ParameterInit(UartNo, Baudrate);
}

/************************************************************************************************
* Function Name: Usb_Config
* Decription   : Usb�˿����ú���
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Usb_Config(void)
{
    SERIALPORT_PORT *portPtr = &SerialPort.Port[Usb_Port];

    USB_Init();
    portPtr->TxTimer = SERIALPORT_TIMER_STOP;
    portPtr->TxCounter = 0;
    portPtr->TxMemPtr = (void *)0;
    portPtr->MboxPostTime = 0;
    portPtr->RxTimer = SERIALPORT_TIMER_STOP;
    portPtr->RxOutTime = USB_RX_OUTTIME;
    portPtr->RxMemPtr = (void *)0;
    portPtr->MboxTx = (void *)0;
    portPtr->MboxRx = (void *)0;
}

/************************************************************************************************
* Function Name: Usb_RxProc
* Decription   : Usb�˿ڽ������ݴ�����
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Usb_RxProc(void)
{
    uint8 len;
    SERIALPORT_PORT *portPtr = &SerialPort.Port[Usb_Port];
    PORT_BUF_FORMAT *bufPtr;

    if (OSRunning == OS_FALSE) {
        return;
    }

    len = GetEPRxCount(ENDP1);
    Led_FlashTime(WORKSTATUS_LED, Delay50ms, Delay50ms, TRUE);

    // ���û�н��ջ�����,������һ�����ջ�����
    if ((void *)0 == portPtr->RxMemPtr) {
        if ((void *)0 != (portPtr->RxMemPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
            bufPtr = (PORT_BUF_FORMAT *)(portPtr->RxMemPtr);
            bufPtr->Length = 0;
            bufPtr->Property.PortNo = Usb_Port;
            bufPtr->Property.FilterDone = FALSE;
            portPtr->MboxPostTime = 0;
        }
    }

    // �н��ջ�������ֱ�ӽ�������
    if ((void *)0 != portPtr->RxMemPtr) {
        bufPtr = (PORT_BUF_FORMAT *)(portPtr->RxMemPtr);
        if (bufPtr->Length < MEM_LARGE_BLOCK_LEN - 3 - len) {
            PMAToUserBufferCopy(bufPtr->Buffer + bufPtr->Length, ENDP1_RXADDR, len);
            bufPtr->Length += len;
            if (FALSE == bufPtr->Property.FilterDone) {
                Serialport_RxFilter(Usb_Port);
            }
        }
        SetEPRxValid(ENDP1);
        portPtr->RxTimer = portPtr->RxOutTime;
    }
    Led_FlashTime(WORKSTATUS_LED, Delay100ms, Delay3000ms, FALSE);
}

/************************************************************************************************
* Function Name: Usb_TxProc
* Decription   : Usb�˿ڷ������ݴ�����
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Usb_TxProc(void)
{
    SERIALPORT_PORT *portPtr = &SerialPort.Port[Usb_Port];
    PORT_BUF_FORMAT *bufPtr = (PORT_BUF_FORMAT *)(portPtr->TxMemPtr);

    if ((void *)0 == portPtr->TxMemPtr) {
        return;
    }

    Led_FlashTime(WORKSTATUS_LED, Delay50ms, Delay50ms, TRUE);
    if (EP_TX_NAK == GetEPTxStatus(ENDP1)) {
        portPtr->TxCounter += USB_PKG_SIZE;
        if (portPtr->TxCounter >= bufPtr->Length) {
            OSMemPut(LargeMemoryPtr, portPtr->TxMemPtr);
            portPtr->TxMemPtr = (void *)0;
            portPtr->TxTimer = SERIALPORT_TIMER_STOP;
            Led_FlashTime(WORKSTATUS_LED, Delay100ms, Delay3000ms, FALSE);
            return;
        } else {
            UserToPMABufferCopy(bufPtr->Buffer + portPtr->TxCounter, GetEPTxAddr(ENDP1), USB_PKG_SIZE);
            SetEPTxCount(ENDP1, USB_PKG_SIZE);
            SetEPTxValid(ENDP1);
            portPtr->TxTimer = SERIALPORT_TX_MAX_TIME;
        }
    }
    Led_FlashTime(WORKSTATUS_LED, Delay100ms, Delay3000ms, FALSE);
}

/************************************************************************************************
* Function Name: SerialPort_Init
* Decription   : ���ж˿ڳ�ʼ������
* Input        : ��
* Output       : ��
* Others       : ���пڰ���5��Uart�ں�1��Usb��
************************************************************************************************/
void SerialPort_Init(void)
{
    Uart_Config(Usart_Debug, Parity_Even, DataBits_9, StopBits_1, 115200);          // Debug���ڳ�ʼ��
    Uart_Config(Usart_Ir, Parity_Even, DataBits_9, StopBits_1, 9600);               // IR���ڳ�ʼ��
    Uart_Config(Usart_Rf, Parity_Even, DataBits_9, StopBits_1, 9600);               // RF���ڳ�ʼ��
    Uart_Config(Uart_Rs485, Parity_Even, DataBits_9, StopBits_1, 9600);             // RS485���ڳ�ʼ��
    Uart_Config(Uart_Gprs, Parity_None, DataBits_8, StopBits_1, 115200);            // GPRS���ڳ�ʼ��
    Usb_Config();                                                                   // USB�˿ڳ�ʼ��
}

/************************************************************************************************
* Function Name: SerialPort_Task
* Decription   : �������ݴ���������,ֻ�������¼�
* Input        : *p_arg-����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void SerialPort_Task(void *p_arg)
{
    uint8 retry, err, *msg;
    PORT_NO portNo;
    OS_FLAGS eventFlag;
    SERIALPORT_PORT *portPtr;

    (void)p_arg;

    while (TRUE ) {
        eventFlag = OSFlagPend(GlobalEventFlag, (OS_FLAGS)SERIALPORT_TX_EVENT_FILTER, (OS_FLAG_WAIT_SET_ANY | OS_FLAG_CONSUME), TIME_DELAY_MS(5000), &err);

        // �õ���Ҫ���͵Ĵ�������(��ʶλ�ڸ��ֽ�)
        eventFlag >>= SERIALPORT_TX_FLAG_OFFSET;
        for (portNo = Port_Start; portNo < Port_Total; portNo++) {
            portPtr = &SerialPort.Port[portNo];
            if (eventFlag & (1 << portNo)) {
                msg = OSMboxAccept(portPtr->MboxTx);
                if ((void *)0 != msg) {
                    // ������ȳ��������
                    if (((PORT_BUF_FORMAT *)msg)->Length > MEM_LARGE_BLOCK_LEN - 3) {
                        OSMemPut(LargeMemoryPtr, msg);
                        continue;
                    }
                    // ������ͻ�������������,����������3��
                    retry = 3;
                    while ((void *)0 != portPtr->TxMemPtr && retry) {
                        retry -= 1;
                        OSTimeDlyHMSM(0, 0, 0, 200);
                    }
                    if (0 == retry) {
                        OSMemPut(LargeMemoryPtr, msg);
                    } else {
                        portPtr->TxMemPtr = msg;
                        portPtr->TxCounter = 0;
                        portPtr->TxTimer = SERIALPORT_TX_MAX_TIME;
                        switch (portNo) {
                            case Usart_Debug:
                                USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
                                break;
                            case Usart_Ir:
                                USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
                                break;
                            case Usart_Rf:
                                USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
                                break;
                            case Uart_Rs485:
                                RS485_TX_ON();
                                USART_ITConfig(UART4, USART_IT_TXE, ENABLE);
                                break;
                            case Uart_Gprs:
                                USART_ITConfig(UART5, USART_IT_TXE, ENABLE);
                                break;
                            case Usb_Port:
                                UserToPMABufferCopy(((PORT_BUF_FORMAT *)(portPtr->TxMemPtr))->Buffer, GetEPTxAddr(ENDP1), USB_PKG_SIZE);
                                SetEPTxCount(ENDP1, USB_PKG_SIZE);
                                SetEPTxValid(ENDP1);
                                break;
                            default:
                                OSMemPut(LargeMemoryPtr, msg);
                                break;
                        }
                    }
                }
            }
        }
    }
}

/***************************************End of file*********************************************/

