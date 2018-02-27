/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : DataHandle.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        08/12/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  DATAHANDLE_H
#define  DATAHANDLE_H


#ifdef   DATAHANDLE_GLOBALS
#define  DATAHANDLE_EXT
#else
#define  DATAHANDLE_EXT  extern
#endif


/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
// ������ȥ��������ʹ���·��������ݳ���,������(2)+���ı�ʶ(1)+�����(1)+������(1)+�豸����(1)+��������(1)+
//                                          ·����Ϣ(1)+�����ź�ǿ��(1)+�����ź�ǿ��(1)+У����(1)+������(1)
#define DATA_FIXED_AREA_LENGTH                      12

// ���ݴ������¼����˶���
#define DATAHANDLE_EVENT_FILTER                     (FLAG_USART_DEBUG_RX |          \
                                                     FLAG_USART_IR_RX |             \
                                                     FLAG_USART_RF_RX |             \
                                                     FLAG_UART_RS485_RX |           \
                                                     FLAG_USB_RX |                  \
                                                     FLAG_GPRS_RX |                 \
                                                     FLAG_DELAY_SAVE_TIMER |        \
                                                     FLAG_DATA_REPLENISH_TIMER |    \
                                                     FLAG_DATA_UPLOAD_TIMER |       \
													 FLAG_RTC_TIMING_TIMER  |	   \
                                                     FLAG_GPRS_DELAY_UPLOAD)
													 //FLAG_GPRS_UPDATA_RTC

// ����������ز�������
#define DATAREPLENISH_INTERVAL_TIME                 (5 * 60)        // ÿ�����ʱ��ͻ����Ƿ�������������(��λ:��)

// �����ϴ����ʱ�䶨��
#define DATAUPLOAD_INTERVAL_TIME                    (5 * 60)        // ÿ�����ʱ��ͻ����Ƿ������ݻ��ϴ�(��λ:��)
#undef ALWAYS_UPLOAD                                                // �����������������ݺ������Ͳ��������ϱ���,���屾��ʶ
#define GIVEUP_ABNORMAL_DATA                                        // ����������յ��Ƿ����ݴ���:���������ϴ���������

// Уʱ���ʱ�䶨��
#define RTCTIMING_INTERVAL_TIME                     (3 * 3600)      // ÿ�����ʱ��ͻ�ͷ�����У��ʱ��
// Gprs���ݴ���ȴ���ʱ��
#define GPRS_WAIT_ACK_OVERTIME                      TIME_DELAY_MS(10000)    // Gprs���ݷ�����ȴ�Ӧ��ĳ�ʱʱ��

#define MAX_ROUTER_NUM                              7
#define MAX_NODES_ONE_PKG                           10
#define MAX_DATA_HANDLE_TASK_NUM                    5
#define DELAYTIME_ONE_LAYER                         8000

// ��������ʱĬ�ϵ�Ӧ���ŵ�
#define DEFAULT_TX_CHANNEL                          3   // ������Ĭ�Ϸ����ŵ�
#define DEFAULT_RX_CHANNEL                          9   // ������Ĭ�Ͻ����ŵ�
#define CHANNEL_OFFSET                              16  // ֪ͨ����ģ������ŵ�ʱ,��Ҫ���ϴ�ֵ

// ���ı�ʶ����
//---------------------------------------------------------------------------------------------------------------
// ��������Ӫ�̱��벻����־����(Bit0) 0-��� 1-�����
#define XOR_OFF                                     0   // �����
#define XOR_ON                                      1   // ���

// �Ƿ���Ҫ��ִ����(Bit4) 0-�����ִ 1-��Ҫ��ִ
#define NONE_ACK                                    0   // ����Ҫ��ִ
#define NEED_ACK                                    1   // ��Ҫ��ִ

// ��������ܱ�־����(Bit5) 0-�Ǽ��� 1-����
#define ENCRYPT_OFF                                 0   // �Ǽ���
#define ENCRYPT_ON                                  1   // ����

