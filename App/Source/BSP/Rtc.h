/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Rtc.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/31/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  RTC_H
#define  RTC_H

#ifdef   RTC_GLOBALS
#define  RTC_EXT
#else
#define  RTC_EXT  extern
#endif

/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
// Rtc�ĺϷ�ʱ��ζ���
#define RTC_MINYEAR                     1970                // ʵʱʱ�ӿ�ʼ����
#define RTC_MAXYEAR                     2099                // ʵʱʱ����ֹ����
#define RTC_STARTWEEK                   4                   // 1970��1��1����������

#define RTC_MAX_ERRTIME                 1                   // ����Уʱ��������

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/
// Rtc�����Ľṹ�嶨��
typedef  struct {
    uint16 Year;
    uint8 Month;
    uint8 Day;
    uint8 Hour;
    uint8 Minute;
    uint8 Second;
    uint8 Week;                                             // 0����������
} RTC_TIME;

/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/
// ��дRTCʱ�ĸ�ʽ
typedef enum {
    Format_Bcd,                                             // Bcd��ʽ��д
    Format_Bin,                                             // Bin��ʽ��д
} RTC_FORMAT;
/************************************************************************************************
*                            Variable Declare Section
************************************************************************************************/

/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
RTC_EXT uint8 Rtc_LastDayofMonth(uint16 Year, uint8 Month);
RTC_EXT bool Gprs_Rtc_Set(RTC_TIME Time, RTC_FORMAT Format, uint8 TimeZone, uint8 addsub );
RTC_EXT bool Rtc_Set(RTC_TIME Time, RTC_FORMAT Format);
RTC_EXT void Rtc_Get(RTC_TIME *Time, RTC_FORMAT Format);
RTC_EXT void Rtc_Init(void);

#endif
/***************************************End of file*********************************************/

