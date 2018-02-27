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
* Decription   : 数据库初始化
* Input        : 无
* Output       : 无
* Others       : 无
************************************************************************************************/
void Data_Init(void)
{
    Flash_LoadConcentratorInfo();
}

/************************************************************************************************
* Function Name: Data_RdWrConcChannelParam
* Decription   : 读写集中器信道设置方式
* Input        : DataFrmPtr-指向数据帧的指针
* Output       : 无
* Others       : 信道获取(1)+信道号(1)
************************************************************************************************/
void Data_RdWrConcChannelParam(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 err;

    if (Read_CONC_Channel_Param == DataFrmPtr->Command) {
		if( (Concentrator.EnableChannelSet > 0x1) || (Concentrator.TxRxChannel > 0xF) ){
			Concentrator.EnableChannelSet = 0x0;
			Concentrator.TxRxChannel = 0x3;
			//发送校时命令到表端,仅用于改变 2e28 模块收发信道。
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
		// 错误的设置
 		if( (DataFrmPtr->DataBuf[0] > 0x1) || (Concentrator.TxRxChannel > 0xF) ){
			DataFrmPtr->DataBuf[0] = OP_Failure;
			DataFrmPtr->DataLen = 1;
			return ;
		}

		// 无任何改变，返回成功
		if(DataFrmPtr->DataBuf[0] == Concentrator.EnableChannelSet &&
		   DataFrmPtr->DataBuf[1] == Concentrator.TxRxChannel){
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			return ;
		}

		Concentrator.EnableChannelSet = DataFrmPtr->DataBuf[0];
		Concentrator.TxRxChannel = DataFrmPtr->DataBuf[1];

		//发送校时命令到表端,仅用于改变 2e28 模块收发信道。
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
* Decription   : 设置 SIM 卡号,只能通过物理连接的通道设定
* Input        : DataFrmPtr-指向数据帧的指针
* Output       : 无
* Others       : 下行:集中器ID的BCD码(11)
*                上行:操作状态(1)
************************************************************************************************/
void Data_SetSimNum(DATA_FRAME_STRUCT *DataFrmPtr)
{
	uint8 err;
    if (DataFrmPtr->PortNo != Usart_Debug && DataFrmPtr->PortNo != Usb_Port) {
        DataFrmPtr->DataBuf[0] = OP_NoFunction;
    } else {
        if (0 == memcmp(Concentrator.GprsParam.SimCardId, DataFrmPtr->DataBuf, 13)) {
			Gprs_OutputDebugMsg(FALSE, "\n--相同的 SIM 卡号--\n");
            DataFrmPtr->DataBuf[0] = OP_Succeed;
        } else {
			memset(Concentrator.GprsParam.SimCardId, 0x20, 16);
			memcpy(Concentrator.GprsParam.SimCardId, DataFrmPtr->DataBuf, 13);
            Flash_SaveConcentratorInfo();
            DataFrmPtr->DataBuf[0] = OP_Succeed;
			Gprs_OutputDebugMsg(FALSE, "\n设置 SIM 卡号为 : ");
			OSTimeDlyHMSM(0, 0, 0, 500);
			Gprs_OutputDebugMsg(FALSE, Concentrator.GprsParam.SimCardId);
			Gprs_OutputDebugMsg(FALSE, "\n--重新登录服务器--\n");
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
* Decription   : 设置集中器地址,只能通过物理连接的通道设定
* Input        : DataFrmPtr-指向数据帧的指针
* Output       : 无
* Others       : 下行:集中器ID的BCD码(6)
*                上行:操作状态(1)
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
            DevResetTimer = 10000;      // 10秒钟后设备复位
        }
    }
    DataFrmPtr->DataLen = 1;
    return;
}

/************************************************************************************************
* Function Name: Data_FindNodeId
* Decription   : 从指定位置查找指定属性的节点ID
* Input        : StartId-指定查找的开始位置, BufPtr-指向节点长地址的指针
* Output       : 符合要求的节点ID值
* Others       : 无
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
* Decription   : 读取或设置Gprs的参数信息
* Input        : DataFrmPtr-指向数据帧的指针
* Output       : 无
* Others       :
*   读下行:空数据域
*   读上行:IP(4)+PORT(2)+保留(1)+APN(n)+用户名(n)+密码(n)
*   写下行:IP(4)+PORT(2)+保留(1)+APN(n)+用户名(n)+密码(n)
*   写上行:操作状态(1)
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
        // 首选IP(4)+首选PORT(2)+备用IP(4)+备用PORT(2)+信息输出类型(1)+APN长度(1)+APN(N)+用户名长度(1)+用户名(N)+密码长度(1)+密码(N)
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
            // 下行:空数据域
            // 上行:首选IP(4)+首选PORT(2)+备用IP(4)+备用PORT(2)+保留(1)+APN(n)+用户名(n)+密码(n)
            memcpy(dataBufPtr, msg, dataLen);
        } else if(Write_GPRS_Param == DataFrmPtr->Command) {
            // 下行:首选IP(4)+首选PORT(2)+备用IP(4)+备用PORT(2)+保留(1)+APN长度(1)+APN(n)+用户名长度(1)+用户名(n)+密码长度(1)+密码(n)
            // 上行:操作状态(1)
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
                DevResetTimer = 10000;      // 10秒钟后设备复位
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
* Decription   : 程序升级函数
* Input        : DataFrmPtr-指向数据帧的指针
* Output       : 无
* Others       : 下行: Crc16(2)+写入地址(4)+升级代码总长度(4)+本包升级代码长度(2)+升级代码(N)
*                上行: Crc16(2)+写入地址(4)+操作结果(1)
************************************************************************************************/
void Data_SwUpdate(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint16 crc16, pkgCodeLen;
    uint32 writeAddr, codeLength;
    uint8 *codeBufPtr, *dataBufPtr;
    uint8 buf[12];

    // 提取数据
    dataBufPtr = DataFrmPtr->DataBuf;
    crc16 = ((uint16 *)dataBufPtr)[0];
    writeAddr = ((uint32 *)(dataBufPtr + 2))[0];
    codeLength = ((uint32 *)(dataBufPtr + 6))[0];
    pkgCodeLen = ((uint16 *)(dataBufPtr + 10))[0];
    codeBufPtr = dataBufPtr + 12;

    // 如果升级代码长度错误
    if (codeLength > FLASH_UPGRADECODE_SIZE * FLASH_PAGE_SIZE) {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // 如果收到的写入地址为0,表示有一个新的升级要进行
    if (0 == writeAddr) {
        Flash_Erase(FLASH_UPGRADECODE_START_ADDR, FLASH_UPGRADECODE_SIZE);
        Flash_Erase(FLASH_UPGRADE_INFO_START, FLASH_UPGRADE_INFO_SIZE);
        // 升级信息保存格式: Crc16(2)+升级文件保存位置(4)+升级代码总长度(4)+Crc16(2)
        memcpy(buf, dataBufPtr, sizeof(buf));
        ((uint32 *)(&buf[2]))[0] = FLASH_UPGRADECODE_START_ADDR;
        ((uint16 *)(&buf[10]))[0] = CalCrc16(buf, 10);
        Flash_Write(buf, 16, FLASH_UPGRADE_INFO_START);
    }

    // 如果程序的校验字节或升级代码总长度错误则返回错误
    if (crc16 != ((uint16 *)FLASH_UPGRADE_INFO_START)[0] ||
        codeLength != ((uint32 *)(FLASH_UPGRADE_INFO_START + 6))[0]) {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // 写入升级代码
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

    // 检查是否是最后一包
    if (writeAddr + pkgCodeLen >= codeLength) {
        if (crc16 == CalCrc16((uint8 *)FLASH_UPGRADECODE_START_ADDR, codeLength)) {
            *(dataBufPtr + 6) = OP_Succeed;
            DevResetTimer = 10000;      // 10秒钟后设备复位
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

