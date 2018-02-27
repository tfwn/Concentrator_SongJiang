/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Main.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/14/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define MAIN_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "Bsp.h"
#include "ucos_ii.h"
#include "Main.h"
#include "Rtc.h"
#include "SerialPort.h"
#include "Gprs.h"
#include "Led.h"
#include "Eeprom.h"
#include "Flash.h"
#include "DataHandle.h"
#include "Database.h"

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
__root const uint8 SWInfo[] = SOFTWARE_VERSION_INFO;


/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Main_Task
* Decription   : ��������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Main_Task(void *p_arg)
{
    uint8 i, err;

    (void)p_arg;
    OS_ENTER_CRITICAL();
    SysTick_Init();
    OS_EXIT_CRITICAL();
#if OS_TASK_STAT_EN > 0u
    OSStatInit();
#endif

    // �����ڴ�
    GprsMemoryPtr  = OSMemCreate(MemoryGprsBlock,  TOTAL_GPRS_BLOCK,  MEM_GPRS_BLOCK_LEN,  &err);
    SmallMemoryPtr = OSMemCreate(MemorySmallBlock, TOTAL_SMALL_BLOCK, MEM_SMALL_BLOCK_LEN, &err);
    LargeMemoryPtr = OSMemCreate(MemoryLargeBlock, TOTAL_LARGE_BLOCK, MEM_LARGE_BLOCK_LEN, &err);

    // �����ź�����
    GlobalEventFlag = OSFlagCreate(0, &err);

    // ��������
    for (i = Port_Start; i < Port_Total; i++) {
        SerialPort.Port[i].MboxTx = OSMboxCreate((void *)0);
        SerialPort.Port[i].MboxRx = OSMboxCreate((void *)0);
    }
    Gprs.MboxTx = OSMboxCreate((void *)0);
    Gprs.MboxRx = OSMboxCreate((void *)0);

    // ���������ź���
    EepromAccessMutex = OSMutexCreate(TASK_EEPROM_PRIO, &err);

    // �����������ݹ�������
    OSTaskCreate(SerialPort_Task,
                 (void *)0,
                 &SerialPort.Task_Stk[TASK_SERIALPORT_STK_SIZE - 1],
                 TASK_SERIAL_PRIO);

    // ����GPRS���Ӻ����ݹ�������
    OSTaskCreate(Gprs_Task,
                 (void *)0,
                 &Gprs.Task_Stk[TASK_GPRS_STK_SIZE - 1],
                 TASK_GPRS_PRIO);

    // �������ݴ�������
    OSTaskCreate(DataHandle_Task,
                 (void *)0,
                 &Task_DataHandle_Stk[TASK_DATAHANDLE_STK_SIZE - 1],
                 TASK_DATAHANDLE_PRIO);

    // ����LED���ƹ�������
    OSTaskCreate(Led_Task,
                 (void *)0,
                 &Led.Task_Stk[TASK_LED_STK_SIZE - 1],
                 TASK_LED_PRIO);

    // ��λRFģ��
    GPIO_ResetBits(GPIOB, GPIO_Pin_13);
    OSTimeDlyHMSM(0, 0, 0, 800);
    GPIO_SetBits(GPIOB, GPIO_Pin_13);

    while (TRUE) {

        // GPRS ����������ʱ�ϴ�
        if (GprsDelayDataUpTimer) {
            if (0 == --GprsDelayDataUpTimer) {
                OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_GPRS_DELAY_UPLOAD, OS_FLAG_SET, &err);
            }
        }

        // ʱ��Уʱ
        if (RTCTimingTimer) {
            if (0 == --RTCTimingTimer) {
                OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_RTC_TIMING_TIMER, OS_FLAG_SET, &err);
            }
        }

        // �������
        if (Gprs.HeartbeatInterval) {
            if (0 == --Gprs.HeartbeatInterval) {
                OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_HEATBEAT_TIMER, OS_FLAG_SET, &err);
            }
        }

        // ι��,ע��������ʱ��ʱ��һ��ҪС��ι��ʱ��
        IWDG_ReloadCounter();
        OSTimeDlyHMSM(0, 0, 1, 0);
    }
}

/************************************************************************************************
* Function Name: main
* Decription   : ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void main(void)
{
    BSP_Init();
    OSInit();
    OSTaskCreate(Main_Task,
                 (void *)0,
                 &MainTask_Stk[TASK_MAIN_STK_SIZE - 1],
                 TASK_MAIN_PRIO);
    OSStart();
    return;
}

/***************************************End of file*********************************************/

