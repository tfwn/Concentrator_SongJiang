/************************************************************************************************
*                                   SRWF-6009-BOOT
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Uart.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        10/08/2015      Zhangxp         SRWF-6009-BOOT  Original Version
************************************************************************************************/

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Uart.h"


/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
UART_PARAM_STRUCT Uart1;


/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/
static bool Uart_InitIO(void);
static bool Uart_InitInt(void);
static bool Uart_CfgParam(UART_NAME_ENUM UartName, UART_BAUDRATE_ENUM UartBaudrate, \
                            UART_DATA_BITS_ENUM UartDataBits, UART_PARITY_ENUM UartParity, UART_STOP_BITS_ENUM UartStopBits);


/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Uart_Init
* Decription   : ���ڳ�ʼ������
* Input        : None
* Output       : None
* Others       : ����0
************************************************************************************************/
void Uart_Init(void)
{
    Uart1.RxLen = 0;
    Uart1.TxLen = 0;
    Uart1.RxTimer = 0;
    Uart1.TxCount = 0;

    // Enable USART1 Clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    Uart_InitIO();
    Uart_CfgParam(UART_NAME_USART1, UART_BAUDRATE_115200, UART_DATA_BITS_8, UART_PARITY_EVEN, UART_STOP_BITS_1);
    Uart_InitInt(); // USART1�ж�;
}

/************************************************************************************************
* Function Name: Uart_InitIO
* Decription   : ���ڹܽų�ʼ��
* Input        : None
* Output       : None
* Others       :
************************************************************************************************/
static bool Uart_InitIO(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Configure USART1_Rx as input floating */
    GPIO_InitStructure.GPIO_Pin = USART1_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, & GPIO_InitStructure);

    /* Configure USART1_Tx as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = USART1_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, & GPIO_InitStructure);

    return TRUE;
}

/************************************************************************************************
* Function Name: Uart_CfgParam
* Decription   : ����ͨѶ��������
* Input        : None
* Output       : None
* Others       :
************************************************************************************************/
static bool Uart_CfgParam(UART_NAME_ENUM UartName, UART_BAUDRATE_ENUM UartBaudrate, UART_DATA_BITS_ENUM UartDataBits, UART_PARITY_ENUM UartParity, UART_STOP_BITS_ENUM UartStopBits)
{
    USART_InitTypeDef USART_InitStructure;

    USART_DeInit((USART_TypeDef *)UartName);

    USART_InitStructure.USART_BaudRate = UartBaudrate;

    USART_InitStructure.USART_WordLength = UartDataBits;

    USART_InitStructure.USART_Parity = UartParity;

    USART_InitStructure.USART_StopBits = UartStopBits;

    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init((USART_TypeDef *)UartName, &USART_InitStructure);

    USART_Cmd((USART_TypeDef *)UartName, ENABLE);

    return TRUE;
}

/************************************************************************************************
* Function Name: Uart_InitInt
* Decription   : �����жϳ�ʼ��
* Input        : None
* Output       : None
* Others       :
************************************************************************************************/
static bool Uart_InitInt(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Enable the USART1 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    return TRUE;
}

/************************************************************************************************
* Function Name: Uart_StartTx
* Decription   : ��ʼ��������
* Input        : None
* Output       : None
* Others       : ����1
************************************************************************************************/
void Uart_StartTx(void)
{
    if (Uart1.TxLen > 0)
    {
        Uart1.TxCount = 0;
        USART1->DR = Uart1.TxBuf[Uart1.TxCount ++];
        UART1_TX_INT_ENABLE;
    }
}

/************************************************************************************************
* Function Name: Uart_IntHandler
* Decription   : ����1�жϴ�����
* Input        : None
* Output       : None
* Others       : ����1
************************************************************************************************/
void Uart_IntHandler(void)
{
    uint8 tmp;

    // ���ڽ������ݹ���
    if (USART_GetITStatus( USART1, USART_IT_RXNE ) != RESET)
    {
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
        tmp = USART1->DR;
        if (Uart1.RxLen < UART_BUF_SIZE)
        {
            Uart1.RxBuf[Uart1.RxLen ++] = tmp;
            Uart1.RxTimer = UART_RX_OVERTIME;
        }
        if (USART_GetFlagStatus(USART1, USART_FLAG_PE) != RESET)
        {
            // У�����-�������У�������Ҫ�ȶ�SR,�ٶ�DR�Ĵ��� �������˱�־λ
            USART_ClearFlag(USART1, USART_FLAG_NE);       //��SR
            USART_ReceiveData(USART1);                    //��DR
        }
    }

    // ���ڷ������ݹ���
    if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET)
    {
        USART_ClearITPendingBit(USART1, USART_IT_TXE);

        if (Uart1.TxCount < Uart1.TxLen)
        {
            USART1->DR= Uart1.TxBuf[Uart1.TxCount ++];
        }
        else
        {
            Uart1.TxLen = 0;
            Uart1.TxCount = 0;
            UART1_TX_INT_DISABLE;
        }
    }

    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET)
    {
        // ���-������������Ҫ�ȶ�SR,�ٶ�DR�Ĵ��� �������������жϵ�����
        USART_ClearFlag(USART1, USART_FLAG_ORE);          //��SR
        USART_ReceiveData(USART1);                        //��DR
    }

    if (USART_GetFlagStatus(USART1, USART_FLAG_NE) != RESET)
    {
        // ����-�����������������Ҫ�ȶ�SR,�ٶ�DR�Ĵ��� �������������жϵ�����
        USART_ClearFlag(USART1, USART_FLAG_NE);           //��SR
        USART_ReceiveData(USART1);                        //��DR
    }

    if (USART_GetFlagStatus( USART1, USART_FLAG_FE ) != RESET)
    {
        // ֡����-�������֡������Ҫ�ȶ�SR,�ٶ�DR�Ĵ��� �������������жϵ�����
        USART_ClearFlag(USART1, USART_FLAG_NE);           //��SR
        USART_ReceiveData(USART1);                        //��DR
    }
}


/**************************************End of file**********************************************/