// ���ݰ����Ͷ���(Bit6) 0-����֡ 1-Ӧ��֡
#define CMD_PKG                                     0   // ����֡
#define ACK_PKG                                     1   // Ӧ��֡

// ������������(Bit7) 0-���� 1-����
#define DOWN_DIR                                    0   // ����
#define UP_DIR                                      1   // ����

// �Ƿ�תԭ��·��
#define REVERSED                                    1   // ��ת·��
#define UNREVERSED                                  0   // ����ת·��

//gprs���ݻ��洦��ı�־
#define GPRS_DELAY_UP_CRCX	0xFF
#define GPRS_DELAY_UP_CRC0	0x53
#define GPRS_DELAY_UP_CRC1	0x48
#define GPRS_DELAY_UP_CRC2	0x52
#define GPRS_DELAY_UP_CRC3	0x51

//������ˮ��MAC��ַ����
#define MAC_ADDR_SIZE		8

/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/
// ����ͨ��ʹ�õ�����������
typedef enum {
    // �ͱ�߶�ͨ�ŵ�����
    Meter_ActiveReport_Data = 0x01,                     // ��������ϱ���ʱ�����򶳽�����
    Read_Meter_Data = 0x01,                             // ���������,�����ϴ�ʱ��������Ҳ�ô�ָ��
    Read_Meter_Freeze_Data = 0x02,                      // ����˶�������
	SwitchMonitor_Data = 0x08,							// ���ر������ģ����ز���������

	Read_Songjiang_Meter_data = 0x31,					//�ɽ���������
	Open_SwitchMonitor = 0x32,							//�򿪱��ʵʱ������
	Close_SwitchMonitor = 0x33,							//�رձ��ʵʱ������
	Open_GprsDebug = 0x34,								//�� gprs debug ��Ϣ
	Close_GprsDebug = 0x35,								//�ر� gprs debug ��Ϣ
	Write_SIM_NUM = 0x36, 								//���� SIM ����
	Open_GprsSoftUp = 0x37,								//�� gprs ������������
	Close_GprsSoftUp = 0x38, 							//�ر� gprs ������������
	Read_CONC_Channel_Param = 0x72,						// ���������ŵ����÷�ʽ
	Write_CONC_Channel_Param = 0x73,					// д�������ŵ����÷�ʽ
	Read_SIM_NUM = 0x74,								// ��ȡSIM ����


    // �ͼ������йص�ͨ������
    Read_CONC_Version = 0x40,                           // ���������汾��Ϣ
    Read_CONC_ID = 0x41,                                // ��������ID
    Write_CONC_ID = 0x42,                               // д������ID
    Read_CONC_RTC = 0x43,                               // ��������ʱ��
    Write_CONC_RTC = 0x44,                              // д������ʱ��
    Read_GPRS_Param = 0x45,                             // ��Gprs����
    Write_GPRS_Param = 0x46,                            // дGprs����
    Read_GPRS_RSSI = 0x47,                              // ��Gprs�ź�ǿ��
    Initial_CONC_Cmd = 0x48,                            // ��������ʼ��
    Read_CONC_Work_Param = 0x49,                        // ���������Ĺ�������
    Write_CONC_Work_Param = 0x4A,                       // д�������Ĺ�������
    CONC_RTC_Timing = 0x4B,                             // ����������Уʱ
    Restart_CONC_Cmd = 0x4C,                            // ��������������
    Data_Forward_Cmd = 0x4D,                            // ����������ת��ָ��

    // ��ߵ������������
    Read_Meter_Total_Number = 0x50,                     // ����ߵ�����������
    Read_Meters_Doc_Info = 0x51,                        // ��ȡ��ߵ�����Ϣ
    Write_Meters_Doc_Info = 0x52,                       // д���ߵ�����Ϣ
    Delete_Meters_Doc_Info = 0x53,                      // ɾ����ߵ�����Ϣ
    Modify_Meter_Doc_Info = 0x54,                       // �޸ı�ߵ�����Ϣ
    Read_Custom_Route_Info = 0x55,                      // ���Զ���·����Ϣ
    Write_Custom_Route_Info = 0x56,                     // д�Զ���·����Ϣ
    Batch_Read_Custom_Routes_Info = 0x57,               // �������Զ���·����Ϣ
    Batch_Write_Custom_Routes_Info = 0x58,              // ����д�Զ���·����Ϣ

    // ������ݲ���ָ��
    Upload_HighPrio_Data = 0x60,                        // �����ȼ��������ϴ�ָ��(����)
    Upload_RealTime_Data = 0x61,                        // ����/������ʱ���������ϴ�ָ��
    Upload_Freeze_Data = 0x62,                          // ����/�������������ϴ�ָ��
    Read_RealTime_Data = 0x63,                          // ����������ʱ��������ָ��
    Read_Freeze_Data = 0x64,                            // ����������������ָ��
    Batch_Read_RealTime_Data = 0x65,                    // ��������������ʱ��������ָ��
    Batch_Read_Freeze_Data = 0x66,                      // ��������������������ָ��

    // �ڲ�����ָ��
    Software_Update_Cmd = 0xF1,                         // ������������
    Output_Monitior_Msg_Cmd = 0xF2,                     // ��������Ϣ����
    Eeprom_Check_Cmd = 0xF3,                            // Eeprom���

    // �޲���ָ��
    Idle_Cmd = 0xFF                                     // �޲���ָ��
} COMMAND_TYPE;

