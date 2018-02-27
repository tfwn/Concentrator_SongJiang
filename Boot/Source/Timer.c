/************************************************************************************************
*                                   SRWF-6009-BOOT
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Timer.c
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
#include "Uart.h"
#include "Hal.h"


/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
TIMEREVENT_STRUCT TimerEvent[MAX_TIMER_AMOUNT];
uint32 Timer1ms;


/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Timer_Init
* Decription   : ��ʱ����ʼ������
* Input        : None
* Output       : None
* Others       : Timer RA ��ʱ������ʱʱ��Ϊ1ms
************************************************************************************************/
void Timer_Init(void)
{
    uint8 i;

    for (i = 0; i < MAX_TIMER_AMOUNT; i ++)
    {
        TimerEvent[i].Time = INACTIVE_TIMER;
    }
}

/************************************************************************************************
* Function Name: Timer_Handler
* Decription   : ��ʱ���¼�������
* Input        : None
* Output       : None
* Others       : Timer RA ��ʱ������ʱʱ��Ϊ1ms
************************************************************************************************/
void Timer_Handler(void)
{
    uint8 i;

    for (i = 0; i < MAX_TIMER_AMOUNT; i++)
    {
        // �����ʱ��ʱ�䵽�����ж�Ӧ���¼�������
        if (0 == TimerEvent[i].Time)
        {
            TimerEvent[i].Time = INACTIVE_TIMER;
            (*TimerEvent[i].Event)();
        }
        FEED_WATCHDOG
    }
}

/************************************************************************************************
* Function Name: Timer_ActiveEvent
* Decription   : ��ʱ���¼������
* Input        : msTime - ��ʱ��ʱ��(ms), Event - ʱ�䵽ʱ��ִ�еĺ���, Mode - ����ģʽ(���ּ�ʱ�����¼�ʱ)
* Output       : ��ʱ���¼���ʱ�����
* Others       : Timer RA ��ʱ������ʱʱ��Ϊ1ms
************************************************************************************************/
uint8 Timer_ActiveEvent(uint32 msTime, void (*Event)(), TIMEREVENT_ACTIVEMODE_ENUM Mode)
{
    uint8 i;

    // �ж������ʱ���¼��Ƿ��Ѿ�����,����Ѿ����������Mode���ж��Ǽ�����ʱ�������¼�ʱ
    for (i = 0; i < MAX_TIMER_AMOUNT; i++)
    {
        if ((INACTIVE_TIMER != TimerEvent[i].Time) && (Event == TimerEvent[i].Event))
        {
            if (Timer_ActiveResetMode == Mode)
            {
                TimerEvent[i].Time = msTime;
            }
            return i;
        }
    }

    // ��������ʱ���¼�û�б�����,�򼤻������ʱ���¼�
    for (i = 0; i < MAX_TIMER_AMOUNT; i++)
    {
        if (INACTIVE_TIMER == TimerEvent[i].Time)
        {
            TimerEvent[i].Time = msTime;
            TimerEvent[i].Event = Event;
            return i;
        }
    }

    return TIMER_ERROR;
}

/************************************************************************************************
* Function Name: Timer2_ISR
* Decription   : ��ʱ���жϴ�����
* Input        : None
* Output       : None
* Others       : �������ɶ�ʱ��2�жϴ���������
************************************************************************************************/
void Timer2_ISR(void)
{
    uint8 i;

    Timer1ms++;

    // ����1ms���ж��¼�
    if (Uart1.RxTimer > 0)
    {
        Uart1.RxTimer--;
    }

    // ��ʱ���¼�ʱ�Ӽ�ʱ
    for (i = 0; i < MAX_TIMER_AMOUNT; i++)
    {
        if (INACTIVE_TIMER != TimerEvent[i].Time && TimerEvent[i].Time > 0)
        {
            TimerEvent[i].Time--;
        }
    }
}

/**************************************End of file**********************************************/


