/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Eeprom.c
* Description  : ����Stm32_eval_i2c_ee.c��д,����DMA
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        08/03/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define EEPROM_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Eeprom.h"

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
static uint8 EepromDMADone;             // 0-������ 1-��� 2-���ִ���


/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: DMA1_Channel6_IRQHandler
* Decription   : Eepromд����DMA�жϴ�����
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void DMA1_Channel6_IRQHandler(void)
{
    uint32 timeOut;

    OSIntEnter();

    // ���DMA�����Ƿ����
    if (DMA_GetFlagStatus(DMA1_IT_TC6) != RESET) {

        // ����DMA����ͨ��������жϱ�ʶλ
        DMA_Cmd(DMA1_Channel6, DISABLE);
        DMA_ClearFlag(DMA1_IT_GL6);
        EepromDMADone = 1;

        // �ȴ�ֱ�����ݶ��������͵�������,����������,���ж��⴦��
        timeOut = EEPROM_LONG_TIMEOUT;
        while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF)) {
            if ((timeOut--) == 0) {
                EepromDMADone = 2;
            }
        }

        // ����ֹͣ����
        I2C_GenerateSTOP(I2C1, ENABLE);

        // ����SR1��SR2�Ķ������Ա�����¼�λ
        (void)I2C1->SR1;
        (void)I2C1->SR2;
    }

    OSIntExit();
}

/************************************************************************************************
* Function Name: DMA1_Channel7_IRQHandler
* Decription   : Eeprom������DMA�жϴ�����
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void DMA1_Channel7_IRQHandler(void)
{
    OSIntEnter();

    // ���DMA�����Ƿ����
    if (DMA_GetFlagStatus(DMA1_IT_TC7) != RESET) {

        // ���ͽ�������
        I2C_GenerateSTOP(I2C1, ENABLE);

        // ����DMA������жϱ�־λ
        DMA_Cmd(DMA1_Channel7, DISABLE);
        DMA_ClearFlag(DMA1_IT_GL7);

        EepromDMADone = 1;
    }

    OSIntExit();
}

/************************************************************************************************
* Function Name: Eeprom_Timeout_UserCallback
* Decription   : Eeprom������ʱ������
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
static void Eeprom_Timeout_UserCallback(void)
{
    uint8 i, delay;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ��λ����������:SCL��Ӧ�Ķ˿�ΪOD���,��ǿ����SCL����9������
    for (i = 0; i < 10; i++) {
        GPIO_SetBits(GPIOB, GPIO_Pin_6);
        for (delay = 0; delay < 100; delay++);
        GPIO_ResetBits(GPIOB, GPIO_Pin_6);
        for (delay = 0; delay < 100; delay++);
    }
    GPIO_SetBits(GPIOB, GPIO_Pin_6);                // �˾������,������Ч

    // ��λSTM32��I2C����(SCLK,SDA����Ϊ�ߵ�ƽ����Ч)
    I2C_SoftwareResetCmd(I2C1, ENABLE);

    // ���³�ʼ��I2C
    Eeprom_Init();
}

/************************************************************************************************
* Function Name: Eeprom_LowLevel_DMAConfig
* Decription   : Eeprom���ݴ���DMA�ͼ�����
* Input        : BufPtr-�������ݵ�ָ��ֵ,BufSize-�������ݵ�����,AccessMode-�����ڼ��ģʽ
* Output       : ��
* Others       : ��
************************************************************************************************/
static void Eeprom_LowLevel_DMAConfig(uint32 BufPtr, uint32 BufSize, uint8 AccessMode)
{
    DMA_InitTypeDef eprom_DMA_InitStructure;

    if (AccessMode == WRITE_ATTR) {
        // ����DMA����ͨ������
        eprom_DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)I2C1_DR_Address;
        eprom_DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)BufPtr;
        eprom_DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
        eprom_DMA_InitStructure.DMA_BufferSize = (uint32_t)BufSize;
        eprom_DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        eprom_DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        eprom_DMA_InitStructure.DMA_PeripheralDataSize = DMA_MemoryDataSize_Byte;
        eprom_DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
        eprom_DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
        eprom_DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
        eprom_DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
        DMA_Init(DMA1_Channel6, &eprom_DMA_InitStructure);
    } else {
        // ����DMA����ͨ������
        eprom_DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)I2C1_DR_Address;
        eprom_DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)BufPtr;
        eprom_DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
        eprom_DMA_InitStructure.DMA_BufferSize = (uint32_t)BufSize;
        eprom_DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        eprom_DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        eprom_DMA_InitStructure.DMA_PeripheralDataSize = DMA_MemoryDataSize_Byte;
        eprom_DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
        eprom_DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
        eprom_DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
        eprom_DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
        DMA_Init(DMA1_Channel7, &eprom_DMA_InitStructure);
    }
}

