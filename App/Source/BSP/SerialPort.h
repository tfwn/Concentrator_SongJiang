/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : SerialPort.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/16/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  SERIALPORT_H
#define  SERIALPORT_H

#ifdef   SERIALPORT_GLOBALS
#define  SERIALPORT_EXT
#else
#define  SERIALPORT_EXT  extern
#endif

/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
#define SERIALPORT_TIMER_STOP                 0xFF                          // ��ʱ��ֹͣ����
#define SERIALPORT_TX_FLAG_OFFSET             8U                            // ���ڷ������ݱ�ʶƫ����
#define SERIALPORT_TX_EVENT_FILTER            (FLAG_USART_DEBUG_TX |      \
                                               FLAG_USART_IR_TX |         \
                                               FLAG_USART_RF_TX |         \
                                               FLAG_UART_RS485_TX |       \
                                               FLAG_UART_GPRS_TX |        \
                                               FLAG_USB_TX)

#define WORKSTATUS_LED                        Led1
#define USB_RX_OUTTIME                        50                            // Usb������ʱ�ȴ�ʱ��
#define USB_PKG_SIZE                          64                            // Usb�շ�ʱÿ�����ֽ���
#define MBOX_POST_MAX_TIME                    5                             // �������䷢��ʧ�ܺ�������Դ���
#define SERIALPORT_TX_MAX_TIME                200                           // �������ݱ��������ʱ����ڷ��ͳ�ȥ

#define RS485_TX_ON()                         GPIO_SetBits(GPIOC, GPIO_Pin_7)
#define RS485_RX_ON()                         GPIO_ResetBits(GPIOC, GPIO_Pin_7)


/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/
// ����ʹ�õ�USART�ں�UART��,����1 2 3ΪUSART,4 5ΪUART,�˴�Ҫ��main.h�����еĶ�Ӧ����һ��
typedef enum {
    Port_Start = 0,                                                         // �˿ڿ�ʼ���
    Usart_Debug = Port_Start,                                               // Debug����
    Usart_Ir,                                                               // Irͨ�Ŵ���
    Usart_Rf,                                                               // Rfģ���
    Uart_Rs485,                                                             // Rs485�ӿ�
    Uart_Gprs,                                                              // Gprsģ��ӿ�,������ΪGprsֱ���յ�������
    Uart_Total,                                                             // Uart�˿�����
    Usb_Port = Uart_Total,                                                  // Usb�˿�
    Port_Total,                                                             // �˿�����
    Gprs_Port = Port_Total,                                                 // Gprs�˿�,������ΪGprs���ݽ���������
    End_Port
} PORT_NO;

// ������żУ�����:��У�� �û��Զ��� żУ�� ��У��
typedef enum {
    Parity_Even = (uint16)0x0400,
    Parity_Odd = (uint16)0x0600,
    Parity_None = (uint16)0x0000
} PARITY_TYPE;

// �������ݳ�������:8λ 9λ(9λ������)
typedef enum {
    DataBits_8 = (uint16)0x0000,
    DataBits_9 = (uint16)0x1000
} DATABITS_TYPE;

// ֹͣλ����:1λ 0.5λ 2λ 1.5λ
typedef enum {
    StopBits_1 = (uint16)0x0000,
    StopBits_05 = (uint16)0x1000,
    StopBits_2 = (uint16)0x2000,
    StopBits_15 = (uint16)0x3000
} STOPBITS_TYPE;

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/
// �˿����ݰ�������
typedef struct {
    uint8 PortNo: 7;                                                        // Bit0-Bit6 ָ���Ǵ��ĸ������շ���
    uint8 FilterDone: 1;                                                    // Bit7 �Ƿ񾭹�����
} PORT_DATA_PROPERTY;

// �˿��շ������ض��Ľṹ����
typedef struct {
    uint16 Length;                                                          // ���ݳ���,�ó��Ȳ�����Property���Լ�����
    PORT_DATA_PROPERTY Property;                                            // ���ݰ�������
    uint8 Buffer[1];                                                        // ��������
} PORT_BUF_FORMAT;

// GPRS�˿��յ��ɽ������������ض��Ľṹ����
typedef struct {
    uint16 Length;                                                          // ���ݳ���,�ó��Ȳ�����Property���Լ�����
    uint8 Reserve[24];                                            			// һЩ��Ч����
    uint8 MacAddr[8];                                                       // MAC��ַ
	uint8 Buf[0];																// ��������
} GPRS_PORT_BUF_FORMAT;

// ���ж˿ڲ����Ľṹ�嶨��
typedef struct {
    uint8 TxTimer;                                                          // ���ͳ�ʱʱ��
    uint16 TxCounter;                                                       // ���͵ĸ���
    uint8 *TxMemPtr;                                                        // ���ͻ�����ָ��
    uint8 MboxPostTime;                                                     // �������䷢��ʱʧ�ܴ���
    uint8 RxTimer;                                                          // �������һ�������ʱ��ʱ��
    uint8 RxOutTime;                                                        // ���ճ�ʱʱ��
    uint8 *RxMemPtr;                                                        // ���ջ�����ָ��
    OS_EVENT *MboxTx;                                                       // ��������
    OS_EVENT *MboxRx;                                                       // ��������
} SERIALPORT_PORT;

// ���ж˿ڲ����ṹ���������
typedef struct {
    SERIALPORT_PORT Port[Port_Total];                                       // ���ж˿ڶ���
    OS_STK Task_Stk[TASK_SERIALPORT_STK_SIZE];                              // ���ж˿ڶ��������ջ
} SERIALPORT_PARAM;

/************************************************************************************************
*                            Variable Declare Section
************************************************************************************************/
// ���ж˿ڲ�������
SERIALPORT_EXT SERIALPORT_PARAM SerialPort;


/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
SERIALPORT_EXT void SerialPort_Init(void);
SERIALPORT_EXT void SerialPort_DelayProc(void);
SERIALPORT_EXT void Usb_RxProc(void);
SERIALPORT_EXT void Usb_TxProc(void);
SERIALPORT_EXT void SerialPort_Task(void *p_arg);


#endif
/***************************************End of file*********************************************/

