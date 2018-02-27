/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Database.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        08/11/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define DATABASE_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Bsp.h"
#include "Main.h"
#include "Rtc.h"
#include "Gprs.h"
#include "Eeprom.h"
#include "Flash.h"
#include "Led.h"
#include "SerialPort.h"
#include "DataHandle.h"
#include "Database.h"
#include <string.h>

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
* Function Name: Data_Init
* Decription   : ���ݿ��ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Data_Init(void)
{
    Flash_LoadConcentratorInfo();
}

/************************************************************************************************
* Function Name: Data_RdWrConcChannelParam
* Decription   : ��д�������ŵ����÷�ʽ
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : �ŵ���ȡ(1)+�ŵ���(1)
************************************************************************************************/
void Data_RdWrConcChannelParam(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 err;

    if (Read_CONC_Channel_Param == DataFrmPtr->Command) {
		if( (Concentrator.EnableChannelSet > 0x1) || (Concentrator.TxRxChannel > 0xF) ){
			Concentrator.EnableChannelSet = 0x0;
			Concentrator.TxRxChannel = 0x3;
			//����Уʱ������,�����ڸı� 2e28 ģ���շ��ŵ���
			DataHandle_SetRtcTiming( (void *)0, 0x1 );
			if ( TRUE == Gprs.Online && Concentrator.EnableChannelSet == 0x1 ) {
				OSFlagPost(GlobalEventFlag, FLAG_GPRS_RESIGN_IN, OS_FLAG_SET, &err);
			}else if ( FALSE == Gprs.Online && Concentrator.EnableChannelSet == 0x1 ){
				DevResetTimer = 10000;
			}
			OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_DELAY_SAVE_TIMER, OS_FLAG_SET, &err);
		}
        DataFrmPtr->DataBuf[0] = Concentrator.EnableChannelSet;
        DataFrmPtr->DataBuf[1] = Concentrator.TxRxChannel;
        DataFrmPtr->DataLen = 2;
    } else {
		// ���������
 		if( (DataFrmPtr->DataBuf[0] > 0x1) || (Concentrator.TxRxChannel > 0xF) ){
			DataFrmPtr->DataBuf[0] = OP_Failure;
			DataFrmPtr->DataLen = 1;
			return ;
		}

		// ���κθı䣬���سɹ�
		if(DataFrmPtr->DataBuf[0] == Concentrator.EnableChannelSet &&
		   DataFrmPtr->DataBuf[1] == Concentrator.TxRxChannel){
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			return ;
		}

		Concentrator.EnableChannelSet = DataFrmPtr->DataBuf[0];
		Concentrator.TxRxChannel = DataFrmPtr->DataBuf[1];

		//����Уʱ������,�����ڸı� 2e28 ģ���շ��ŵ���
		DataHandle_SetRtcTiming( (void *)0, 0x1 );
		if ( TRUE == Gprs.Online && Concentrator.EnableChannelSet == 0x1 ) {
			OSFlagPost(GlobalEventFlag, FLAG_GPRS_RESIGN_IN, OS_FLAG_SET, &err);
 		}else if ( FALSE == Gprs.Online && Concentrator.EnableChannelSet == 0x1 ){
 			DevResetTimer = 10000;
 		}
		OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_DELAY_SAVE_TIMER, OS_FLAG_SET, &err);
		DataFrmPtr->DataBuf[0] = OP_Succeed;
		DataFrmPtr->DataLen = 1;
    }
}