/************************************************************************************************
* Function Name: Eeprom_AccessProc
* Decription   : Eeprom��д���ʺ���
* Input        : DevAddr-EepromоƬ�ĵ�ַ
*                SubAddr-EepromоƬ���ڲ���ַ,����д��ַ
*                BufPtr-����������ݵ�ָ���д�����ݵĻ�����ָ��
*                Count-��д���ݵ�����
*                AccessMode-Eeprom���ʷ�ʽ,������д
* Output       : �ɹ�����ʧ��
* Others       : ����DMAģʽ����
************************************************************************************************/
static bool Eeprom_AccessProc(uint8 DevAddr, uint32 SubAddr, uint8 *BufPtr, uint16 Count, uint8 AccessMode)
{
    uint32 timeOut;

    // ���I2C�����Ƿ�æ
    timeOut = EEPROM_LONG_TIMEOUT;
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY)) {
        if ((timeOut--) == 0) {
            goto EEPROM_TIMEOUT_HANDLE;
        }
    }

    // ������ʼ����������Ƿ���ȷִ��
    I2C_GenerateSTART(I2C1, ENABLE);
    timeOut = EEPROM_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT)) {
        if ((timeOut--) == 0) {
            goto EEPROM_TIMEOUT_HANDLE;
        }
    }

    // ����оƬ��ַ(дģʽ)������Ƿ���ȷִ��
    if (SubAddr > 0xFFFF) {
        DevAddr |= 0x02;
    }
    I2C_Send7bitAddress(I2C1, DevAddr, I2C_Direction_Transmitter);
    timeOut = EEPROM_LONG_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        if ((timeOut--) == 0) {
            goto EEPROM_TIMEOUT_HANDLE;
        }
    }

    // ����оƬ�ڲ��ӵ�ַ���ֽڲ�����Ƿ���ȷִ��
    I2C_SendData(I2C1, (uint8)(SubAddr >> 8));
    timeOut = EEPROM_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        if ((timeOut--) == 0) {
            goto EEPROM_TIMEOUT_HANDLE;
        }
    }

    // ����оƬ�ڲ��ӵ�ַ���ֽڲ�����Ƿ���ȷִ��
    I2C_SendData(I2C1, (uint8)SubAddr);
    timeOut = EEPROM_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        if ((timeOut--) == 0) {
            goto EEPROM_TIMEOUT_HANDLE;
        }
    }

    if (WRITE_ATTR == AccessMode) {
        // �����дģʽ,������DMA����ͨ��,����Ϊ������ָ��ͻ�������С��ʹ��DMAͨ��
        Eeprom_LowLevel_DMAConfig((uint32)BufPtr, Count, AccessMode);
        DMA_Cmd(DMA1_Channel6, ENABLE);
        EepromDMADone = 0;
    } else {
        // ����Ƕ�ģʽ,�ٴη�����ʼ����������Ƿ���ȷִ��
        I2C_GenerateSTART(I2C1, ENABLE);
        timeOut = EEPROM_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT)) {
            if ((timeOut--) == 0) {
                goto EEPROM_TIMEOUT_HANDLE;
            }
        }

        // ����оƬ��ַ(��ģʽ)
        I2C_Send7bitAddress(I2C1, DevAddr, I2C_Direction_Receiver);

        // �����ȡ���ֽ�������2���ֽ���ֱ�Ӷ�ȡ,������DMA
        if (Count < 2) {
            // ��鷢��оƬ��ַ�����Ƿ�ɹ�
            timeOut = EEPROM_FLAG_TIMEOUT;
            while (I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == RESET) {
                if ((timeOut--) == 0) {
                    goto EEPROM_TIMEOUT_HANDLE;
                }
            }

            // ������Ӧ��λNACK
            I2C_AcknowledgeConfig(I2C1, DISABLE);

            // ���ж�
            OS_ENTER_CRITICAL();

            // ͨ����ȡSR1��SR2�����ADDR�Ĵ���(SR1�Ѿ�������)
            (void)I2C1->SR2;

            // ����ֹͣ����
            I2C_GenerateSTOP(I2C1, ENABLE);

            // ���ж�
            OS_EXIT_CRITICAL();

            // �ȴ��յ�������
            timeOut = EEPROM_FLAG_TIMEOUT;
            while (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET) {
                if ((timeOut--) == 0) {
                    goto EEPROM_TIMEOUT_HANDLE;
                }
            }

            // ��ȡ��Eeprom���յ�������
            *BufPtr = I2C_ReceiveData(I2C1);

            // ���ֹͣλ�Ƿ�ִ�����
            timeOut = EEPROM_FLAG_TIMEOUT;
            while (I2C1->CR1 & I2C_CR1_STOP) {
                if ((timeOut--) == 0) {
                    goto EEPROM_TIMEOUT_HANDLE;
                }
            }

            // ������Ӧλ�Ա��´ν�������
            I2C_AcknowledgeConfig(I2C1, ENABLE);

            EepromDMADone = 1;
        } else {
            // ��鷢�͵�ַ�Ƿ�ִ�����
            timeOut = EEPROM_FLAG_TIMEOUT;
            while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) {
                if ((timeOut--) == 0) {
                    goto EEPROM_TIMEOUT_HANDLE;
                }
            }

            // ����DMA����ͨ��,����Ϊ��ַָ��Ͷ�ȡ������
            Eeprom_LowLevel_DMAConfig((uint32)BufPtr, Count, READ_ATTR);

            // ֪ͨDMA��һ��ֹͣ�����������һ��
            I2C_DMALastTransferCmd(I2C1, ENABLE);

            // ʹ��DMAͨ��
            DMA_Cmd(DMA1_Channel7, ENABLE);
        }
    }

    return SUCCESS;

