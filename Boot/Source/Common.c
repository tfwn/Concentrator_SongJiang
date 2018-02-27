/************************************************************************************************
*                                   SRWF-6009-BOOT
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Common.c
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
#include "Timer.h"
#include "Hal.h"
#include "stm32f10x.h"
#include "Core_cm3.h"


/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/


/************************************************************************************************
* Function Name: DataCopy
* Decription   : ���ݿ�������
* Input        : Src - Դ���ݻ�����ָ��, Dst - Ŀ�����ݻ�����ָ��, Len - �������ݵĳ���
* Output       : ʵ�ʿ������ݵ�����
* Others       : None
************************************************************************************************/
uint16 DataCopy(uint8 *Dst, uint8 *Src, uint16 Len)
{
    uint16 i;

    for (i = 0; i < Len; i++)
    {
        FEED_WATCHDOG
        * Dst++ = * Src++;
    }
    return i;
}

/************************************************************************************************
* Function Name: DataCompare
* Decription   : ���ݱȽϺ���
* Input        : Buf1 - ���ݻ�����1ָ��, Buf2 - ���ݻ�����2ָ��, Len - �Ƚ����ݵĳ���
* Output       : TRUE - ��ͬ, FALSE - ��ͬ
* Others       : None
************************************************************************************************/
bool DataCompare(uint8 *Buf1, uint8 *Buf2, uint16 Len)
{
    uint16 i;

    for (i = 0; i < Len; i++)
    {
        FEED_WATCHDOG
        if (*(Buf1 + i) != *(Buf2 + i))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************************************
* Function Name: Cal_Crc
* Decription   : CRCУ�麯��
* Input        : Seed,��У�����ݵ�Bufָ��,��У�����ݳ���Len
* Output       : ���������У��ֵ
* Others       : None
************************************************************************************************/
uint16 Cal_Crc(uint16 Seed, uint8 *Buf, uint32 Len)
{
    uint16 crc = 0xFFFF;
    uint8 i;

    while (Len--)
    {
        FEED_WATCHDOG
        crc ^= * Buf++;
        for (i = 0; i < 8; i ++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= Seed;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    crc ^= 0xFFFF;

    return crc;
}

/************************************************************************************************
* Function Name: SoftReset
* Decription   : ϵͳ���������λ
* Input        : None
* Output       : None
* Others       :
************************************************************************************************/
void SoftReset(void)
{
    NVIC_SystemReset();
}

/**************************************End of file**********************************************/


