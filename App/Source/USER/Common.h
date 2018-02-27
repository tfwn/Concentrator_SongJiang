/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Common.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        07/16/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  COMMON_H
#define  COMMON_H


#ifdef   COMMON_GLOBALS
#define  COMMON_EXT
#else
#define  COMMON_EXT  extern
#endif

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Version.h"

/**************************************************************************************************
*                          Pubilc Macro Define Section
**************************************************************************************************/
#define TRUE                                    1
#define FALSE                                   0

#define ON                                      1
#define OFF                                     0

#define HIGH                                    1
#define LOW                                     0

#define NULL                                    0

#define READ_ATTR                               0           // ������
#define WRITE_ATTR                              1           // д����
#define DELETE_ATTR                             2           // ɾ������

#define BIT(x)                                  (1 << (x))

#define BROADCAST_CRC16                         0xBECF
#define LONG_ADDR_SIZE                          6
#define SHORT_ADDR_SIZE                         2
#define PREAMBLE                                0x55
#define SYNCWORD1                               0xD3
#define SYNCWORD2                               0x91
#define TAILBYTE                                0x16

#define GPRS_SOFT_UP_EN							0xD9

/************************************************************************************************
*                                Typedef Section
************************************************************************************************/
typedef unsigned char       uint8;                          // �޷���8λ���ͱ���
typedef signed char         int8;                           // �з���8λ���ͱ���
typedef unsigned short      uint16;                         // �޷���16λ���ͱ���
typedef signed short        int16;                          // �з���16λ���ͱ���
typedef unsigned long       uint32;                         // �޷���32λ���ͱ���
typedef signed long         int32;                          // �з���32λ���ͱ���
typedef float               fp32;                           // �����ȸ�������32λ���ȣ�
typedef double              fp64;                           // ˫���ȸ�������64λ���ȣ�
typedef unsigned char       bool;                           // �������ͱ���


/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/
// �������
typedef enum
{
    OP_Succeed = 0xAA,                                      // �����ɹ�
    OP_Failure = 0xAB,                                      // ����ʧ��
    OP_CommunicateError = 0xAC,                             // ͨѶʧ��
    OP_CommunicateOK = 0xAD,                                // ͨѶ�ɹ�
    OP_FormatError = 0xAE,                                  // ��ʽ����
    OP_TimeAbnormal = 0xAF,                                 // ʱ���쳣
    OP_ObjectNotExist = 0xBA,                               // ���󲻴���
    OP_ObjectRepetitive = 0xBB,                             // �����ظ�
    OP_Objectoverflow = 0xBC,                               // ��������
    OP_ParameterError = 0xBD,                               // ��������
    OP_OvertimeError = 0xCC,                                // ��ʱ����
    OP_SRunOvertimeError = 0xCD,                            // �������г�ʱ����
    OP_Executing = 0xCE,                                    // ����ִ��
    OP_HadDealed = 0xCF,                                    // �����Ѵ���
    OP_HadAck = 0xD0,                                       // ��Ӧ��
    OP_ErrorMeterData = 0xD1,                               // ���������д���
    OP_NoFunction = 0xD2                                    // û�д����
} OP_RESULT;

/************************************************************************************************
*                                  Union Define Section
************************************************************************************************/

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
COMMON_EXT uint8 BroadcastAddrIn[LONG_ADDR_SIZE];
COMMON_EXT uint8 BroadcastAddrOut[LONG_ADDR_SIZE];
COMMON_EXT uint8 NullAddress[LONG_ADDR_SIZE];

/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
COMMON_EXT uint8 CalCrc8(const uint8 *DataBuf, uint16 DataLen);
COMMON_EXT uint16 CalCrc16(const uint8 *DataBuf, uint32 DataLen);
COMMON_EXT void Uint16ToString(uint16 Src, uint8 *DstPtr);
COMMON_EXT void Uint8ToString(uint8 Src, uint8 *DstPtr);
COMMON_EXT void StringToByte(uint8 *SrcPtr, uint8 *DstPtr, uint8 Len);
COMMON_EXT uint8 BinToBcd(uint8 Val);
COMMON_EXT uint8 BcdToBin(uint8 Val);
COMMON_EXT uint16 BcdToAscii(uint8 *SrcPtr, uint8 *DstPtr, uint8 SrcLength, uint8 LenMul);
COMMON_EXT uint8 gprs_print(uint8 *DataFrmPtr, uint8 len);
COMMON_EXT uint16 phyCalCRC16(uint8*Ptr, uint8 Len);

#endif
/***************************************End of file*********************************************/

