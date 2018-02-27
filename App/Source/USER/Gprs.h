/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Bsp.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/14/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  GPRS_H
#define  GPRS_H


#ifdef   GPRS_GLOBALS
#define  GPRS_EXT
#else
#define  GPRS_EXT  extern
#endif

/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
#define GPRS_POWER_OFF()                GPIO_ResetBits(GPIOC, GPIO_Pin_9)       // Gprsģ���Դ��������PC9:0-OFF
#define GPRS_POWER_ON()                 GPIO_SetBits(GPIOC, GPIO_Pin_9)         // Gprsģ���Դ��������PC9:1-ON

#define GPRS_PWRKEY_DOWN()              GPIO_SetBits(GPIOB, GPIO_Pin_8)         // Gprsģ���Դ��������PB8:1-DOWN
#define GPRS_PWRKEY_UP()                GPIO_ResetBits(GPIOB, GPIO_Pin_8)       // Gprsģ���Դ����̧��PB8:0-UP

#define GPRS_EMERG_OFF()                GPIO_ResetBits(GPIOB, GPIO_Pin_9)       // Gprsģ�鼱ͣ����ʼ�չ�PB9

#define GPRS_STATUS()                   GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5)// Gprsģ��״̬PB5:1-ON 0-OFF

#define GPRS_NORMALWORKTIME             (30 * 60 * 1000)                        // ����������ʱ��ֵ
#define GPRS_CONTINUERESTARTTIME        10                                      // ����Gprsģ�����������Ĵ���
#define GPRS_CONTINUECLOSEPDPTIME       10                                      // ����Gprsģ�������ر�PDP�Ĵ���
#define GPRS_POWERDOWNTIME              10                                      // ������Ӷ�ʧ�ܺ�,ģ��ػ���ʱ��(����)������

#define GPRS_FLAG_FILTER                (FLAG_UART_GPRS_RX | FLAG_GPRS_TX | FLAG_HEATBEAT_TIMER | FLAG_LOGOFF_EVENT | FLAG_GPRS_UPDATA_RTC | FLAG_GPRS_RESIGN_IN)

#define GPRS_IP_CLOSE                   "CLOSE"
#define GPRS_LOCAL_PORT                 2020
#define GPRS_LED                        Led2

#define GPRS_HEATBEAT_CMD               0x01                                    // ������ָ��
#define GPRS_LOGOFF_CMD                 0x02                                    // ע��ָ��
#define GPRS_DATAPKG_CMD                0x09                                    // ���ݰ�ָ��

#define GPRS_DATA_MAX_DATA              (MEM_LARGE_BLOCK_LEN - 50)              // Gprsÿ������ܹ����͵��ֽ���,50����GPRS������ݺ�Э��֡��������֮������ݳ���
#define GPRS_BACKUP_IP_CONNECT          3                                       // ���ӱ���IP��ַʱ�Ĵ���

/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/
// Gprs�������
typedef enum {
    GprsPowerOff = 0,                                       // Gprsģ��ص�Դ
    GprsPowerOn,                                            // Gprsģ�鿪��Դ
    GprsRestart,                                            // Gprsģ����������
    GprsConfig,                                             // Gprsģ���������
    GprsGsmStatus,                                          // Gprsģ������״̬
    GprsDataFormat,                                         // Gprs���ݸ�ʽ����
    GprsOpenPdp,                                            // Gprsģ���PDP����
    GprsOpenTcp,                                            // Gprsģ���TCP����
    GprsClosePdp,                                           // Gprsģ��ر�PDP����
    GprsCloseTcp,                                           // Gprsģ��ر�TCP����
    GprsReceiveData,                                        // Gprsģ���������
    GprsTransmitData,                                       // Gprsģ�鷢������
    GprsCheckConnect,                                       // Gprsģ�鷢��������
	GprsSignIn,												// Gprsģ�鷢�͵�¼֡
    GprsUpdataRtc,                                       	// Gprsģ�鷢�� RTC Уʱ
	GprsLogoffConnect,                                      // Gprsģ��ע������
    GprsConnectIdleStatus,                                  // Gprs�������Ҵ��ڿ���״̬
} GPRS_CMD;

// Gprsģ������
typedef enum {
    Quectel_M35 = 0,                                        // ��Զ2G GSMģ��
    Quectel_Ec20,                                           // ��Զ4Gȫ��ͨģ��
    Unknow_Type
} GPRS_MODULE_TYPE;

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/
// Gprs�����ṹ���������
typedef struct {
    GPRS_CMD Cmd;                                           // Gprs����
    GPRS_MODULE_TYPE ModuleType;                            // Gprsģ������
    uint8 RestartCount;                                     // Gprs�������Ĵ���������
    uint8 ClosePDPCount;                                    // Gprs�ر�PDP�Ĵ���������
    uint8 ConnectCount;                                     // Gprs���Ӽ�����
    uint8 Online: 1;                                        // Gprs���߱�ʶ
    uint8 HeartbeatRetryTime: 7;                            // Gprs������ʧ�����Դ���
    uint16 HeartbeatInterval;                               // Gprs���������
    uint32 WorkTime;                                        // Gprsģ��������Ĺ���ʱ��
    uint8 LocalIp[4];                                       // Gprs����IP
    OS_EVENT *MboxTx;                                       // Gprs��������
    OS_EVENT *MboxRx;                                       // Gprs��������
    OS_STK Task_Stk[TASK_GPRS_STK_SIZE];                    // Gprs�����ջ
} GPRS_PARAM;

/************************************************************************************************
*                                 Variable Declare Section
************************************************************************************************/
// Gprs�����������
GPRS_EXT GPRS_PARAM Gprs;

/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
GPRS_EXT uint8 Gprs_GetCSQ(void);
GPRS_EXT uint8 Gprs_GetIMSI(uint8 *BufPtr);
GPRS_EXT uint8 Gprs_GetGMM(uint8 *BufPtr);
GPRS_EXT void Gprs_Init(void);
GPRS_EXT void Gprs_Task(void *p_arg);
GPRS_EXT void Gprs_OutputDebugMsg(bool NeedTime, uint8 *StrPtr);


#endif
/***************************************End of file*********************************************/

