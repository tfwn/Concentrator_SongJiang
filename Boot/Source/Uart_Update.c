/************************************************************************************************
*                                   SRWF-6009-BOOT
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Uart_Update.c
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
#include "Uart_Update.h"
#include "Main.h"
#include "Timer.h"
#include "Uart.h"
#include "Hal.h"
#include "Flash.h"


/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
BOOT_PARAM_STRUCT Boot;


/************************************************************************************************
* Function Name: UartTxHandler
* Decription   : ���ڷ��ʹ�����
* Input        : �����������ݵ������ѡ���ʶ
* Output       : None
* Others       : ����0
************************************************************************************************/
static void UartTxHandler(uint8 Cmd, uint8 Option)
{
    uint8 len, buf[50];
    uint16 crc;

    len = 0;
    if (UART_ENTER_UPDATE_ACK == Cmd)
    {
        buf[len ++] = Cmd;
        buf[len ++] = 0x04;
        buf[len ++] = Option;
        buf[len ++] = 0;
    }
    else if (UART_UPDATEING_ACK == Cmd)
    {
        buf[len ++] = Cmd;
        buf[len ++] = 0x04;
        buf[len ++] = (uint8)Boot.CurPkgNum;
        buf[len ++] = (uint8)(Boot.CurPkgNum >> 8);
        buf[len ++] = Option;
        buf[len ++] = 0;
    }

    Uart1.TxLen = 0;
    Uart1.TxBuf[Uart1.TxLen ++] = 0x55;
    Uart1.TxBuf[Uart1.TxLen ++] = 0xAA;
    Uart1.TxBuf[Uart1.TxLen ++] = len;
    Uart1.TxLen += DataCopy(&Uart1.TxBuf[Uart1.TxLen], buf, len);
    crc = Cal_Crc(CRC_SEED, &Uart1.TxBuf[2], Uart1.TxLen - 2);
    Uart1.TxBuf[Uart1.TxLen ++] = (uint8)crc;
    Uart1.TxBuf[Uart1.TxLen ++] = (uint8)(crc >> 8);
    Uart_StartTx();
}

/************************************************************************************************
* Function Name: UartRxHandler
* Decription   : ���ڽ��մ�����
* Input        : None
* Output       : �������ݵ���ʼλ��,�����ȵ��Ǹ�λ��
* Others       : ����0
************************************************************************************************/
static uint8 UartRxHandler(void)
{
    uint8 i, len;
    uint16 crc;

    // ��������յ�������
    FEED_WATCHDOG
    if (Uart1.RxLen > 0 && 0 == Uart1.RxTimer)
    {
        for (i = 0; i < Uart1.RxLen; i++)
        {
            if (0x55 == Uart1.RxBuf[i] && 0xAA == Uart1.RxBuf[i + 1])
            {
                i += 2;
                break;
            }
        }
        if (i < Uart1.RxLen)
        {
            len = Uart1.RxBuf[i];
            crc = Uart1.RxBuf[i + len + 1] | (Uart1.RxBuf[i + len + 2] << 8 & 0xFF00);
            if (crc != Cal_Crc(CRC_SEED, & Uart1.RxBuf[i], len + 1))
            {
                Uart1.RxLen = 0;
                return 0xFF;
            }
            return i;
        }
        Uart1.RxLen = 0;
    }
    return 0;
}

