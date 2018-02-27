/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Led.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        08/13/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  LED_H
#define  LED_H


#ifdef   LED_GLOBALS
#define  LED_EXT
#else
#define  LED_EXT  extern
#endif

/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
#define FlashOnTime(x)                  ((x) >> 4 & 0x0F)
#define FlashOffTime(x)                 ((x) & 0x0F)
#define FLASHDELAYTIME                  600

/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/
// Led��ö�ٱ���
typedef enum {
    Led1 = 0,
    Led2,
    TotalLed
} LED_TYPE;

// Led��˸��ʱ�䶨��
typedef enum
{
    AlwaysOff = 0,              // ���AlwayOff��ѡ�У���ȳ����������ȼ����
    Delay50ms,
    Delay100ms,
    Delay200ms,
    Delay300ms,
    Delay500ms,
    Delay800ms,
    Delay1000ms,
    Delay1200ms,
    Delay1500ms,
    Delay1800ms,
    Delay2000ms,
    Delay2500ms,
    Delay3000ms,
    Delay5000ms,
    AlwaysOn,
    EndDelayTime
} FLASH_TIME;

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/
// Led���Ʋ�����������
typedef struct {
    uint8 RunTime: 7;                               // Led���м�����
    uint8 Status: 1;                                // Led����״̬
    uint8 FlashCtrl;                                // Led��˸����
    uint8 CurLedStatus;                             // Led��ǰ��˸״̬
    uint8 DelayTimer;                               // Led��ʱ��ʱ��
} LED_CONTROL;

// Led�����ṹ���������
typedef struct {
    LED_CONTROL Control[TotalLed];                  // Led������
    OS_STK Task_Stk[TASK_LED_STK_SIZE];             // Led�����ջ
} LED_PARAM;

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
LED_EXT LED_PARAM Led;
LED_EXT LED_PARAM GprsPrint;


/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
LED_EXT void Led_Init(void);
LED_EXT void Led_Task(void *p_arg);
LED_EXT void Led_FlashTime(LED_TYPE LedType, FLASH_TIME OnTime, FLASH_TIME OffTime, bool Effect);


#endif
/***************************************End of file*********************************************/


