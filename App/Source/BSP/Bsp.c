/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Bsp.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        07/14/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define BSP_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Bsp.h"
#include "SerialPort.h"
#include "Timer.h"
#include "Gprs.h"
#include "Rtc.h"
#include "Led.h"
#include "Eeprom.h"
#include "Flash.h"
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
* Function Name: SysTick_Init
* Decription   : ������ʱ����ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void SysTick_Init(void)
{
    RCC_ClocksTypeDef rcc_clocks;

    RCC_GetClocksFreq(&rcc_clocks);
    SysTick_Config(rcc_clocks.HCLK_Frequency / OS_TICKS_PER_SEC);
}

/************************************************************************************************
* Function Name: Rcc_Init
* Decription   : ʱ�ӳ�ʼ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Rcc_Init(void)
{
    /* RCC system reset(for debug purpose) */
    RCC_DeInit();

#if defined (STM32F10X_HD) || (defined STM32F10X_XL) || (defined STM32F10X_HD_VL)
#ifdef DATA_IN_ExtSRAM
    SystemInit_ExtMemCtl();
#endif /* DATA_IN_ExtSRAM */
#endif

    /* Configure the System clock frequency, HCLK, PCLK2 and PCLK1 prescalers */
    /* Configure the Flash Latency cycles and enable prefetch buffer */
    RCC_HSICmd(ENABLE);                                             // ʹ���ڲ�8MHz����
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET) {
    }

    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    FLASH_SetLatency(FLASH_Latency_2);

    RCC_HCLKConfig(RCC_SYSCLK_Div2);                                // ΪSYSCLK����Ƶ
    RCC_PCLK2Config(RCC_HCLK_Div1);                                 // PCLK2ʱ�Ӳ���Ƶ
    RCC_PCLK1Config(RCC_HCLK_Div1);                                 // PCLK1ʱ�Ӳ���Ƶ

    // ���� PLL ʱ��Դ����Ƶϵ��
    RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_12);           // 48MHz

    // ʹ�ܻ���ʧ�� PLL,�����������ȡ:ENABLE����DISABLE,���PLL������ϵͳʱ��,��ô�����ܱ�ʧ��
    RCC_PLLCmd(ENABLE);

    // �ȴ�ָ���� RCC ��־λ���óɹ� �ȴ�PLL��ʼ���ɹ�
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {

    }

    // ����ϵͳʱ�ӣ�SYSCLK�� ����PLLΪϵͳʱ��Դ
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    // �ȴ�PLL�ɹ�������ϵͳʱ�ӵ�ʱ��Դ
    //  0x00��HSI ��Ϊϵͳʱ��
    //  0x04��HSE��Ϊϵͳʱ��
    //  0x08��PLL��Ϊϵͳʱ��
    while (RCC_GetSYSCLKSource() != 0x08) {

    }

#ifdef VECT_TAB_SRAM
    //     SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM. */
#else
    //     SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH. */
#endif

    /* Enable GPIOA, GPIOB, GPIOC and AFIO clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD| RCC_APB2Periph_AFIO , ENABLE);

    /* Enable TIM2 Clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Enable WWDG clock */
//    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG | RCC_APB1Periph_SPI2, ENABLE);

    /* Enable DMA clock */
//    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // ����USBʱ��
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1);  //USBclk=PLLclk/1=48Mhz
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

/************************************************************************************************
* Function Name: Gpio_Init
* Decription   : Gpio���ų�ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);

    // PG���ų�ʼ��
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    // D+��������(PA18),�������USB��ʼ��֮ǰ,Ŀ������PC������ö��USB�˿�
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_ResetBits(GPIOA, GPIO_Pin_12);

    // RF������������Ϊ����״̬
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/************************************************************************************************
* Function Name: Nvic_Init
* Decription   : �ж�������ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
static void Nvic_Init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Configure the NVIC Preemption Priority Bits */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    /* Enable the TIM2 UP Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

     /* Enable the USB interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Enable the USB Wake-up interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_Init(&NVIC_InitStructure);
}

/**
  \brief   System Reset
  \details Initiates a system reset request to reset the MCU.
 */
__STATIC_INLINE void __NVIC_SystemReset(void)
{
	__DSB();                                                          /* Ensure all outstanding memory accesses included
	                                                                   buffered write are completed before reset */
	SCB->AIRCR  = (uint32_t)((0x5FAUL << SCB_AIRCR_VECTKEY_Pos)    |
	                       (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) |
	                        SCB_AIRCR_SYSRESETREQ_Msk    );         /* Keep priority group unchanged */
	__DSB();                                                          /* Ensure completion of memory access */

	for(;;)                                                           /* wait until reset */
	{
		__NOP();
	}
}

/************************************************************************************************
* Function Name: SoftReset
* Decription   : �����λ
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void SoftReset(void)
{
    //NVIC_SystemReset();
    __NVIC_SystemReset();
}

/************************************************************************************************
* Function Name: Iwdg_Init
* Decription   : �������Ź���ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
static void Iwdg_Init(void)
{
    // ���������ģʽʱ���Ź�ֹͣ����
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;

    // ���д����
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    // ���÷�Ƶϵ��,ʱ�����(���):Tout=((4*2^prer)*rlr)/40 (ms) =((4*2^5)*1600)/40=5S
    IWDG_SetPrescaler(IWDG_Prescaler_128);
    IWDG_SetReload(1600);

    // ��װһ��
    IWDG_ReloadCounter();

    // �������Ź�
    IWDG_Enable();

    RCC_LSICmd(ENABLE);                                     //��LSI,��Ϊ�������Ź�ʹ�õ���LSI��������ó���������ʱ��ʹʱ��Դ�ȶ�:
    while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY)==RESET);       //�ȴ�ֱ��LSI�ȶ�
}

/************************************************************************************************
* Function Name: BSP_Init
* Decription   : �弶֧�ְ���ʼ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void BSP_Init(void)
{
    CriticalSecCntr = 0;
    DevResetTimer = MAIN_BOARD_RESTART_PERIOD;
    OS_ENTER_CRITICAL();//�ؼ�����Σ����ɱ���ϣ���ʼ���� OS_EXIT_CRITICAL ��һ��
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, (FLASH_APPCODE_START_ADDR - NVIC_VectTab_FLASH));
    Rcc_Init();			// STM32 ʱ�ӳ�ʼ��
    Gpio_Init();
    Timer2_Init();
    Rtc_Init();
    SerialPort_Init();	// 5 �� uart �ں�һ�� usb �ڳ�ʼ��
    Gprs_Init();
    Nvic_Init();
    Eeprom_Init();		// I2C ��ʼ��
    Led_Init();
    Iwdg_Init();
    OS_EXIT_CRITICAL();	//�ؼ�����Σ����ɱ���ϣ�������
}

/***************************************End of file*********************************************/