/************************************************************************************************
* Function Name: Data_SetConcentratorAddr
* Decription   : ���� SIM ����,ֻ��ͨ���������ӵ�ͨ���趨
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:������ID��BCD��(11)
*                ����:����״̬(1)
************************************************************************************************/
void Data_SetSimNum(DATA_FRAME_STRUCT *DataFrmPtr)
{
	uint8 err;
    if (DataFrmPtr->PortNo != Usart_Debug && DataFrmPtr->PortNo != Usb_Port) {
        DataFrmPtr->DataBuf[0] = OP_NoFunction;
    } else {
        if (0 == memcmp(Concentrator.GprsParam.SimCardId, DataFrmPtr->DataBuf, 13)) {
			Gprs_OutputDebugMsg(FALSE, "\n--��ͬ�� SIM ����--\n");
            DataFrmPtr->DataBuf[0] = OP_Succeed;
        } else {
			memset(Concentrator.GprsParam.SimCardId, 0x20, 16);
			memcpy(Concentrator.GprsParam.SimCardId, DataFrmPtr->DataBuf, 13);
            Flash_SaveConcentratorInfo();
            DataFrmPtr->DataBuf[0] = OP_Succeed;
			Gprs_OutputDebugMsg(FALSE, "\n���� SIM ����Ϊ : ");
			OSTimeDlyHMSM(0, 0, 0, 500);
			Gprs_OutputDebugMsg(FALSE, Concentrator.GprsParam.SimCardId);
			Gprs_OutputDebugMsg(FALSE, "\n--���µ�¼������--\n");
			if (TRUE == Gprs.Online) {
				OSFlagPost(GlobalEventFlag, FLAG_GPRS_RESIGN_IN, OS_FLAG_SET, &err);
			}
        }
    }
    DataFrmPtr->DataLen = 1;
    return;
}

/************************************************************************************************
* Function Name: Data_SetConcentratorAddr
* Decription   : ���ü�������ַ,ֻ��ͨ���������ӵ�ͨ���趨
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:������ID��BCD��(6)
*                ����:����״̬(1)
************************************************************************************************/
void Data_SetConcentratorAddr(DATA_FRAME_STRUCT *DataFrmPtr)
{
    if (DataFrmPtr->PortNo != Usart_Debug && DataFrmPtr->PortNo != Usb_Port) {
        DataFrmPtr->DataBuf[0] = OP_NoFunction;
    } else {
        if (0 == memcmp(Concentrator.LongAddr, DataFrmPtr->DataBuf, LONG_ADDR_SIZE)) {
            DataFrmPtr->DataBuf[0] = OP_Succeed;
        } else if (NULL_U16_ID != Data_FindNodeId(0, DataFrmPtr->DataBuf)) {
            DataFrmPtr->DataBuf[0] = OP_ObjectRepetitive;
        } else {
            memcpy(Concentrator.LongAddr, DataFrmPtr->DataBuf, LONG_ADDR_SIZE);
            Flash_SaveConcentratorInfo();
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DevResetTimer = 10000;      // 10���Ӻ��豸��λ
        }
    }
    DataFrmPtr->DataLen = 1;
    return;
}

/************************************************************************************************
* Function Name: Data_FindNodeId
* Decription   : ��ָ��λ�ò���ָ�����ԵĽڵ�ID
* Input        : StartId-ָ�����ҵĿ�ʼλ��, BufPtr-ָ��ڵ㳤��ַ��ָ��
* Output       : ����Ҫ��Ľڵ�IDֵ
* Others       : ��
************************************************************************************************/
uint16 Data_FindNodeId(uint16 StartId, uint8 *BufPtr)
{
    uint16 i;

    if (0 == memcmp(BufPtr, Concentrator.LongAddr, LONG_ADDR_SIZE)) {
        return DATA_CENTER_ID;
    }
    if (0 == memcmp(BufPtr, NullAddress, LONG_ADDR_SIZE)) {
        return NULL_U16_ID;
    }
    for (i = StartId; i < Concentrator.MaxNodeId; i++) {
        if (0 == memcmp(BufPtr, SubNodes[i].LongAddr, LONG_ADDR_SIZE)) {
            return i;
        }
    }
    return NULL_U16_ID;
}