/************************************************************************************************
* Function Name: Uart_UpdateProcess
* Decription   : �ж��Ƿ���UART����
* Input        : None
* Output       : None
* Others       : None
************************************************************************************************/
void Uart_UpdateProcess(void)
{
    uint8 ret, tmp, err, * p;
    uint16 cmd, ver, totalPkg;
    uint32 timer;

    // Boot������ʼ��
    Boot.SoftVersion = 0;
    Boot.CodeFilePkgNum = 0;
    Boot.AllPkgCRC1 = 0;
    Boot.AllPkgCRC2 = 0;
    Boot.AllPkgCRC3 = 0;
    Boot.AllPkgCRC4 = 0;
    Boot.CalAllPkgCRC = 0xFFFF;
    Boot.PkgCounter = 0;

    // ����Ƿ���Ҫ��������
    Uart1.RxLen = 0;
    timer = Timer1ms;

    do
    {
        ret = UartRxHandler();
        if (0xFF == ret)
        {
            err = 6;                                    // CRCУ�����
            UartTxHandler(UART_UPDATEING_ACK, err);
        }
        else if (ret > 0)
        {
            // ��ȡ�����汾����������
            p = &Uart1.RxBuf[ret + 1];
            cmd = *p;
            p += 2;
            ver = * p | (* (p + 1) << 8 & 0xFF00);
            p += 2;
            totalPkg = * p | (* (p + 1) << 8 & 0xFF00);
            p += 2;

            // �����汾(2)+��������(2)+CRC1(2)+CRC2(2)+CRC3(2)+CRC(4)
            if (UART_ENTER_UPDATE_CMD == cmd)
            {
                timer = Timer1ms;
                Boot.SoftVersion = ver;
                Boot.CodeFilePkgNum = totalPkg;
                Boot.AllPkgCRC1 = *p | (* (p + 1) << 8 & 0xFF00);
                p += 2;
                Boot.AllPkgCRC2 = *p | (* (p + 1) << 8 & 0xFF00);
                p += 2;
                Boot.AllPkgCRC3 = *p | (* (p + 1) << 8 & 0xFF00);
                p += 2;
                Boot.AllPkgCRC4 = *p | (* (p + 1) << 8 & 0xFF00);
                Boot.PkgCounter = 0;

                // ����APP���Flash
                tmp = (Boot.CodeFilePkgNum + 15) * 128 / FLASH_PAGE_SIZE;
                Flash_Erase(FLASH_APPCODE_START_ADDR, tmp);

                // ����Ӧ��֡
                UartTxHandler(UART_ENTER_UPDATE_ACK, 0);
            }

            // �����汾(2)+��������(2)+��ǰ���ݰ���(2)+���ݰ�����(128)
            else if (UART_UPDATEING_CMD == cmd)
            {
                timer = 0xFFFF;
                err = 0;
                Boot.CurPkgNum = * p | (* (p + 1) << 8 & 0xFF00);
                p += 2;
                if (Boot.SoftVersion != ver)
                {
                    err = 1;                            // �汾��ǰ��һ��
                }
                else if (Boot.CodeFilePkgNum != totalPkg)
                {
                    err = 2;                            // �����ļ�����ǰ��һ��
                }
                else if (Boot.PkgCounter != Boot.CurPkgNum)
                {
                    err = 3;                            // ��ǰ������������Ҫ�İ���
                }
                if (0 == err || 3 == err)
                {
                    if (0 == err)
                    {
                        Flash_Write(p, 128, FLASH_APPCODE_START_ADDR + Boot.CurPkgNum * 128);
                    }
                    if (FALSE == DataCompare(p, (uint8 *)(FLASH_APPCODE_START_ADDR + Boot.CurPkgNum * 128), 128))
                    {
                        err = 4;                        // дFLASHʱ����
                    }
                    else
                    {
                        err = 0;
                        Boot.PkgCounter = Boot.CurPkgNum + 1;
                        if (Boot.PkgCounter >= Boot.CodeFilePkgNum)
                        {
                            Boot.CalAllPkgCRC = Cal_Crc(CRC_SEED, (uint8 *)FLASH_APPCODE_START_ADDR, Boot.CodeFilePkgNum * 128);
                            if (Boot.AllPkgCRC1 == Boot.CalAllPkgCRC)
                            {
                                Timer_ActiveEvent(1000, SoftReset, Timer_ActiveResetMode);
                            }
                            else
                            {
                                err = 5;                // ����������У��ֵ����
                                Boot.PkgCounter --;
                            }
                        }
                    }
                }

                // ����Ӧ��֡
                UartTxHandler(UART_UPDATEING_ACK, err);
            }
            Uart1.RxLen = 0;
        }
        Timer_Handler();
    }while (Timer1ms - timer < 500 || 0xFFFF == timer);
}

/**************************************End of file**********************************************/


