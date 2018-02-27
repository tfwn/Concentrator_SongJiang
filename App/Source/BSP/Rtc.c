/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Rtc.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/31/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define RTC_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "Rtc.h"

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
// ����Уʱ�ļ�����
static uint8 Rtc_ErrTimes = 0;

// ƽ����·����ڱ�
const uint8 Rtc_MonthTab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Rtc_IsLeapYear
* Decription   : �ж�һ�����Ƿ�Ϊ����
* Input        : Year-��
* Output       : ��
* Others       : ��
************************************************************************************************/
static bool Rtc_IsLeapYear(uint16 Year)
{
    if (Year % 400 == 0 || Year % 4 == 0 && Year % 100 != 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/************************************************************************************************
* Function Name: Rtc_LastDayofMonth
* Decription   : �����һ���µ����һ���Ǽ���
* Input        : Year-��,Month-��
* Output       : ����
* Others       : ����BCD��ʽ
************************************************************************************************/
uint8 Rtc_LastDayofMonth(uint16 Year, uint8 Month)
{
    uint8 a, b;

    a = (uint8)Year;
    b = (uint8)(Year >> 8);
    Year = (a >> 4 & 0x0F) * 1000 + (a & 0x0F) * 100 + (b >> 4 & 0x0F) * 10 + (b & 0x0F);
    Month = BcdToBin(Month);
    if (2 == Month) {
        return (Rtc_IsLeapYear(Year) ? 0x29 : 0x28);
    }
    if (Month - 1 < sizeof(Rtc_MonthTab)) {
        return (BinToBcd(Rtc_MonthTab[Month - 1]));
    }
    return 0x30;
}

/************************************************************************************************
* Function Name: Gprs_Rtc_Set
* Decription   : ���õ�ǰ��ʱ��,Уʱ��ʱ��͵�ǰʱ��Ĳ�ֵ���ܳ���һ��,�������һ��,������Уʱ
*                RTC_MAX_ERRTIME��������ܳɹ�
* Input        : Time-ʱ��Ľṹ��ָ��, Format-д��ĸ�ʽ
* Output       : �ɹ�����ʧ��
* Others       : ����ʱ���ʱ����Ҫ��������
*				����Уʱ����Ҫ����ʱ����Ϣ��
************************************************************************************************/
bool Gprs_Rtc_Set(RTC_TIME Time, RTC_FORMAT Format, uint8 timezone, uint8 addsub )
{
    uint8 day;
    uint16 i;
    uint32 temp, curTemp, dValue;

    // ���Ƚ���ʽת����HEX��ʽ
    if (Format_Bcd == Format) {
        Time.Year = BcdToBin(Time.Year / 256) + BcdToBin(Time.Year) * 100;
        Time.Month = BcdToBin(Time.Month);
        Time.Day = BcdToBin(Time.Day);
        Time.Hour = BcdToBin(Time.Hour);
        Time.Minute = BcdToBin(Time.Minute);
        Time.Second = BcdToBin(Time.Second);
    }

    // ���Уʱʱ���Ƿ���ȷ
    day = Rtc_MonthTab[Time.Month - 1];
    if (2 == Time.Month && TRUE == Rtc_IsLeapYear(Time.Year)) {
        day += 1;
    }
    if (Time.Year < RTC_MINYEAR || Time.Year > RTC_MAXYEAR ||
        Time.Month == 0 || Time.Month > 12 ||
        Time.Day == 0 || Time.Day > day ||
        Time.Hour >= 24 || Time.Minute >= 60 || Time.Second >= 60) {
        return ERROR;
    }

    // ������ݵ����� ��������� ƽ�������
    temp = 0;
    for (i = RTC_MINYEAR; i < Time.Year; i++) {
        temp += TRUE == Rtc_IsLeapYear(i) ? 366 : 365;
    }

    // �����·ݵ�����
    for (i = 0; i < Time.Month - 1; i++) {
        temp += Rtc_MonthTab[i];
        if (i == 1 && TRUE == Rtc_IsLeapYear(Time.Year)) {
            temp += 1;
        }
    }

    // �����������������������
    temp += Time.Day - 1;
    temp = temp * 24 * 60 * 60;
    if(addsub){
        temp += (uint32)timezone * 60 * 60 / 4;
    }else{
        temp -= (uint32)timezone * 60 * 60 / 4;
    }
    // ����ʱ �� ��
    temp += (uint32)Time.Hour * 60 * 60;
    temp += (uint32)Time.Minute * 60;
    temp += Time.Second;

    // ������ǰʱ���������
    curTemp = RTC_GetCounter();

    // �������ֵ���ж��Ƿ�Ӧ��Уʱ
    dValue = (curTemp >= temp) ? (curTemp - temp) : (temp - curTemp);
    dValue /= 24 * 60 * 60;
    if (dValue && Rtc_ErrTimes < RTC_MAX_ERRTIME) {
        Rtc_ErrTimes += 1;
        return ERROR;
    }
    Rtc_ErrTimes = 0;

    // ����ʱ��,ʹ�ܵ�Դʱ��,ʹ�ܱ���ʱ��
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

    // ʹ��RTC�ͺ󱸼Ĵ�������Ȩ��
    PWR_BackupAccessCmd(ENABLE);

    // ����RTC��������ֵ
    RTC_SetCounter(temp);

    // �ȴ���RTC�Ĵ�����д�������
    RTC_WaitForLastTask();

    Rtc_Init();

    return SUCCESS;
}


/************************************************************************************************
* Function Name: Rtc_Set
* Decription   : ���õ�ǰ��ʱ��,Уʱ��ʱ��͵�ǰʱ��Ĳ�ֵ���ܳ���һ��,�������һ��,������Уʱ
*                RTC_MAX_ERRTIME��������ܳɹ�
* Input        : Time-ʱ��Ľṹ��ָ��, Format-д��ĸ�ʽ
* Output       : �ɹ�����ʧ��
* Others       : ����ʱ���ʱ����Ҫ��������
************************************************************************************************/
bool Rtc_Set(RTC_TIME Time, RTC_FORMAT Format)
{
    uint8 day;
    uint16 i;
    uint32 temp, curTemp, dValue;

    // ���Ƚ���ʽת����HEX��ʽ
    if (Format_Bcd == Format) {
        Time.Year = BcdToBin(Time.Year / 256) + BcdToBin(Time.Year) * 100;
        Time.Month = BcdToBin(Time.Month);
        Time.Day = BcdToBin(Time.Day);
        Time.Hour = BcdToBin(Time.Hour);
        Time.Minute = BcdToBin(Time.Minute);
        Time.Second = BcdToBin(Time.Second);
    }

    // ���Уʱʱ���Ƿ���ȷ
    day = Rtc_MonthTab[Time.Month - 1];
    if (2 == Time.Month && TRUE == Rtc_IsLeapYear(Time.Year)) {
        day += 1;
    }
    if (Time.Year < RTC_MINYEAR || Time.Year > RTC_MAXYEAR ||
        Time.Month == 0 || Time.Month > 12 ||
        Time.Day == 0 || Time.Day > day ||
        Time.Hour >= 24 || Time.Minute >= 60 || Time.Second >= 60) {
        return ERROR;
    }

    // ������ݵ����� ��������� ƽ�������
    temp = 0;
    for (i = RTC_MINYEAR; i < Time.Year; i++) {
        temp += TRUE == Rtc_IsLeapYear(i) ? 366 : 365;
    }

    // �����·ݵ�����
    for (i = 0; i < Time.Month - 1; i++) {
        temp += Rtc_MonthTab[i];
        if (i == 1 && TRUE == Rtc_IsLeapYear(Time.Year)) {
            temp += 1;
        }
    }

    // �����������������������
    temp += Time.Day - 1;
    temp = temp * 24 * 60 * 60;

    // ������ǰʱ���������
    curTemp = RTC_GetCounter();

    // �������ֵ���ж��Ƿ�Ӧ��Уʱ
    dValue = (curTemp >= temp) ? (curTemp - temp) : (temp - curTemp);
    dValue /= 24 * 60 * 60;
    if (dValue && Rtc_ErrTimes < RTC_MAX_ERRTIME) {
        Rtc_ErrTimes += 1;
        return ERROR;
    }
    Rtc_ErrTimes = 0;

    // ����ʱ �� ��
    temp += (uint32)Time.Hour * 60 * 60;
    temp += (uint32)Time.Minute * 60;
    temp += Time.Second;

    // ����ʱ��,ʹ�ܵ�Դʱ��,ʹ�ܱ���ʱ��
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

    // ʹ��RTC�ͺ󱸼Ĵ�������Ȩ��
    PWR_BackupAccessCmd(ENABLE);

    // ����RTC��������ֵ
    RTC_SetCounter(temp);

    // �ȴ���RTC�Ĵ�����д�������
    RTC_WaitForLastTask();

    Rtc_Init();

    return SUCCESS;
}

/************************************************************************************************
* Function Name: Rtc_Get
* Decription   : �õ���ǰ��ʱ��
* Input        : Time-ʱ��Ľṹ��ָ��, Format-�����ĸ�ʽ
* Output       : ��
* Others       : ��
************************************************************************************************/
void Rtc_Get(RTC_TIME *Time, RTC_FORMAT Format)
{
    uint32 temp, secCount;

    // ��ȡ���������ֵ,�õ�����
    secCount = RTC_GetCounter();
    temp = secCount / (24 * 60 * 60);

    // ����õ�����(��4����Ϊ1970��1��1����������)
    Time->Week = (temp + RTC_STARTWEEK) % 7;

    // ����õ���
    Time->Year = RTC_MINYEAR;
    while (temp >= 365) {
        if (Rtc_IsLeapYear(Time->Year)) {
            if (temp >= 366) {
                temp -= 366;
            } else {
                break;
            }
        } else {
            temp -= 365;
        }
        Time->Year += 1;
    }

    // ����õ���
    Time->Month = 0;
    while (temp >= 28) {
        if (Time->Month == 1 && TRUE == Rtc_IsLeapYear(Time->Year)) {
            if (temp >= 29) {
                temp -= 29;
            } else {
                break;
            }
        } else {
            if (temp >= Rtc_MonthTab[Time->Month]) {
                temp -= Rtc_MonthTab[Time->Month];
            } else {
                break;
            }
        }
        Time->Month += 1;
    }
    Time->Month += 1;

    // ����õ���
    Time->Day = temp + 1;

    // ����õ�ʱ �� ��
    temp = secCount % (24 * 60 * 60);
    Time->Hour = temp / (60 * 60);
    temp %= (60 * 60);
    Time->Minute = temp / 60;
    Time->Second = temp % 60;

    // ��ʽת��
    if (Format_Bcd == Format) {
        Time->Year = BinToBcd(Time->Year / 100) | BinToBcd(Time->Year % 100) << 8;
        Time->Month = BinToBcd(Time->Month);
        Time->Day = BinToBcd(Time->Day);
        Time->Hour = BinToBcd(Time->Hour);
        Time->Minute = BinToBcd(Time->Minute);
        Time->Second = BinToBcd(Time->Second);
    }
}

/************************************************************************************************
* Function Name: Rtc_Init
* Decription   : Rtc���ܳ�ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Rtc_Init(void)
{
    // ��Ҫ��������������,����ᵱ��RTC_WaitForSynchro
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    RCC_RTCCLKCmd(ENABLE);

    // ���ϵͳ���ڴ���ģʽ�»ָ����Ҳ�����NRST��������ĸ�λ��:�������λ,ʹ��RTC,��ȷ���Ѿ�ͬ��
    if (PWR_GetFlagStatus(PWR_FLAG_SB) != RESET && RCC_GetFlagStatus(RCC_FLAG_PINRST) == RESET) {
        // ����Ҫ����RTC,��Ϊ�ڴӴ���״̬�»ָ���ʱ��,RTC���ö�û�б�
        PWR_ClearFlag(PWR_FLAG_SB);
        RCC_RTCCLKCmd(ENABLE);
        RTC_WaitForSynchro();
    } else {
        // RTCʱ��Դ����
        if (PWR_GetFlagStatus(PWR_FLAG_SB) != RESET) {
            PWR_ClearFlag(PWR_FLAG_SB);
        }
        if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET) {          // �ϵ縴λʱ,��������ID�ŷ�ȫF,���SRWF-1028ģ�鷢��дID����

        }
        RCC_ClearFlag();                                            // �����־

        if (BKP_ReadBackupRegister(BKP_DR1) != 0x5555) {            // ��ȡ�󱸼Ĵ���1������
            BKP_DeInit();                                           // ��λ���ݼĴ����������
        }

#if 1
        // Rtcʱ��Դ����,�ⲿ����ʱ��Դ,���ȴ����ȶ�
        RCC_LSEConfig(RCC_LSE_ON);
        while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) {
        }
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
#else
        // ���ʹ�õ����ڲ�����ʱ��Դ
        RCC_LSICmd(ENABLE);
        while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET) {
        }
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
#endif
        RCC_RTCCLKCmd(ENABLE);                                      // ʹ��RTCʱ��

        RTC_WaitForSynchro();                                       // �ȴ�RTC APB�Ĵ���ͬ�����
        RTC_WaitForLastTask();                                      // �ȴ����һ�ζ�RTC�Ĵ�����д�������

        RTC_ITConfig(RTC_IT_ALR, DISABLE);                          // ����RTC�����ж� zhangxp-��ʱ��ֹ,û����ʲô����
        RTC_WaitForLastTask();                                      // �ȴ����һ��дָ�����

        RTC_SetPrescaler(32767);                                    // ����RTCʱ���׼Ϊ1��
        RTC_WaitForLastTask();                                      // �ȴ����һ��д����

        BKP_WriteBackupRegister(BKP_DR1, 0x5555);                   // д��RTC�󱸼Ĵ���1 0x5555 ��ʾʱ���Ѿ����ù���

        RCC_ClearFlag();
    }
}

/***************************************End of file*********************************************/

