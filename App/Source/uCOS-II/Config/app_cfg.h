/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : App_cfg.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/15/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  APP_CFG_H
#define  APP_CFG_H

/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
// �����ڴ�ռ��������
#define TOTAL_GPRS_BLOCK                        200          // GPRS �����ڴ����
#define TOTAL_LARGE_BLOCK                       18          // ���ڴ����
#define TOTAL_SMALL_BLOCK                       5           // С�ڴ����


// �̶��ڴ�ռ��������,�ÿռ�Ϊĳ������ר��,���ͷ�
#define TASK_MAIN_STK_SIZE                      128
#define TASK_SERIALPORT_STK_SIZE                128
#define TASK_GPRS_STK_SIZE                      128
#define TASK_DATAHANDLE_STK_SIZE                168
#define TASK_LED_STK_SIZE                       88

// ��ʱʱ�䶨��,�˴�����Ҫ��Os_cfg.h�ļ��е�OS_TICKS_PER_SEC����һ�����,��СֵΪ10
#define TIME_DELAY_MS(x)                        ((x) * OS_TICKS_PER_SEC / 1000)

// �������ȼ�����
#define TASK_MAIN_PRIO                          4           // ������
#define TASK_SERIAL_PRIO                        6           // ������������
#define TASK_EEPROM_PRIO                        7           // Eeprom���ʻ����ź���
#define TASK_GPRS_PRIO                          8           // Gprs����

#define TASK_DATAHANDLE_RF_PRIO                 9           // Rf�������ݴ�������
#define TASK_DATAHANDLE_GPRS_PRIO               10

#define TASK_DATAHANDLE_DATA_PRIO               12          // �������ݴ���,����5�����ȼ�����Ϊ��

#define TASK_DATAHANDLE_PRIO                    20          // ���ݴ�������

#define TASK_LED_PRIO                           25          // Led����

/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/

/************************************************************************************************
*                                 Variable Declare Section
************************************************************************************************/

/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/

#endif

/***************************************End of file*********************************************/