// ����ʹ�õ�USART�ں�UART��,����1 2 3ΪUSART,4 5ΪUART��
typedef enum {
    Event_Start = 0,
    Event_Debug = Event_Start,                          // Debug����
    Event_Ir,                                           // Irͨ�Ŵ���
    Event_Rf,                                           // RFģ���
    Event_Rs485,                                        // RS485�ӿ�
    Event_Gprs,                                         // GPRSģ��ӿ�
    Event_Total                                         // UART����
} DATA_EVENT;

// ����������ݺ����Ľ��
typedef enum {
    Error_Data = 0,                                     // ��������,û�з��ֹؼ���
    Error_DataLength,                                   // ��������ݳ���
    Error_DataOverFlow,                                 // �������
    Error_DataCrcCheck,                                 // ���ݵ�Crc8У�����
    Error_DstAddress,                                   // Ŀ���ַ����
    Error_GetMem,                                       // �����ڴ�ռ�ʧ��
    Ok_Data,                                            // ��ȷ������
} EXTRACT_DATA_RESULT;

// ����ϴ��������Ͷ���
typedef enum {
    RealTimeData = 0,                                   // ʵʱ����
    FwFreezingData,                                     // ��ת��������
    RwFreezingData,                                     // ��ת��������
    FixedTimeData,                                      // ��ʱ�ϴ�����
    QuantitativeData,                                   // �����ϴ�����
    AlarmData,                                          // ��������
} METER_DATA_TYPE;

// �����Ϣ����
typedef enum {
    Gprs_Connect_Msg = 0,                               // Gprs������Ϣ
    Total_Msg
} MONITOR_MSG_TYPE;

/************************************************************************************************
*                                  Union Define Section
************************************************************************************************/
// ·����Ϣ����
typedef union {
    uint8 Content;
    struct {
        uint8 Level: 4;                                 // Bit0-Bit3 ·������,��СֵΪ2
        uint8 CurPos: 4;                                // Bit4-Bit7 ��ǰ���͵��λ��,��0��ʼ
    };
} ROUTER_INFO;

// ���ı�ʶ����(�μ�define���ֵĶ�Ӧ����)
typedef union {
    uint8 Content;
    struct {
        uint8 PkgXor: 1;                                // Bit0 ��������Ӫ�̱��벻����־: 0-����� 1-���
        uint8 Reserve: 3;                               // Bit1-Bit3 ����
        uint8 NeedAck: 1;                               // Bit4 �Ƿ���Ҫ��ִ 0-�����ִ 1-��Ҫ��ִ
        uint8 Encrypt: 1;                               // Bit5 ��������ܱ�־ 0-�Ǽ��� 1-����
        uint8 PkgType: 1;                               // Bit6 ֡���� 0-����֡ 1-Ӧ��֡
        uint8 Direction: 1;                             // Bit7 �����б�ʶ 0-���� 1-����
    };
} PKG_PROPERTY;