/************************************************************************************************
* Function Name: Data_GprsParameter
* Decription   : ��ȡ������Gprs�Ĳ�����Ϣ
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       :
*   ������:��������
*   ������:IP(4)+PORT(2)+����(1)+APN(n)+�û���(n)+����(n)
*   д����:IP(4)+PORT(2)+����(1)+APN(n)+�û���(n)+����(n)
*   д����:����״̬(1)
************************************************************************************************/
void Data_GprsParameter(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 *msg, *dataBufPtr;
    uint16 dataLen;
    GPRS_PARAMETER *gprsParamPtr;

    dataBufPtr = DataFrmPtr->DataBuf;
    gprsParamPtr = &Concentrator.GprsParam;
    msg = OSMemGetOpt(SmallMemoryPtr, 10, TIME_DELAY_MS(50));
    if ((void *)0 != msg) {
        // ��ѡIP(4)+��ѡPORT(2)+����IP(4)+����PORT(2)+��Ϣ�������(1)+APN����(1)+APN(N)+�û�������(1)+�û���(N)+���볤��(1)+����(N)
        memcpy(msg, gprsParamPtr->PriorDscIp, 4);
        ((uint16 *)(&msg[4]))[0] = gprsParamPtr->PriorDscPort;
        memcpy(&msg[6], gprsParamPtr->BackupDscIp, 4);
        ((uint16 *)(&msg[10]))[0] = gprsParamPtr->BackupDscPort;
        msg[12] = gprsParamPtr->HeatBeatPeriod;
        dataLen = 13;
        if ((msg[dataLen++] = strlen(gprsParamPtr->Apn)) > 0) {
            strcpy((char *)(msg + dataLen), gprsParamPtr->Apn);
            dataLen += msg[dataLen - 1];
        }
        if ((msg[dataLen++] = strlen(gprsParamPtr->Username)) > 0) {
            strcpy((char *)(msg + dataLen), gprsParamPtr->Username);
            dataLen += msg[dataLen - 1];
        }
        if ((msg[dataLen++] = strlen(gprsParamPtr->Password)) > 0) {
            strcpy((char *)(msg + dataLen), gprsParamPtr->Password);
            dataLen += msg[dataLen - 1];
        }
        if (Read_GPRS_Param == DataFrmPtr->Command) {
            // ����:��������
            // ����:��ѡIP(4)+��ѡPORT(2)+����IP(4)+����PORT(2)+����(1)+APN(n)+�û���(n)+����(n)
            memcpy(dataBufPtr, msg, dataLen);
        } else if(Write_GPRS_Param == DataFrmPtr->Command) {
            // ����:��ѡIP(4)+��ѡPORT(2)+����IP(4)+����PORT(2)+����(1)+APN����(1)+APN(n)+�û�������(1)+�û���(n)+���볤��(1)+����(n)
            // ����:����״̬(1)
            if (0 == memcmp(dataBufPtr, msg, dataLen) && dataLen == DataFrmPtr->DataLen) {
                *dataBufPtr = OP_Succeed;
            } else {
                memcpy(gprsParamPtr->PriorDscIp, dataBufPtr, 4);
                gprsParamPtr->PriorDscPort = *(uint16 *)(dataBufPtr + 4);
                memcpy(gprsParamPtr->BackupDscIp, dataBufPtr + 6, 4);
                gprsParamPtr->BackupDscPort = *(uint16 *)(dataBufPtr + 10);
                gprsParamPtr->HeatBeatPeriod = *(dataBufPtr + 12);
                if (gprsParamPtr->HeatBeatPeriod < 3) {
                    gprsParamPtr->HeatBeatPeriod = 3;
                }
                dataLen = 13;
                memset(gprsParamPtr->Apn, 0, sizeof(gprsParamPtr->Apn));
                memcpy(gprsParamPtr->Apn, dataBufPtr + dataLen + 1, *(dataBufPtr + dataLen));
                dataLen += *(dataBufPtr + dataLen) + 1;
                memset(gprsParamPtr->Username, 0, sizeof(gprsParamPtr->Username));
                memcpy(gprsParamPtr->Username, dataBufPtr + dataLen + 1, *(dataBufPtr + dataLen));
                dataLen += *(dataBufPtr + dataLen) + 1;
                memset(gprsParamPtr->Password, 0, sizeof(gprsParamPtr->Password));
                memcpy(gprsParamPtr->Password, dataBufPtr + dataLen + 1, *(dataBufPtr + dataLen));
                Flash_SaveConcentratorInfo();
                *dataBufPtr = OP_Succeed;
                DevResetTimer = 10000;      // 10���Ӻ��豸��λ
            }
            dataLen = 1;
        }
        OSMemPut(SmallMemoryPtr, msg);
    } else {
        *dataBufPtr = OP_Failure;
        dataLen = 1;
    }
    DataFrmPtr->DataLen = dataLen;
}