EEPROM_TIMEOUT_HANDLE:              // Eeprom��д������ʱ����
    Eeprom_Timeout_UserCallback();
    EepromDMADone = 1;

    return ERROR;

}

/************************************************************************************************
* Function Name: Eeprom_Init
* Decription   : Eeprom��ʼ��
* Input        : ��
* Output       : ��
* Others       : ����DMAģʽ����
************************************************************************************************/
void Eeprom_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    // ����I2C1���ܲ���λ,I2C1ʱ�ӽ���
    I2C_Cmd(I2C1, DISABLE);
    I2C_DeInit(I2C1);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, DISABLE);

    // DMA1ͨ��6,7����
    DMA_Cmd(DMA1_Channel6, DISABLE);
    DMA_Cmd(DMA1_Channel7, DISABLE);

    // I2C1 IO��������ʱ��
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    // I2C1 GPIO���ų�ʼ��
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ʹ��DMA1ʱ�Ӳ���λͨ��6,7
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel6);
    DMA_DeInit(DMA1_Channel7);

    // ���ò�ʹ��I2C DMA TXͨ���ж�
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // ���ò�ʹ��I2C DMA RXͨ���ж�
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_Init(&NVIC_InitStructure);

    // ʹ��DMA1ͨ���ж�
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);
    DMA_ITConfig(DMA1_Channel7, DMA_IT_TC, ENABLE);

    // I2C��������
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = EEPROM_DEVADDR;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;

    // ʹ��I2C1��Ӧ������
    I2C_Cmd(I2C1, ENABLE);
    I2C_Init(I2C1, &I2C_InitStructure);

    // ʹ��I2C DMA����
    I2C_DMACmd(I2C1, ENABLE);
}

/************************************************************************************************
* Function Name: Eeprom_ReadWrite
* Decription   : Eeprom��д����
* Input        : BufPtr-���������д�����ݵ�ָ��;
*                SubAddr-оƬ�ڲ��ӵ�ַ;
*                Count-������д�������
*                AccessMode-����оƬģʽ
* Output       : �ɹ���ʧ��
* Others       : ����DMAģʽ����
************************************************************************************************/
bool Eeprom_ReadWrite(uint8 *BufPtr, uint32 SubAddr, uint16 Count, uint8 AccessMode)
{
    bool result, flag;
    uint8 err, addr, timeOut;
    uint16 num;

    if (Count == 0 ) {
        return ERROR;
    }

    // �ȴ���ȡEeprom�ķ���Ȩ��,�����ʱ��λEepromоƬ��I2c����
    OSMutexPend(EepromAccessMutex, TIME_DELAY_MS(10000), &err);
    if (OS_ERR_TIMEOUT == err) {
        Eeprom_Timeout_UserCallback();
    }
    flag = TRUE;
    while (flag) {
        OSSchedLock();                  // ��ֹ�������,������ܻ�Ӱ�쵽I2C��ʱ��
        EepromDMADone = 0;
        addr = (uint8)(SubAddr & (EEPROM_PAGESIZE - 1));
        if (addr + Count <= EEPROM_PAGESIZE) {
            result = Eeprom_AccessProc(EEPROM_DEVADDR, SubAddr, BufPtr, Count, AccessMode);
            flag = FALSE;
        } else {
            num = EEPROM_PAGESIZE - addr;
            result = Eeprom_AccessProc(EEPROM_DEVADDR, SubAddr, BufPtr, num, AccessMode);
            SubAddr += num;
            Count -= num;
            BufPtr += num;
        }
        OSSchedUnlock();
        timeOut = 0;
        while (0 == EepromDMADone && timeOut ++ < 200) {
            OSTimeDly(TIME_DELAY_MS(10));
        }

        if (WRITE_ATTR == AccessMode) {                     // Ҫ����20MS,����Eepromд,�˴������Ը�д
            OSTimeDly(TIME_DELAY_MS(20));
        }
        // DMA�ж��з����˴���
        if (2 == EepromDMADone) {
            Eeprom_Timeout_UserCallback();
            result = ERROR;
        }

        // ���ִ����жϴ���
        if (result == ERROR) {
            break;
        }
    }

    OSMutexPost(EepromAccessMutex);
    return result;
}

/***************************************End of file*********************************************/