// �������ں�Ӧ���ŵ�����
typedef union {
    uint8 Content;
    struct {
        uint8 LifeCycle: 4;                             // ��������,Bit0-Bit3 ���Ϊ15,��һ��·�ɼ�һ,Ϊ0������
        uint8 AckChannel: 4;                            // Ӧ���ŵ�,Bit4-Bit7 ����ʱ�Ǹ��߻���ģ��Ӧ����ŵ�,�����ϴ�ʱ��ָ������Ӧ���ϱ����ŵ�
    };
} LIFE_ACK;
/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/
// �������б�־����
typedef struct {
    uint8 RTCTiming: 1;                                 // Ϊ1ʱ��ʾRTCУʱ�������ڽ�����
    uint8 DataReplenish: 1;                             // Ϊ1ʱ��ʾ���ݲ����������ڽ�����
    uint8 RTCService: 1;                                // Ϊ1ʱ��ʾ��ʱ�����������ڽ�����
    uint8 DataUpload: 1;                                // Ϊ1ʱ��ʾ�����ϴ��������ڽ�����
    uint8 DataForward: 1;                               // Ϊ1ʱ��ʾ����ת���������ڽ�����
} TASK_STATUS_STRUCT;

// ���ݸ�ʽ
typedef struct {
    uint8 PortNo;                                       // ���ĸ��˿ڽ���������
    uint16 PkgLength;                                   // ���ݰ�����,�ӱ���ʼ(��������)��������(����������)�ĳ���
    PKG_PROPERTY PkgProp;                               // ���ı�ʶ
    uint8 PkgSn;                                        // �������,���ͷ��ۼ�,�㲥����Դ���������,�м䲻���
    COMMAND_TYPE Command;                               // ͨ��������
    uint8 DeviceType;                                   // �豸����
    LIFE_ACK Life_Ack;                                  // �������ں�Ӧ���ŵ�����
    ROUTER_INFO RouteInfo;                              // ·����Ϣ
    uint8 Route[MAX_ROUTER_NUM][LONG_ADDR_SIZE];        // ����·��
    uint8 DownRssi;                                     // �����ź�ǿ��
    uint8 UpRssi;                                       // �����ź�ǿ��
    uint8 Crc8;                                         // Crc8У��ֵ
    uint16 DataLen;                                     // �����򳤶�
    uint8 DataBuf[1];                                   // ��������Ϣ
} DATA_FRAME_STRUCT;

// ��ߴ���������ز�������
typedef struct {
    uint8 Prio;                                         // ��������ȼ�
    OS_STK *StkPtr;                                     // ����Ķ�ջָ��
    uint8 PkgSn;                                        // ���ݰ��ı������
    uint8 Command;                                      // ͨ��������
    uint8 RouteLevel;                                   // ͨ��·���ļ���
    uint8 PortNo;                                       // ͨ��ʱʹ�õĴ���
    uint16 NodeId;                                      // �ð�������ӽڵ��ID
    OS_EVENT *Mbox;                                     // ͨ������
    uint8 *Msg;                                         // ����ʼʱ�յ�����Ϣ
} DATA_HANDLE_TASK;

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
DATAHANDLE_EXT uint16 GprsDelayDataUpTimer;
DATAHANDLE_EXT uint16 RTCTimingTimer;
DATAHANDLE_EXT OS_STK Task_DataHandle_Stk[TASK_DATAHANDLE_STK_SIZE];

/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
DATAHANDLE_EXT void DataHandle_Task(void *p_arg);
DATAHANDLE_EXT void DataHandle_OutputMonitorMsg(MONITOR_MSG_TYPE MsgType, uint8 *MsgPtr, uint16 MsgLen);
DATAHANDLE_EXT bool DataHandle_SetRtcTiming(DATA_FRAME_STRUCT *dat, uint8 PreambleLen);

#endif
/***************************************End of file*********************************************/


