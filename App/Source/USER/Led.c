/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Led.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        08/13/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define LED_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Led.h"
#include "DataHandle.h"
#include "Database.h"

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Led_OnOffCtrl
* Decription   : Led�ƵĿ��ؿ��ƺ���
* Input        : Led-Led������,OnOff-�����߹�
* Output       : ��
* Others       : ��
************************************************************************************************/
static void Led_OnOffCtrl(LED_TYPE Led, bool OnOff)
{
    if (Led1 == Led) {
        if (OnOff) {
            GPIO_SetBits(GPIOD, GPIO_Pin_0);
        } else {
            GPIO_ResetBits(GPIOD, GPIO_Pin_0);
        }
    } else if (Led2 == Led) {
        if (OnOff) {
            GPIO_SetBits(GPIOD, GPIO_Pin_1);
        } else {
            GPIO_ResetBits(GPIOD, GPIO_Pin_1);
        }
    }
}

/************************************************************************************************
* Function Name: Led_Init
* Decription   : Led��ʼ��GPIO�˿�
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Led_Init(void)
{
    bool onOff = ON;
    uint8 size1, size2, size3;
    uint32 delayTime;
    GPIO_InitTypeDef GPIO_InitStructure;

    //OSC_IN��OSC_OUT��ӳ��ΪIO��
    GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // ���Flash�洢��Ԫ�Ĵ�С����Ϊż��,�������ż����˫��LED��ʾ
    size1 = sizeof(CUSTOM_ROUTE_INFO);
    size2 = sizeof(SUBNODE_INFO);
    size3 = sizeof(CONCENTRATOR_INFO);
    if ((size1 & 0x01) || (size2 & 0x01) || (size3 & 0x01)) {
        onOff = ~onOff;
        while (TRUE) {
            onOff = !onOff;
            delayTime = 100000;
            while (delayTime--) {
                Led_OnOffCtrl(Led1, onOff);
                Led_OnOffCtrl(Led2, !onOff);
            }
        }
    }
}

/************************************************************************************************
* Function Name: Led_Task
* Decription   : Led����
* Input        : *p_arg-����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Led_Task(void *p_arg)
{
    uint8 i;
    LED_CONTROL *ledPtr;
    const uint16 FlashTimeArray[] = {0, 1, 2, 4, 6, 10, 16, 20, 24, 30, 36, 40, 50, 60, 100, 0x1F};

    (void)p_arg;

    // ��ʼʱ������˸5��,���ڼ���LED�Ƿ���������
    for (i = 0; i < 5; i++) {
        Led_OnOffCtrl(Led1, ON);
        Led_OnOffCtrl(Led2, ON);
        OSTimeDlyHMSM(0, 0, 0, 100);
        Led_OnOffCtrl(Led1, OFF);
        Led_OnOffCtrl(Led2, OFF);
        OSTimeDlyHMSM(0, 0, 0, 80);
    }

    // ��ʼ��LED���Ʋ���
    for (i = Led1; i < TotalLed; i++) {
        Led.Control[i].RunTime = 0;
        Led.Control[i].Status = 0;
        Led.Control[i].DelayTimer = FLASHDELAYTIME / 50;
    }

    // LED��˸����
    while (TRUE) {
        OSTimeDlyHMSM(0, 0, 0, 50);
        for (i = Led1; i < TotalLed; i++) {
            ledPtr = &Led.Control[i];
            if (--ledPtr->DelayTimer == 0) {
                ledPtr->DelayTimer = FLASHDELAYTIME / 50;
                ledPtr->CurLedStatus = ledPtr->FlashCtrl;
            }
            if (ledPtr->RunTime > 0) {
                ledPtr->RunTime--;
            } else {
                if (AlwaysOff == FlashOffTime(ledPtr->CurLedStatus)) {
                    ledPtr->Status = OFF;
                } else if (AlwaysOn == FlashOnTime(ledPtr->CurLedStatus)) {
                    ledPtr->Status = ON;
                } else {
                    if (ON == ledPtr->Status) {
                        ledPtr->RunTime = FlashTimeArray[FlashOffTime(ledPtr->CurLedStatus)];
                        ledPtr->Status = OFF;
                    } else {
                        ledPtr->RunTime = FlashTimeArray[FlashOnTime(ledPtr->CurLedStatus)];
                        ledPtr->Status = ON;
                    }
                }
                Led_OnOffCtrl((LED_TYPE)i, ledPtr->Status);
            }
        }
    }
}

/************************************************************************************************
* Function Name: Led_FlashTime
* Decription   : Led��˸���ƺ���
* Input        : LedType-Led����,OnTime-Led����ʱ��,OffTime-Led���ʱ��,Effect-�Ƿ�����ʵʩ
* Output       : ��
* Others       : ��
************************************************************************************************/
void Led_FlashTime(LED_TYPE LedType, FLASH_TIME OnTime, FLASH_TIME OffTime, bool Effect)
{
    uint8 ledFlash = (OnTime << 4 & 0xF0 | OffTime);
    LED_CONTROL *ledPtr = &Led.Control[LedType];

    if (TRUE == Effect) {
        ledPtr->CurLedStatus = ledFlash;
        ledPtr->DelayTimer = 1;
        ledPtr->RunTime = 0;
    }
    ledPtr->FlashCtrl = ledFlash;
}

/***************************************End of file*********************************************/