/************************************************************************************************
* Function Name: Data_SwUpdate
* Decription   : ������������
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����: Crc16(2)+д���ַ(4)+���������ܳ���(4)+�����������볤��(2)+��������(N)
*                ����: Crc16(2)+д���ַ(4)+�������(1)
************************************************************************************************/
void Data_SwUpdate(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint16 crc16, pkgCodeLen;
    uint32 writeAddr, codeLength;
    uint8 *codeBufPtr, *dataBufPtr;
    uint8 buf[12];

    // ��ȡ����
    dataBufPtr = DataFrmPtr->DataBuf;
    crc16 = ((uint16 *)dataBufPtr)[0];
    writeAddr = ((uint32 *)(dataBufPtr + 2))[0];
    codeLength = ((uint32 *)(dataBufPtr + 6))[0];
    pkgCodeLen = ((uint16 *)(dataBufPtr + 10))[0];
    codeBufPtr = dataBufPtr + 12;

    // ����������볤�ȴ���
    if (codeLength > FLASH_UPGRADECODE_SIZE * FLASH_PAGE_SIZE) {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // ����յ���д���ַΪ0,��ʾ��һ���µ�����Ҫ����
    if (0 == writeAddr) {
        Flash_Erase(FLASH_UPGRADECODE_START_ADDR, FLASH_UPGRADECODE_SIZE);
        Flash_Erase(FLASH_UPGRADE_INFO_START, FLASH_UPGRADE_INFO_SIZE);
        // ������Ϣ�����ʽ: Crc16(2)+�����ļ�����λ��(4)+���������ܳ���(4)+Crc16(2)
        memcpy(buf, dataBufPtr, sizeof(buf));
        ((uint32 *)(&buf[2]))[0] = FLASH_UPGRADECODE_START_ADDR;
        ((uint16 *)(&buf[10]))[0] = CalCrc16(buf, 10);
        Flash_Write(buf, 16, FLASH_UPGRADE_INFO_START);
    }

    // ��������У���ֽڻ����������ܳ��ȴ����򷵻ش���
    if (crc16 != ((uint16 *)FLASH_UPGRADE_INFO_START)[0] ||
        codeLength != ((uint32 *)(FLASH_UPGRADE_INFO_START + 6))[0]) {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // д����������
    if (codeLength >= writeAddr + pkgCodeLen) {
        if (0 != memcmp(codeBufPtr, (uint8 *)(FLASH_UPGRADECODE_START_ADDR + writeAddr), pkgCodeLen)) {
            Flash_Write(codeBufPtr, pkgCodeLen, FLASH_UPGRADECODE_START_ADDR + writeAddr);
            if (0 != memcmp(codeBufPtr, (uint8 *)(FLASH_UPGRADECODE_START_ADDR + writeAddr), pkgCodeLen)) {
                *(dataBufPtr + 6) = OP_Failure;
                DataFrmPtr->DataLen = 7;
                return;
            }
        }
    } else {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // ����Ƿ������һ��
    if (writeAddr + pkgCodeLen >= codeLength) {
        if (crc16 == CalCrc16((uint8 *)FLASH_UPGRADECODE_START_ADDR, codeLength)) {
            *(dataBufPtr + 6) = OP_Succeed;
            DevResetTimer = 10000;      // 10���Ӻ��豸��λ
        } else {
            *(dataBufPtr + 6) = OP_Failure;
        }
    } else {
        *(dataBufPtr + 6) = OP_Succeed;
    }
    DataFrmPtr->DataLen = 7;
    return;
}


/***************************************End of file*********************************************/

