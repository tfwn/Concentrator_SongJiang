/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : DataHandle.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        08/12/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define DATAHANDLE_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Bsp.h"
#include "Main.h"
#include "Rtc.h"
#include "Timer.h"
#include "SerialPort.h"
#include "Gprs.h"
#include "Flash.h"
#include "Eeprom.h"
#include "DataHandle.h"
#include "Database.h"
#include <string.h>


/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
uint8 PkgNo;
PORT_NO MonitorPort = Usart_Debug;                      // 监控端口
uint16 RTCTimingTimer = 60;                             // RTC校时任务启动定时器
uint16 GprsDelayDataUpTimer = 60;						// Gprs 缓存数据延时上传定时器
TASK_STATUS_STRUCT TaskRunStatus;                       // 任务运行状态
DATA_HANDLE_TASK DataHandle_TaskArry[MAX_DATA_HANDLE_TASK_NUM];
const uint8 Uart_RfTx_Filter[] = {SYNCWORD1, SYNCWORD2};
const uint8 DayMaskTab[] = {0xF0, 0xE0, 0xC0, 0x80};
uint8 GetRtcOK = 0;
uint8 SetRtcNum = 0;
uint8 GprsDebug = DISABLE;
uint8 SwitchMonitor_EN = DISABLE;
uint8 GprsSoftUp = 0;
extern RTC_TIME RtcTime;


/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: DataHandle_GetEmptyTaskPtr
* Decription   : 在任务队列中搜索空的任务指针
* Input        : 无
* Output       : 任务的指针
* Others       : 无
************************************************************************************************/
DATA_HANDLE_TASK *DataHandle_GetEmptyTaskPtr(void)
{
    uint8 i;

    // 搜索未被占用的空间,创建数据上传任务
    for (i = 0; i < MAX_DATA_HANDLE_TASK_NUM; i++) {
        if ((void *)0 == DataHandle_TaskArry[i].StkPtr) {
            return (&DataHandle_TaskArry[i]);
        }
    }

    // 任务队列全部使用返回空队列
    return ((void *)0);
}

/************************************************************************************************
* Function Name: DataHandle_SetPkgProperty
* Decription   : 设置包属性值
* Input        : PkgXor-报文与运营商编码不异或标志: 0-不异或 1-异或
*                NeedAck-是否需要回执 0-不需回执 1-需要回执
*                PkgType-帧类型 0-命令帧 1-应答帧
*                Dir-上下行标识 0-下行 1-上行
* Output       : 属性值
* Others       : 无
************************************************************************************************/
PKG_PROPERTY DataHandle_SetPkgProperty(bool PkgXor, bool NeedAck, bool PkgType, bool Dir)
{
    PKG_PROPERTY pkgProp;

    pkgProp.Content = 0;
    pkgProp.PkgXor = PkgXor;
    pkgProp.NeedAck = NeedAck;
    pkgProp.Encrypt = Concentrator.Param.DataEncryptCtrl;
    pkgProp.PkgType = PkgType;
    pkgProp.Direction = Dir;
    return pkgProp;
}

/************************************************************************************************
* Function Name: DataHandle_SetPkgPath
* Decription   : 设置数据包的路径
* Input        : DataFrmPtr-数据指针
*                ReversePath-是否需要翻转路径
* Output       : 无
* Others       : 无
************************************************************************************************/
void DataHandle_SetPkgPath(DATA_FRAME_STRUCT *DataFrmPtr, bool ReversePath)
{
    uint8 i, tmpBuf[LONG_ADDR_SIZE];

    if (0 == memcmp(BroadcastAddrIn, DataFrmPtr->Route[DataFrmPtr->RouteInfo.CurPos], LONG_ADDR_SIZE)) {
        memcpy(DataFrmPtr->Route[DataFrmPtr->RouteInfo.CurPos], Concentrator.LongAddr, LONG_ADDR_SIZE);
    }
    // 路径是否翻转处理
    if (REVERSED == ReversePath) {
        DataFrmPtr->RouteInfo.CurPos = DataFrmPtr->RouteInfo.Level - 1 - DataFrmPtr->RouteInfo.CurPos;
        for (i = 0; i < DataFrmPtr->RouteInfo.Level / 2; i++) {
            memcpy(tmpBuf, DataFrmPtr->Route[i], LONG_ADDR_SIZE);
            memcpy(DataFrmPtr->Route[i], DataFrmPtr->Route[DataFrmPtr->RouteInfo.Level - 1 - i], LONG_ADDR_SIZE);
            memcpy(DataFrmPtr->Route[DataFrmPtr->RouteInfo.Level - 1 - i], tmpBuf, LONG_ADDR_SIZE);
        }
    }
}

/************************************************************************************************
* Function Name: AJL_DataHandle_CreateTxData
* Decription   : 创建发送数据包,安居利专用
* Input        : DataFrmPtr-待发送的数据
* Output       : 成功或错误
* Others       : 该函数执行完毕后会释放DataBufPtr指向的存储区,还会将路由区的地址排列翻转
************************************************************************************************/
ErrorStatus AJL_DataHandle_CreateTxData(DATA_FRAME_STRUCT *DataFrmPtr, uint8 PreambleLen)
{
    uint8 err;
    uint16 tmp;
    PORT_BUF_FORMAT *txPortBufPtr;

    // 先申请一个内存用于中间数据处理
    if ((void *)0 == (txPortBufPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        OSMemPut(LargeMemoryPtr, DataFrmPtr);
        return ERROR;
    }
    txPortBufPtr->Property.PortNo = DataFrmPtr->PortNo;
    txPortBufPtr->Property.FilterDone = 1;
    memcpy(txPortBufPtr->Buffer, Uart_RfTx_Filter, sizeof(Uart_RfTx_Filter));
    txPortBufPtr->Length = sizeof(Uart_RfTx_Filter);

    tmp = txPortBufPtr->Length;
    DataFrmPtr->PkgLength = DataFrmPtr->DataLen + DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE + DATA_FIXED_AREA_LENGTH;
    ((uint16 *)(&txPortBufPtr->Buffer[txPortBufPtr->Length]))[0] = DataFrmPtr->PkgLength;
    txPortBufPtr->Length += 2;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgProp.Content;         // 报文标识
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgSn;                   // 任务号
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Command;                 // 命令字
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->DeviceType;          // 设备类型
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Life_Ack.Content;        // 生命周期和应答信道
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->RouteInfo.Content;       // 路径信息
    memcpy(&txPortBufPtr->Buffer[txPortBufPtr->Length], DataFrmPtr->Route[0], DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE);
    txPortBufPtr->Length += DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE;
    memcpy(txPortBufPtr->Buffer + txPortBufPtr->Length, DataFrmPtr->DataBuf, DataFrmPtr->DataLen);      // 数据域
    txPortBufPtr->Length += DataFrmPtr->DataLen;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // 下行信号强度
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // 上行信号强度
    txPortBufPtr->Buffer[txPortBufPtr->Length] = CalCrc8((uint8 *)(&txPortBufPtr->Buffer[tmp]), txPortBufPtr->Length - tmp);     // Crc8校验
    txPortBufPtr->Length += 1;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = TAILBYTE;

    if (CMD_PKG == DataFrmPtr->PkgProp.PkgType) {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = PreambleLen;
    } else {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x00;
    }
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = Concentrator.TxRxChannel;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = Concentrator.TxRxChannel;

    OSMemPut(LargeMemoryPtr, DataFrmPtr);
    if (Uart_Gprs == txPortBufPtr->Property.PortNo) {
        if (FALSE == Gprs.Online ||
            OS_ERR_NONE != OSMboxPost(Gprs.MboxTx, txPortBufPtr)) {
            OSMemPut(LargeMemoryPtr, txPortBufPtr);
            return ERROR;
        } else {
            OSFlagPost(GlobalEventFlag, FLAG_GPRS_TX, OS_FLAG_SET, &err);
            return SUCCESS;
        }
    } else {
        if (txPortBufPtr->Property.PortNo < Port_Total &&
            OS_ERR_NONE != OSMboxPost(SerialPort.Port[txPortBufPtr->Property.PortNo].MboxTx, txPortBufPtr)) {
            OSMemPut(LargeMemoryPtr, txPortBufPtr);
            return ERROR;
        } else {
            OSFlagPost(GlobalEventFlag, (OS_FLAGS)(1 << txPortBufPtr->Property.PortNo + SERIALPORT_TX_FLAG_OFFSET), OS_FLAG_SET, &err);
            return SUCCESS;
        }
    }
}

bool DataHandle_SetRtcTiming(DATA_FRAME_STRUCT *dat, uint8 PreambleLen)
{
    RTC_TIME rtcTimer;
    DATA_FRAME_STRUCT *DataFrmPtr;
    uint16 crc16 = 0;
    if ((void *)0 == (DataFrmPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return ERROR;
    }

    Rtc_Get(&rtcTimer, Format_Bcd);

    // 创建转发
    DataFrmPtr->PortNo = Usart_Rf;
    DataFrmPtr->Command = Read_Songjiang_Meter_data;
    DataFrmPtr->DeviceType = 0x00;
    DataFrmPtr->PkgSn = PkgNo++;
    DataFrmPtr->Life_Ack.LifeCycle = 0x02;
    DataFrmPtr->Life_Ack.AckChannel = Concentrator.TxRxChannel;//DEFAULT_RX_CHANNEL;
    DataFrmPtr->RouteInfo.Level = 2;
    memcpy(DataFrmPtr->Route[0], Concentrator.LongAddr, LONG_ADDR_SIZE);
    memcpy(DataFrmPtr->Route[1], dat->Route[0], LONG_ADDR_SIZE);
    DataFrmPtr->RouteInfo.CurPos = 0;

    DataFrmPtr->DataBuf[0] = 0x09;// 命令长度
    DataFrmPtr->DataBuf[1] = 0x01;// 校时命令
    DataFrmPtr->DataBuf[2] = BcdToBin((uint8)((rtcTimer.Year>>8)&0xFF));// 年 ，实际年份 - 2000后的 Hex 值
    DataFrmPtr->DataBuf[3] = BcdToBin(rtcTimer.Month);// 月
    DataFrmPtr->DataBuf[4] = BcdToBin(rtcTimer.Day);// 日
    DataFrmPtr->DataBuf[5] = BcdToBin(rtcTimer.Hour);// 时
    DataFrmPtr->DataBuf[6] = BcdToBin(rtcTimer.Minute);// 分
    DataFrmPtr->DataBuf[7] = BcdToBin(rtcTimer.Second);// 秒
    crc16 = phyCalCRC16(DataFrmPtr->DataBuf, DataFrmPtr->DataBuf[0] - 1);
    DataFrmPtr->DataBuf[8] = (uint8)((crc16 >> 8)&0xFF);// crc16校验码 [2]
    DataFrmPtr->DataBuf[9] = (uint8)((crc16)&0xFF);
    DataFrmPtr->DataLen = 10;

    DataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NONE_ACK, CMD_PKG, DOWN_DIR);
    DataHandle_SetPkgPath(DataFrmPtr, UNREVERSED);
    AJL_DataHandle_CreateTxData(DataFrmPtr, PreambleLen);

    return NONE_ACK;

}

/************************************************************************************************
* Function Name: DataHandle_GprsExtractData
* Decription   : 按协议提取出数据并检验数据的正确性
* Input        : BufPtr-原数据指针
* Output       : 成功或错误说明
* Others       : 注意-成功调用此函数后BufPtr指向提取数据后的内存
************************************************************************************************/
bool DataHandle_GprsExtractData(uint8 *BufPtr)
{
	uint8 len, err, ret;
	GPRS_PORT_BUF_FORMAT *gprsPortBufPtr;
	uint16 crc16;

	// 按协议格式提取相应的数据
	gprsPortBufPtr = (GPRS_PORT_BUF_FORMAT *)BufPtr;
	len = gprsPortBufPtr->Buf[0];
	crc16 = phyCalCRC16(&gprsPortBufPtr->Buf[0], len-1);
	if((gprsPortBufPtr->Buf[len-1] != (uint8)((crc16 >> 8)&0xFF)) || (gprsPortBufPtr->Buf[len] != (uint8)((crc16)&0xFF))){
		//crc16 错误
		//Gprs_OutputDebugMsg(FALSE, "\n -- Grps command crc16 error --\n");
		return ERROR;
	}
	if( ( 0x0D == gprsPortBufPtr->Buf[0] && 0x01 == gprsPortBufPtr->Buf[1] )
		|| ( 0x11 == gprsPortBufPtr->Buf[0] && 0x04 == gprsPortBufPtr->Buf[1] )){

		if( 0x0D == gprsPortBufPtr->Buf[0] && 0x01 == gprsPortBufPtr->Buf[1] ){
			RtcTime.Year = 2000 + (gprsPortBufPtr->Buf[6]);
			RtcTime.Month = (gprsPortBufPtr->Buf[7]);
			RtcTime.Day = (gprsPortBufPtr->Buf[8]);
			RtcTime.Hour = (gprsPortBufPtr->Buf[9]);
			RtcTime.Minute = (gprsPortBufPtr->Buf[10]);
			RtcTime.Second = (gprsPortBufPtr->Buf[11]);
			ret = 0;
			if (RtcTime.Year < 2017 || RtcTime.Year > 2027 ||
		        RtcTime.Month == 0 || RtcTime.Month > 12 ||
		        RtcTime.Day == 0 || RtcTime.Day > 31 ||
		        RtcTime.Hour >= 24 || RtcTime.Minute >= 60 || RtcTime.Second >= 60) {
		        ret = 1;
		    }

			if ((SUCCESS == Gprs_Rtc_Set(RtcTime, Format_Bin, 0, 1)) && (0 == ret)) {
				SetRtcNum = 0;
				GetRtcOK = 1;
				//Gprs_OutputDebugMsg(TRUE, "\n ---- RTC 校时成功 ----\n");

				memcpy( Concentrator.CustomerName, &gprsPortBufPtr->Buf[2], 2);
				//表端和集中器采用公司代码第一个字节的高4为作为信道号。
				if( 0x1 == Concentrator.EnableChannelSet ){
					Concentrator.TxRxChannel = (Concentrator.CustomerName[0]>>4) & 0xF;
					//发送校时命令到表端,仅用于改变 2e28 模块收发信道。
					DataHandle_SetRtcTiming( (void *)0, 0x1 );
					OSTimeDlyHMSM(0, 0, 0, 200);
					// 延时后保存保存集中器参数信息
					OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_DELAY_SAVE_TIMER, OS_FLAG_SET, &err);
				}
			} else {
				SetRtcNum++;
				if(SetRtcNum == 3){
					GetRtcOK = 0;
				}
				Gprs_OutputDebugMsg(TRUE, "\n ---- RTC 重新登陆服务器校时 ----\n");
				if (TRUE == Gprs.Online && SetRtcNum < 3) {
					OSFlagPost(GlobalEventFlag, FLAG_GPRS_RESIGN_IN, OS_FLAG_SET, &err);
				}
			}
		}

	}
	return SUCCESS;
}

/************************************************************************************************
* Function Name: DataHandle_ExtractData
* Decription   : 按协议提取出数据并检验数据的正确性
* Input        : BufPtr-原数据指针
* Output       : 成功或错误说明
* Others       : 注意-成功调用此函数后BufPtr指向提取数据后的内存
************************************************************************************************/
EXTRACT_DATA_RESULT DataHandle_ExtractData(uint8 *BufPtr)
{
    uint8 i, *msg;
    uint16 tmp;
    PORT_BUF_FORMAT *portBufPtr;
    DATA_FRAME_STRUCT *dataFrmPtr;

    // 按协议格式提取相应的数据
    portBufPtr = (PORT_BUF_FORMAT *)BufPtr;
    if (FALSE == portBufPtr->Property.FilterDone) {
        return Error_Data;
    }
    // 申请一个内存用于存放提取后的数据
    if ((void *)0 == (msg = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return Error_GetMem;
    }
    dataFrmPtr = (DATA_FRAME_STRUCT *)msg;
    dataFrmPtr->PortNo = portBufPtr->Property.PortNo;
    dataFrmPtr->PkgLength = ((uint16 *)portBufPtr->Buffer)[0] & 0x03FF;
    dataFrmPtr->PkgProp.Content = portBufPtr->Buffer[2];
    dataFrmPtr->PkgSn = portBufPtr->Buffer[3];
    dataFrmPtr->Command = (COMMAND_TYPE)(portBufPtr->Buffer[4]);

    dataFrmPtr->DeviceType = portBufPtr->Buffer[5];
    dataFrmPtr->Life_Ack.Content = portBufPtr->Buffer[6];
    dataFrmPtr->RouteInfo.Content = portBufPtr->Buffer[7];
    for (i = 0; i < dataFrmPtr->RouteInfo.Level && i < MAX_ROUTER_NUM; i++) {
        memcpy(dataFrmPtr->Route[i], &portBufPtr->Buffer[8 + LONG_ADDR_SIZE * i], LONG_ADDR_SIZE);
    }
    dataFrmPtr->DownRssi = *(portBufPtr->Buffer + dataFrmPtr->PkgLength- 4);
    dataFrmPtr->UpRssi = *(portBufPtr->Buffer + dataFrmPtr->PkgLength - 3);
    dataFrmPtr->Crc8 = *(portBufPtr->Buffer + dataFrmPtr->PkgLength - 2);
    tmp = LONG_ADDR_SIZE * dataFrmPtr->RouteInfo.Level + DATA_FIXED_AREA_LENGTH;
    if (dataFrmPtr->PkgLength < tmp || dataFrmPtr->PkgLength > MEM_LARGE_BLOCK_LEN - 1) {
        OSMemPut(LargeMemoryPtr, msg);
        return Error_DataLength;
    }
    dataFrmPtr->DataLen = dataFrmPtr->PkgLength - tmp;
    if (dataFrmPtr->DataLen < MEM_LARGE_BLOCK_LEN - sizeof(DATA_FRAME_STRUCT)) {
        memcpy(dataFrmPtr->DataBuf, portBufPtr->Buffer + 8 + LONG_ADDR_SIZE * dataFrmPtr->RouteInfo.Level, dataFrmPtr->DataLen);
    } else {
        OSMemPut(LargeMemoryPtr, msg);
        return Error_DataOverFlow;
    }

    // 检查Crc8是否正确
    if (dataFrmPtr->Crc8 != CalCrc8(portBufPtr->Buffer, dataFrmPtr->PkgLength - 2) || portBufPtr->Length < dataFrmPtr->PkgLength) {
        OSMemPut(LargeMemoryPtr, msg);
        return Error_DataCrcCheck;
    }
    // 检查结束符是否是 0x16
    if ( 0x16 != *(portBufPtr->Buffer + dataFrmPtr->PkgLength - 1)) {
        OSMemPut(LargeMemoryPtr, msg);
        return Error_Data;
    }

    // 检查是否为广播地址或本机地址
    dataFrmPtr->RouteInfo.CurPos += 1;
    if ((0 == memcmp(Concentrator.LongAddr, dataFrmPtr->Route[dataFrmPtr->RouteInfo.CurPos], LONG_ADDR_SIZE) ||
        0 == memcmp(BroadcastAddrIn, dataFrmPtr->Route[dataFrmPtr->RouteInfo.CurPos], LONG_ADDR_SIZE)) &&
        dataFrmPtr->RouteInfo.CurPos < dataFrmPtr->RouteInfo.Level) {
        memcpy(BufPtr, msg, MEM_LARGE_BLOCK_LEN);
        OSMemPut(LargeMemoryPtr, msg);
        return Ok_Data;
    }

    // 要进行后续处理,所以此处将提取出的数据返回
    memcpy(BufPtr, msg, MEM_LARGE_BLOCK_LEN);
    OSMemPut(LargeMemoryPtr, msg);
    return Error_DstAddress;
}


/************************************************************************************************
* Function Name: DataHandle_CreateTxData
* Decription   : 创建发送数据包
* Input        : DataFrmPtr-待发送的数据
* Output       : 成功或错误
* Others       : 该函数执行完毕后会释放DataBufPtr指向的存储区,还会将路由区的地址排列翻转
************************************************************************************************/
ErrorStatus DataHandle_CreateTxData(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 err;
    uint16 tmp, nodeId;
    PORT_BUF_FORMAT *txPortBufPtr;

    // 先申请一个内存用于中间数据处理
    if ((void *)0 == (txPortBufPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        OSMemPut(LargeMemoryPtr, DataFrmPtr);
        return ERROR;
    }
    txPortBufPtr->Property.PortNo = DataFrmPtr->PortNo;
    txPortBufPtr->Property.FilterDone = 1;
    memcpy(txPortBufPtr->Buffer, Uart_RfTx_Filter, sizeof(Uart_RfTx_Filter));
    txPortBufPtr->Length = sizeof(Uart_RfTx_Filter);

    tmp = txPortBufPtr->Length;
    DataFrmPtr->PkgLength = DataFrmPtr->DataLen + DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE + DATA_FIXED_AREA_LENGTH;
    ((uint16 *)(&txPortBufPtr->Buffer[txPortBufPtr->Length]))[0] = DataFrmPtr->PkgLength;
    txPortBufPtr->Length += 2;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgProp.Content;         // 报文标识
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgSn;                   // 任务号
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Command;                 // 命令字
    // 为了和已出货的第一批表兼容
    if (0 == DataFrmPtr->Life_Ack.AckChannel) {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = Dev_Server;                      // 设备类型
    } else {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->DeviceType;          // 设备类型
    }
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Life_Ack.Content;        // 生命周期和应答信道
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->RouteInfo.Content;       // 路径信息
    memcpy(&txPortBufPtr->Buffer[txPortBufPtr->Length], DataFrmPtr->Route[0], DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE);
    txPortBufPtr->Length += DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE;
    memcpy(txPortBufPtr->Buffer + txPortBufPtr->Length, DataFrmPtr->DataBuf, DataFrmPtr->DataLen);      // 数据域
    txPortBufPtr->Length += DataFrmPtr->DataLen;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // 下行信号强度
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // 上行信号强度
    txPortBufPtr->Buffer[txPortBufPtr->Length] = CalCrc8((uint8 *)(&txPortBufPtr->Buffer[tmp]), txPortBufPtr->Length - tmp);     // Crc8校验
    txPortBufPtr->Length += 1;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = TAILBYTE;

    if (CMD_PKG == DataFrmPtr->PkgProp.PkgType) {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x1E;
        nodeId = Data_FindNodeId(0, DataFrmPtr->Route[DataFrmPtr->RouteInfo.Level - 1]);
        if (DATA_CENTER_ID == nodeId || NULL_U16_ID == nodeId) {
            txPortBufPtr->Buffer[txPortBufPtr->Length++] = DEFAULT_TX_CHANNEL;
        } else {
            txPortBufPtr->Buffer[txPortBufPtr->Length++] = SubNodes[nodeId].RxChannel;
        }
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = (DEFAULT_RX_CHANNEL + CHANNEL_OFFSET);
    } else {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x00;
        // 为了和已出货的第一批表兼容
        if (0 == DataFrmPtr->Life_Ack.AckChannel) {
            txPortBufPtr->Buffer[txPortBufPtr->Length++] = (DEFAULT_RX_CHANNEL + CHANNEL_OFFSET);
        } else {
            txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Life_Ack.AckChannel;
        }
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = (DEFAULT_RX_CHANNEL + CHANNEL_OFFSET);
    }

    OSMemPut(LargeMemoryPtr, DataFrmPtr);
    if (Uart_Gprs == txPortBufPtr->Property.PortNo) {
        if (FALSE == Gprs.Online ||
            OS_ERR_NONE != OSMboxPost(Gprs.MboxTx, txPortBufPtr)) {
            OSMemPut(LargeMemoryPtr, txPortBufPtr);
            return ERROR;
        } else {
            OSFlagPost(GlobalEventFlag, FLAG_GPRS_TX, OS_FLAG_SET, &err);
            return SUCCESS;
        }
    } else {
        if (txPortBufPtr->Property.PortNo < Port_Total &&
            OS_ERR_NONE != OSMboxPost(SerialPort.Port[txPortBufPtr->Property.PortNo].MboxTx, txPortBufPtr)) {
            OSMemPut(LargeMemoryPtr, txPortBufPtr);
            return ERROR;
        } else {
            OSFlagPost(GlobalEventFlag, (OS_FLAGS)(1 << txPortBufPtr->Property.PortNo + SERIALPORT_TX_FLAG_OFFSET), OS_FLAG_SET, &err);
            return SUCCESS;
        }
    }
}


/************************************************************************************************
* Function Name: DataHandle_DataDelaySaveProc
* Decription   : 数据延时保存处理函数
* Input        : 无
* Output       : 无
* Others       : 当多组数据需要保存时启动一次延时保存,以延长Flash的寿命
************************************************************************************************/
void DataHandle_DataDelaySaveProc(void)
{
    Flash_SaveConcentratorInfo();
}

/************************************************************************************************
* Function Name: DataHandle_OutputMonitorMsg
* Decription   : 集中器主动输出监控信息
* Input        : MsgType-信息的类型,MsgPtr-输出信息指针,MsgLen-信息的长度
* Output       : 无
* Others       : 无
************************************************************************************************/
void DataHandle_OutputMonitorMsg(MONITOR_MSG_TYPE MsgType, uint8 *MsgPtr, uint16 MsgLen)
{
    DATA_FRAME_STRUCT *dataFrmPtr;

    if ((void *)0 == (dataFrmPtr = OSMemGetOpt(LargeMemoryPtr, 20, TIME_DELAY_MS(50)))) {
        return;
    }
    dataFrmPtr->PortNo = MonitorPort;
    dataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NONE_ACK, CMD_PKG, UP_DIR);
    dataFrmPtr->PkgSn = PkgNo++;
    dataFrmPtr->Command = Output_Monitior_Msg_Cmd;
    dataFrmPtr->DeviceType = Dev_Concentrator;
    dataFrmPtr->Life_Ack.Content = 0x0F;
    dataFrmPtr->RouteInfo.CurPos = 0;
    dataFrmPtr->RouteInfo.Level = 2;
    memcpy(dataFrmPtr->Route[0], Concentrator.LongAddr, LONG_ADDR_SIZE);
    memcpy(dataFrmPtr->Route[1], BroadcastAddrOut, LONG_ADDR_SIZE);
    dataFrmPtr->DataBuf[0] = MsgType;
    memcpy(&dataFrmPtr->DataBuf[1], MsgPtr, MsgLen);
    dataFrmPtr->DataLen = 1 + MsgLen;
    DataHandle_SetPkgPath(dataFrmPtr, UNREVERSED);
    DataHandle_CreateTxData(dataFrmPtr);
    return;
}




/************************************************************************************************
* Function Name: DataHandle_RTCTimingTask
* Decription   : 实时时钟校时处理任务
* Input        : *p_arg-参数指针
* Output       : 无
* Others       : 无
************************************************************************************************/
void DataHandle_RTCTimingTask(void *p_arg)
{
    uint8 err;
    DATA_HANDLE_TASK *taskPtr;
    DATA_FRAME_STRUCT *txDataFrmPtr, *rxDataFrmPtr;

    // 创建上行校时数据包
    TaskRunStatus.RTCTiming = TRUE;
    if ((void *)0 != (txDataFrmPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        txDataFrmPtr->PortNo = Uart_Gprs;
        txDataFrmPtr->PkgLength = DATA_FIXED_AREA_LENGTH;
        txDataFrmPtr->PkgSn = PkgNo++;
        txDataFrmPtr->Command = CONC_RTC_Timing;
        txDataFrmPtr->DeviceType = Dev_Concentrator;
        txDataFrmPtr->Life_Ack.Content = 0x0F;
        txDataFrmPtr->RouteInfo.CurPos = 0;
        txDataFrmPtr->RouteInfo.Level = 2;
        memcpy(txDataFrmPtr->Route[0], Concentrator.LongAddr, LONG_ADDR_SIZE);
        memcpy(txDataFrmPtr->Route[1], BroadcastAddrOut, LONG_ADDR_SIZE);
        txDataFrmPtr->DataLen = 0;

        taskPtr = (DATA_HANDLE_TASK *)p_arg;
        taskPtr->Command = txDataFrmPtr->Command;
        taskPtr->NodeId = NULL_U16_ID;
        taskPtr->PkgSn = txDataFrmPtr->PkgSn;

        // 创建发送数据包
        txDataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NEED_ACK, CMD_PKG, UP_DIR);
        DataHandle_SetPkgPath(txDataFrmPtr, UNREVERSED);
        DataHandle_CreateTxData(txDataFrmPtr);

        // 等待服务器的应答
        rxDataFrmPtr = OSMboxPend(taskPtr->Mbox, GPRS_WAIT_ACK_OVERTIME, &err);
        if ((void *)0 == rxDataFrmPtr) {
            RTCTimingTimer = 300;               // 如果超时则5分钟后重试
        } else {
            if (SUCCESS == Rtc_Set(*(RTC_TIME *)(rxDataFrmPtr->DataBuf), Format_Bcd)) {
                RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
            } else {
                RTCTimingTimer = 5;             // 如果校时失败则5秒后重试
            }
            OSMemPut(LargeMemoryPtr, rxDataFrmPtr);
        }
    }

    // 销毁本任务,此处必须先禁止任务调度,否则无法释放本任务占用的内存空间
    OSMboxDel(taskPtr->Mbox, OS_DEL_ALWAYS, &err);
    OSSchedLock();
    OSTaskDel(OS_PRIO_SELF);
    OSMemPut(LargeMemoryPtr, taskPtr->StkPtr);
    taskPtr->StkPtr = (void *)0;
    TaskRunStatus.RTCTiming = FALSE;
    OSSchedUnlock();
}

/************************************************************************************************
* Function Name: DataHandle_RTCTimingProc
* Decription   : 集中器实时时钟主动校时处理函数
* Input        : 无
* Output       : 无
* Others       : 每隔一段时间就启动一次校时任务
************************************************************************************************/
void DataHandle_RTCTimingProc(void)
{
    uint8 err;
    DATA_HANDLE_TASK *taskPtr;

    // 检查Gprs是否在线或者任务是否正在运行中
    if (FALSE == Gprs.Online || TRUE == TaskRunStatus.RTCTiming) {
        RTCTimingTimer = 60;
        return;
    }

    if ((void *)0 == (taskPtr = DataHandle_GetEmptyTaskPtr())) {
        return;
    }
    if ((void *)0 == (taskPtr->StkPtr = (OS_STK *)OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return;
    }
    taskPtr->Mbox = OSMboxCreate((void *)0);
    taskPtr->Msg = (void *)0;
    if (OS_ERR_NONE != OSTaskCreate(DataHandle_RTCTimingTask, taskPtr,
        taskPtr->StkPtr + MEM_LARGE_BLOCK_LEN / sizeof(OS_STK) - 1, taskPtr->Prio)) {
        OSMemPut(LargeMemoryPtr, taskPtr->StkPtr);
        taskPtr->StkPtr = (void *)0;
        OSMboxDel(taskPtr->Mbox, OS_DEL_ALWAYS, &err);
    }
}



/************************************************************************************************
* Function Name: DataHandle_GprsDelayDataUploadTask
* Decription   : Gprs 数据上传处理任务
* Input        : *p_arg-参数指针
* Output       : 无
************************************************************************************************/
void DataHandle_GprsDelayDataUploadTask(void *p_arg)
{
    uint8 err, i;
    DATA_HANDLE_TASK *taskPtr;
	PORT_BUF_FORMAT *txPortBufPtr;
	uint8 *gprsTxBufPtr;
	uint16 crc16;

    taskPtr = (DATA_HANDLE_TASK *)p_arg;
    TaskRunStatus.DataUpload = TRUE;

	// 循环查找 MemoryGprsBlock 空间，将需要上传的数据上传
	for(i = 0; i < TOTAL_GPRS_BLOCK; i++){
		// 校验是否为 GPRS 需要延时上传的数据包
		if(GPRS_DELAY_UP_CRC0 != MemoryGprsBlock[i][0] || GPRS_DELAY_UP_CRC1 != MemoryGprsBlock[i][1]
		|| GPRS_DELAY_UP_CRC2 != MemoryGprsBlock[i][2] || GPRS_DELAY_UP_CRC3 != MemoryGprsBlock[i][3]){
			continue;
		}

		if ((void *)0 == (txPortBufPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
			goto GprsErrorTask;
		}

		// 复制需要上传的数据到长内存块
		memcpy(txPortBufPtr, &MemoryGprsBlock[i][4], MEM_GPRS_BLOCK_LEN-4);
		// 重置校验头，防止重复发送数据。
		MemoryGprsBlock[i][0] = GPRS_DELAY_UP_CRCX;
		MemoryGprsBlock[i][1] = GPRS_DELAY_UP_CRCX;
		MemoryGprsBlock[i][2] = GPRS_DELAY_UP_CRCX;
		MemoryGprsBlock[i][3] = GPRS_DELAY_UP_CRCX;
		// 释放此内存块
		OSMemPut(GprsMemoryPtr, &MemoryGprsBlock[i][0]);

		// crc16 校验，校验失败后释放长内存块并继续检查下一块gprs内存
		crc16 = phyCalCRC16(&txPortBufPtr->Buffer[8], txPortBufPtr->Buffer[8]-1);
		if((txPortBufPtr->Buffer[8+txPortBufPtr->Buffer[8]-1] != (uint8)((crc16 >> 8)&0xFF))
			|| (txPortBufPtr->Buffer[8+txPortBufPtr->Buffer[8]] != (uint8)((crc16)&0xFF))){
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "++Crc16 error\n");
			continue;
		}

		if(ENABLE == GprsDebug){
			// 打印调试，生产时应去除
			if(0x19 == txPortBufPtr->Buffer[8] && 0x01 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内制造信息++\n");
			}else if(0x13 == txPortBufPtr->Buffer[8] && 0x02 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内用户信息++\n");
			}else if(0x21 == txPortBufPtr->Buffer[8] && 0x03 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内参数信息++\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x04 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内主控价量信息++\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x05 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内浮动价量信息++\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x06 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内充值信息++\n");
			}else if(0x35 == txPortBufPtr->Buffer[8] && 0x07 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内消费信息++\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x08 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内历史信息++\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x09 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内运行信息++\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0A == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内热量信息++\n");
			}else if(0x09 == txPortBufPtr->Buffer[8] && 0x0B == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内直读信息++\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0C == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内实时信息++\n");
			}else if(0x0b == txPortBufPtr->Buffer[8] && 0x0D == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内事件信息++\n");
			}else if(0x11 == txPortBufPtr->Buffer[8] && 0x0E == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++表内自动参数++\n");
			}else if(0x43 == txPortBufPtr->Buffer[8] && (0x10 == txPortBufPtr->Buffer[9] || 0x11 == txPortBufPtr->Buffer[9])){
				Gprs_OutputDebugMsg(FALSE, "\n++ZigBee抄表数据++\n");
			}else{
				Gprs_OutputDebugMsg(FALSE, "\n++其他信息++\n");
			}
			gprs_print(&txPortBufPtr->Buffer[0], txPortBufPtr->Length);
		}

		// 发送数据到gprs，如果失败则保存数据下次重发
		if (FALSE == Gprs.Online || OS_ERR_NONE != OSMboxPost(Gprs.MboxTx, txPortBufPtr)) {
			if ((void *)0 == (gprsTxBufPtr = OSMemGetOpt(GprsMemoryPtr, 10, TIME_DELAY_MS(50)))) {
				OSMemPut(LargeMemoryPtr, txPortBufPtr);
				Gprs_OutputDebugMsg(TRUE, "++GprsMemoryPtr get error\n");
				goto GprsErrorTask;
			}
			gprsTxBufPtr[0] = GPRS_DELAY_UP_CRC0;
			gprsTxBufPtr[1] = GPRS_DELAY_UP_CRC1;
			gprsTxBufPtr[2] = GPRS_DELAY_UP_CRC2;
			gprsTxBufPtr[3] = GPRS_DELAY_UP_CRC3;
			memcpy(&gprsTxBufPtr[4], txPortBufPtr, MEM_GPRS_BLOCK_LEN - 4);
			GprsDelayDataUpTimer = 10;
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "++Gprs not online\n");
			goto GprsErrorTask;
		} else {
			OSFlagPost(GlobalEventFlag, FLAG_GPRS_TX, OS_FLAG_SET, &err);
		}

		if(ENABLE == GprsDebug){
			OSTimeDlyHMSM(0, 0, 1, 0);
		}else{
			OSTimeDlyHMSM(0, 0, 0, 300);
		}
	}

GprsErrorTask:

    // 销毁本任务,此处必须先禁止任务调度,否则无法释放本任务占用的内存空间
    if ((void *)0 != txPortBufPtr) {
        //OSMemPut(LargeMemoryPtr, txPortBufPtr);
    }

    // 销毁本任务,此处必须先禁止任务调度,否则无法释放本任务占用的内存空间
    GprsDelayDataUpTimer = DATAUPLOAD_INTERVAL_TIME;
    OSMboxDel(taskPtr->Mbox, OS_DEL_ALWAYS, &err);
    OSSchedLock();
    OSTaskDel(OS_PRIO_SELF);
    OSMemPut(LargeMemoryPtr, taskPtr->StkPtr);
    taskPtr->StkPtr = (void *)0;
    TaskRunStatus.DataUpload = FALSE;
    OSSchedUnlock();
}

/************************************************************************************************
* Function Name: DataHandle_GprsDelayDataUploadProc
* Decription   : Gprs 缓存数据延时上传服务器处理函数
* Input        : 无
* Output       : 无
* Others       : 判断是否有数据需要上传并启动上传任务
************************************************************************************************/
void DataHandle_GprsDelayDataUploadProc(void)
{
    uint8 err;
    DATA_HANDLE_TASK *taskPtr;

    GprsDelayDataUpTimer = 30;

    // Gprs必须在线,并且上传任务没有运行
    if (FALSE == Gprs.Online || TRUE == TaskRunStatus.DataUpload || TRUE == TaskRunStatus.DataForward) {
        return;
    }
    // 搜索未被占用的空间,创建数据上传任务
    if ((void *)0 == (taskPtr = DataHandle_GetEmptyTaskPtr())) {
        return;
    }
    if ((void *)0 == (taskPtr->StkPtr = (OS_STK *)OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return;
    }
    taskPtr->Mbox = OSMboxCreate((void *)0);
    taskPtr->Msg = (void *)0;
    if (OS_ERR_NONE != OSTaskCreate(DataHandle_GprsDelayDataUploadTask, taskPtr,
        taskPtr->StkPtr + MEM_LARGE_BLOCK_LEN / sizeof(OS_STK) - 1, taskPtr->Prio)) {
        OSMemPut(LargeMemoryPtr, taskPtr->StkPtr);
        taskPtr->StkPtr = (void *)0;
        OSMboxDel(taskPtr->Mbox, OS_DEL_ALWAYS, &err);
    }
}


bool DataHandle_UploadGprsProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
	uint8 OneBufLen = 0;
	uint8 BufLen = 0;
	uint8 err;
	PORT_BUF_FORMAT *txPortBufPtr;
	uint8 *gprsTxBufPtr;
	uint8 MacAddr[8] = {0};
	uint16 crc16;

	//保存 MAC 地址，以便给后续的每条命令增加 MAC
	memcpy(MacAddr, &DataFrmPtr->DataBuf[1], MAC_ADDR_SIZE);
	BufLen = 1 + MAC_ADDR_SIZE;//前 8 个字节给表端 MAC 地址

	if( ENABLE == GprsDebug ){
		// 打印调试，生产时应去除
		// 打印包头信息
		gprs_print((uint8*)DataFrmPtr, 27);
		///Gprs_OutputDebugMsg(FALSE, "\ndata[0-19]: ");
		OSTimeDlyHMSM(0, 0, 0, 500);
		// 打印数据域信息
		//gprs_print(&DataFrmPtr->DataBuf[0], DataFrmPtr->PkgLength-24);
		Gprs_OutputDebugMsg(FALSE, "\ndata[0-19]: ");
		gprs_print(&DataFrmPtr->DataBuf[0], 20);
		Gprs_OutputDebugMsg(FALSE, "\n");
		OSTimeDlyHMSM(0, 0, 1, 0);
	}

	//	数据域长度最长 255 字节
	if( DataFrmPtr->DataLen > 255 ){
		return ERROR;
	}

	while( BufLen < DataFrmPtr->DataLen ){
		// 先申请一个内存用于中间数据处理。每包数据都需要重新申请一个空间。
		if ((void *)0 == (txPortBufPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
			return ERROR;
		}
		memcpy(txPortBufPtr->Buffer, MacAddr, MAC_ADDR_SIZE);	//前 8 个字节给表端 MAC 地址
		OneBufLen = DataFrmPtr->DataBuf[BufLen] + 1;			// 数据包长度
		if( OneBufLen < 5 ){
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "-- error,数据长度错误 --\n");
			return ERROR;
		}
		txPortBufPtr->Length = OneBufLen + MAC_ADDR_SIZE;		//发送给 gprs 的数据包增加 MAC 地址，增加 8 字节
		//将数据包放置于 MAC 地址后
		memcpy(&txPortBufPtr->Buffer[MAC_ADDR_SIZE], &DataFrmPtr->DataBuf[BufLen], OneBufLen);

		if(ENABLE == GprsDebug){
			// 打印调试，生产时应去除
			if(0x19 == txPortBufPtr->Buffer[8] && 0x01 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内制造信息--\n");
			}else if(0x13 == txPortBufPtr->Buffer[8] && 0x02 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内用户信息--\n");
			}else if(0x21 == txPortBufPtr->Buffer[8] && 0x03 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内参数信息--\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x04 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内主控价量信息--\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x05 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内浮动价量信息--\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x06 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内充值信息--\n");
			}else if(0x35 == txPortBufPtr->Buffer[8] && 0x07 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内消费信息--\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x08 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内历史信息--\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x09 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内运行信息--\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0A == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内热量信息--\n");
			}else if(0x09 == txPortBufPtr->Buffer[8] && 0x0B == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内直读信息--\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0C == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内实时信息--\n");
			}else if(0x0b == txPortBufPtr->Buffer[8] && 0x0D == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内事件信息--\n");
			}else if(0x11 == txPortBufPtr->Buffer[8] && 0x0E == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--表内自动参数--\n");
			}else if(0x43 == txPortBufPtr->Buffer[8] && (0x10 == txPortBufPtr->Buffer[9] || 0x11 == txPortBufPtr->Buffer[9])){
				Gprs_OutputDebugMsg(FALSE, "\n--ZigBee抄表数据--\n");
			}else{
				Gprs_OutputDebugMsg(FALSE, "\n--其他信息--\n");
			}
			gprs_print(&txPortBufPtr->Buffer[0], txPortBufPtr->Length);
		}

		// 每包数据的 CRC16 校验，如果失败，此包丢掉，继续校验下一包数据。
		crc16 = phyCalCRC16(&txPortBufPtr->Buffer[MAC_ADDR_SIZE], DataFrmPtr->DataBuf[BufLen]-1);
		if((txPortBufPtr->Buffer[MAC_ADDR_SIZE+DataFrmPtr->DataBuf[BufLen]-1] != (uint8)((crc16 >> 8)&0xFF))
			|| (txPortBufPtr->Buffer[MAC_ADDR_SIZE+DataFrmPtr->DataBuf[BufLen]] != (uint8)((crc16)&0xFF))){
			// 校验失败后需要释放无用的空间，并将指针指向下一包数据头。
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "Crc16 error\n");
			BufLen += OneBufLen;
			return ERROR;
		}

		// 数据包属性，此处为发送到 GPRS
		txPortBufPtr->Property.FilterDone = 1;
		txPortBufPtr->Property.PortNo = Uart_Gprs;

		// 如果 Gprs 未连接，应将数据包存储并延时发射。
		if (FALSE == Gprs.Online) {
			if ((void *)0 == (gprsTxBufPtr = OSMemGetOpt(GprsMemoryPtr, 10, TIME_DELAY_MS(50)))) {
				OSMemPut(LargeMemoryPtr, txPortBufPtr);
				Gprs_OutputDebugMsg(TRUE, "GprsMemoryPtr get error\n");
				return ERROR;
			}
			// 增加一个头作为校验，防止反复发送。
			gprsTxBufPtr[0] = GPRS_DELAY_UP_CRC0;
			gprsTxBufPtr[1] = GPRS_DELAY_UP_CRC1;
			gprsTxBufPtr[2] = GPRS_DELAY_UP_CRC2;
			gprsTxBufPtr[3] = GPRS_DELAY_UP_CRC3;

			// 将需要发送的数据放置到校验后
			memcpy(&gprsTxBufPtr[4], txPortBufPtr, MEM_GPRS_BLOCK_LEN - 4);

			// 延时发射时间, 单位秒(s)
			GprsDelayDataUpTimer = 10;
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "Gprs not online\n");
			BufLen += OneBufLen;
			continue;
		}else{
			// 发送 gprs 邮箱消息
			if (OS_ERR_NONE != OSMboxPost(Gprs.MboxTx, txPortBufPtr)) {
				if ((void *)0 == (gprsTxBufPtr = OSMemGetOpt(GprsMemoryPtr, 10, TIME_DELAY_MS(50)))) {
					OSMemPut(LargeMemoryPtr, txPortBufPtr);
					Gprs_OutputDebugMsg(TRUE, "GprsMemoryPtr get error\n");
					return ERROR;
				}

				gprsTxBufPtr[0] = GPRS_DELAY_UP_CRC0;
				gprsTxBufPtr[1] = GPRS_DELAY_UP_CRC1;
				gprsTxBufPtr[2] = GPRS_DELAY_UP_CRC2;
				gprsTxBufPtr[3] = GPRS_DELAY_UP_CRC3;
				memcpy(&gprsTxBufPtr[4], txPortBufPtr, MEM_GPRS_BLOCK_LEN - 4);
				GprsDelayDataUpTimer = 10;
				OSMemPut(LargeMemoryPtr, txPortBufPtr);
				Gprs_OutputDebugMsg(TRUE, "Gprs OSMboxPost error\n");

				BufLen += OneBufLen;
				continue;
			} else {
				// 发送信号让 GPRS 模块处理任务
				OSFlagPost(GlobalEventFlag, FLAG_GPRS_TX, OS_FLAG_SET, &err);
			}
		}
		if(ENABLE == GprsDebug){
			OSTimeDlyHMSM(0, 0, 1, 0);
		}else{
			OSTimeDlyHMSM(0, 0, 0, 300);
		}
		BufLen += OneBufLen;
	}

    return SUCCESS;
}


/************************************************************************************************
* Function Name: DataHandle_DataForwardProc
* Decription   : 开关无线模块的载波侦听功能。
************************************************************************************************/
bool DataHandle_Switch_Monitor (DATA_FRAME_STRUCT *DataFrmPtr_SM, uint8 enable, uint8 PreambleLen)
{
	DATA_FRAME_STRUCT *DataFrmPtr;

	// 先申请一个内存用于中间数据处理。
	if ((void *)0 == (DataFrmPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
		return ERROR;
	}
	memcpy(DataFrmPtr, DataFrmPtr_SM, MEM_LARGE_BLOCK_LEN);


    DataFrmPtr->PortNo = Usart_Rf;
    DataFrmPtr->Command = SwitchMonitor_Data;
    DataFrmPtr->DeviceType = 0x00;
    DataFrmPtr->PkgSn = PkgNo++;
    DataFrmPtr->Life_Ack.LifeCycle = 0x02;
    DataFrmPtr->Life_Ack.AckChannel = 0;//DEFAULT_RX_CHANNEL;
    DataFrmPtr->RouteInfo.Level = 2;

	uint8 LongAddr1[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	if(enable){
		memcpy(LongAddr1, &DataFrmPtr->DataBuf[3], LONG_ADDR_SIZE);
	}
	memcpy(DataFrmPtr->Route[0], Concentrator.LongAddr, LONG_ADDR_SIZE);
    memcpy(DataFrmPtr->Route[1], LongAddr1, LONG_ADDR_SIZE);

    DataFrmPtr->RouteInfo.CurPos = 0;
	memset(&DataFrmPtr->DataBuf[0], 0x02, 16);
	DataFrmPtr->DataBuf[11] = enable;

    DataFrmPtr->DataLen = 16;

    DataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NEED_ACK, CMD_PKG, DOWN_DIR);
    DataHandle_SetPkgPath(DataFrmPtr, UNREVERSED);
    AJL_DataHandle_CreateTxData(DataFrmPtr, PreambleLen);

    return NONE_ACK;

}

/************************************************************************************************
* Function Name: ReadMeter_DataForwardProc
* Decription   : 松江抄表命令
* Input        : DataFrmPtr-接收到的数据指针
* Output       : 是否需要后续处理
* Others       : 数据域: 长度[1]+命令[1]+公司代码2＋表具类别1＋子表号1
				＋用户代码4＋用水性质代码2＋返回数据参数4 + crc16校验[2]
************************************************************************************************/
bool ReadMeter_DataForwardProc(DATA_FRAME_STRUCT *dat)
{
	uint16 crc16 = 0;
	DATA_FRAME_STRUCT *DataFrmPtr;
	uint8 LongAddr1[6] = {0x01, 0x00, 0x13, 0x07, 0x17, 0x20};

    // 先申请一个内存用于中间数据处理
    if ((void *)0 == (DataFrmPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return ERROR;
    }
	memcpy(DataFrmPtr, dat, MEM_LARGE_BLOCK_LEN);

    // 节点在集中器档案中则创建转发
    DataFrmPtr->PortNo = Usart_Rf;
    DataFrmPtr->Command = Read_Songjiang_Meter_data;
    DataFrmPtr->DeviceType = 0x00;
    DataFrmPtr->PkgSn = PkgNo++;
    DataFrmPtr->Life_Ack.LifeCycle = 0x02;
    DataFrmPtr->Life_Ack.AckChannel = 0;//DEFAULT_RX_CHANNEL;
    DataFrmPtr->RouteInfo.Level = 2;

	memcpy(LongAddr1, DataFrmPtr->DataBuf, LONG_ADDR_SIZE);
	memcpy(DataFrmPtr->Route[0], Concentrator.LongAddr, LONG_ADDR_SIZE);
    memcpy(DataFrmPtr->Route[1], LongAddr1, LONG_ADDR_SIZE);

    DataFrmPtr->RouteInfo.CurPos = 0;
	DataFrmPtr->DataBuf[0] = 0x11;
	DataFrmPtr->DataBuf[1] = 0x04;
	memcpy(&DataFrmPtr->DataBuf[2], Concentrator.CustomerName, 2);
	DataFrmPtr->DataBuf[4] = 0x01;
	DataFrmPtr->DataBuf[5] = 0x00;
	DataFrmPtr->DataBuf[6] = 0x00;
	DataFrmPtr->DataBuf[7] = 0x00;
	DataFrmPtr->DataBuf[8] = 0x00;
	DataFrmPtr->DataBuf[9] = 0x00;
	DataFrmPtr->DataBuf[10] = 0x00;
	DataFrmPtr->DataBuf[11] = 0x00;
	DataFrmPtr->DataBuf[12] = 0x00;
	DataFrmPtr->DataBuf[13] = 0xff;
	DataFrmPtr->DataBuf[14] = 0xff;
	DataFrmPtr->DataBuf[15] = 0xff;

	//此处的 crc16 校验 1021 模式校验
	crc16 = phyCalCRC16(DataFrmPtr->DataBuf, DataFrmPtr->DataBuf[0]-1);
	DataFrmPtr->DataBuf[16] = (uint8)((crc16 >> 8)&0xFF);
	DataFrmPtr->DataBuf[17] = (uint8)((crc16)&0xFF);

    DataFrmPtr->DataLen = 18;

    DataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NEED_ACK, CMD_PKG, DOWN_DIR);
    DataHandle_SetPkgPath(DataFrmPtr, UNREVERSED);
    AJL_DataHandle_CreateTxData(DataFrmPtr, 0x2E);

    return NONE_ACK;

}


/************************************************************************************************
* Function Name: DataHandle_RxCmdProc
* Decription   : 数据处理任务,只处理接收到的命令事件
* Input        : DataFrmPtr-数据帧的指针
* Output       : 无
* Others       : 该函数处理来自表端或服务器或PC机或手持机发送过来的指令并根据指令来应答
************************************************************************************************/
void DataHandle_RxCmdProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    bool postHandle, reversePath;
    PKG_PROPERTY ackPkgProperty;
	uint8 err;

    postHandle = DataFrmPtr->PkgProp.NeedAck;
    reversePath = REVERSED;
    ackPkgProperty = DataHandle_SetPkgProperty(XOR_OFF, NONE_ACK, ACK_PKG, UP_DIR);
    switch (DataFrmPtr->Command) {
        // 松江抄表 0x31
        case Read_Songjiang_Meter_data:
            //松江抄表命令
            ReadMeter_DataForwardProc(DataFrmPtr);
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            break;

            // 打开无线模块的载波侦听功能 0x32
        case Open_SwitchMonitor:
            Gprs_OutputDebugMsg(FALSE, "\n ==== 打开无线模块的载波侦听功能 ====\n");
            // 打开无线模块的载波侦听功能
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            SwitchMonitor_EN = ENABLE;
            break;

            //立即关闭无线模块的载波侦听功能 0x33
        case Close_SwitchMonitor:
            Gprs_OutputDebugMsg(FALSE, "\n ==== 立即关闭无线模块的载波侦听功能 ====\n");
            DataHandle_Switch_Monitor(DataFrmPtr, DISABLE, 0x1E);
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            SwitchMonitor_EN = DISABLE;
            break;

            //打开GprsDebug功能 0x34
        case Open_GprsDebug:
            Gprs_OutputDebugMsg(FALSE, "\n ==== 打开GprsDebug功能 ====\n");
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            GprsDebug = ENABLE;
            break;

            //关闭GprsDebug功能 0x35
        case Close_GprsDebug:
            Gprs_OutputDebugMsg(FALSE, "\n ==== 关闭GprsDebug功能 ====\n");
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            GprsDebug = DISABLE;
            break;

        // 设置SIM卡号 0x36
        case Write_SIM_NUM:
            Data_SetSimNum(DataFrmPtr);
            break;

        // 获取SIM卡号 0x74
        case Read_SIM_NUM:
            memcpy(DataFrmPtr->DataBuf, Concentrator.GprsParam.SimCardId, 13);
            DataFrmPtr->DataLen = 13;
            break;

        // 读集中器信道设置方式 0x72
        case Read_CONC_Channel_Param:
            Data_RdWrConcChannelParam(DataFrmPtr);
            break;

        // 写集中器信道设置方式 0x73
        case Write_CONC_Channel_Param:
            Data_RdWrConcChannelParam(DataFrmPtr);
            break;

        // 读集中器ID 0x41
        case Read_CONC_ID:
            // 下行:空数据域
            // 上行:集中器ID的BCD码(6)
            memcpy(DataFrmPtr->DataBuf, Concentrator.LongAddr, LONG_ADDR_SIZE);
            DataFrmPtr->DataLen = LONG_ADDR_SIZE;
            break;

        // 读Gprs参数 0x45
        case Read_GPRS_Param:
            Data_GprsParameter(DataFrmPtr);
            break;

        // 写Gprs参数 0x46
        case Write_GPRS_Param:
            Data_GprsParameter(DataFrmPtr);
            break;

		// 读Gprs信号强度 0x47
		case Read_GPRS_RSSI:
			// 下行:无
			// 上行:信号强度
			DataFrmPtr->DataBuf[0] = Gprs_GetCSQ();
			DataFrmPtr->DataBuf[1] = Gprs.Online ? 0x01 : 0x00;
			DataFrmPtr->DataLen = 2;
			DataFrmPtr->DataLen += Gprs_GetIMSI(&DataFrmPtr->DataBuf[DataFrmPtr->DataLen]);
			DataFrmPtr->DataLen += Gprs_GetGMM(&DataFrmPtr->DataBuf[DataFrmPtr->DataLen]);
			break;

		// 集中器重新启动 0x4C
		case Restart_CONC_Cmd:
			// 下行:无
			// 上行:操作状态
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			DevResetTimer = 5000;
			break;

		// 读集中器版本信息 0x40
		case Read_CONC_Version:
			// 下行:空数据域
			// 上行:程序版本(2)+硬件版本(2)+协议版本(2)
			DataFrmPtr->DataLen = 0;
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)(SW_VERSION >> 8);
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)SW_VERSION;
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)(HW_VERSION >> 8);
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)HW_VERSION;
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)(PT_VERSION >> 8);
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)PT_VERSION;
			break;

		// 写集中器ID 0x42
		case Write_CONC_ID:
			Data_SetConcentratorAddr(DataFrmPtr);
			break;

		// 读集中器时钟 0x43
		case Read_CONC_RTC:
			// 下行:空数据域
			// 上行:集中器时钟(7)
			Rtc_Get((RTC_TIME *)DataFrmPtr->DataBuf, Format_Bcd);
			DataFrmPtr->DataLen = 7;
			break;

		// 写集中器时钟 0x44
		case Write_CONC_RTC:
			// 下行:集中器时钟(7)
			// 上行:操作状态(1)
			if (SUCCESS == Rtc_Set(*(RTC_TIME *)(DataFrmPtr->DataBuf), Format_Bcd)) {
				DataFrmPtr->DataBuf[0] = OP_Succeed;
				GprsDelayDataUpTimer = 10;
				RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
			} else {
				DataFrmPtr->DataBuf[0] = OP_TimeAbnormal;
			}
			DataFrmPtr->DataLen = 1;
			break;

			//打开Gprs无线升级功能 0x37
		case Open_GprsSoftUp:
			Gprs_OutputDebugMsg(FALSE, "\n ==== 打开Gprs无线升级功能 ====\n");
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			GprsSoftUp = GPRS_SOFT_UP_EN;
			OSFlagPost(GlobalEventFlag, FLAG_LOGOFF_EVENT, OS_FLAG_SET, &err);
			break;

			//关闭Gprs无线升级功能 0x38
		case Close_GprsSoftUp:
			Gprs_OutputDebugMsg(FALSE, "\n ==== 关闭Gprs无线升级功能 ====\n");
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			GprsSoftUp = 0x0;
			OSFlagPost(GlobalEventFlag, FLAG_LOGOFF_EVENT, OS_FLAG_SET, &err);
			break;

		// 集中器程序升级 0xF1
		case Software_Update_Cmd:
			Data_SwUpdate(DataFrmPtr);
			break;

		// 其他指令不支持
		default:
			postHandle = NONE_ACK;
			OSMemPut(LargeMemoryPtr, DataFrmPtr);
			break;
    }

    if (NEED_ACK == postHandle) {
        DataFrmPtr->PkgProp = ackPkgProperty;
        DataHandle_SetPkgPath(DataFrmPtr, reversePath);
        DataHandle_CreateTxData(DataFrmPtr);
    }
}

/************************************************************************************************
* Function Name: DataHandle_RxAckProc
* Decription   : 数据处理任务,只处理接收到的应答事件
* Input        : DataBufPtr-命令数据指针
* Output       : 无
* Others       : 该函数处理来自表端或服务器或PC机或手持机发送过来的应答
************************************************************************************************/
void DataHandle_RxAckProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 i;
    uint16 nodeId;
    DATA_HANDLE_TASK *taskPtr;

    // 查找应答节点是否在本档案中
    nodeId = Data_FindNodeId(0, DataFrmPtr->Route[0]);

    // 判断该应答帧应该传递给谁
    for (i = 0; i < MAX_DATA_HANDLE_TASK_NUM; i++) {
        taskPtr = &DataHandle_TaskArry[i];
        if ((void *)0 != taskPtr->StkPtr &&
            taskPtr->NodeId == nodeId &&
            taskPtr->Command == DataFrmPtr->Command &&
            taskPtr->PkgSn == DataFrmPtr->PkgSn) {
            if (OS_ERR_NONE != OSMboxPost(taskPtr->Mbox, DataFrmPtr)) {
                OSMemPut(LargeMemoryPtr, DataFrmPtr);
            }
            return;
        }
    }
    OSMemPut(LargeMemoryPtr, DataFrmPtr);
}

/************************************************************************************************
* Function Name: DataHandle_PassProc
* Decription   : 透传处理任务
* Input        : DataFrmPtr-接收到的数据指针
* Output       : TRUE-已经处理,FALSE-没有处理
* Others       : 目标地址不是自己时,传递到下一个节点
************************************************************************************************/
bool DataHandle_PassProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    // 如果是目标节点则跳至下一步处理
    if (DataFrmPtr->RouteInfo.CurPos == DataFrmPtr->RouteInfo.Level - 1) {
        return FALSE;
    }
    // 如果是上行且是倒数第二级则按照设备类型选择通讯端口,其他情况都用RF端口
    if (UP_DIR == DataFrmPtr->PkgProp.Direction &&
        DataFrmPtr->RouteInfo.CurPos == DataFrmPtr->RouteInfo.Level - 2) {
        switch (DataFrmPtr->DeviceType) {
            case Dev_USB:
                DataFrmPtr->PortNo = Usb_Port;
                break;
            case Dev_Server:
                DataFrmPtr->PortNo = Uart_Gprs;
                break;
            case Dev_SerialPort:
                DataFrmPtr->PortNo = Usart_Debug;
                break;
            default:
                DataFrmPtr->PortNo = Usart_Rf;
                break;
        }
    } else {
        DataFrmPtr->PortNo = Usart_Rf;
    }
    DataHandle_CreateTxData(DataFrmPtr);
    return TRUE;
}


void DebugOutputLength(uint8 *StrPtr, uint8 SrcLength)
{
    uint16 len;
    uint8 *bufPtr;

    if ((void *)0 == (bufPtr = OSMemGetOpt(LargeMemoryPtr, 20, TIME_DELAY_MS(50)))) {
        return;
    }
    len = BcdToAscii( StrPtr, (uint8 *)bufPtr, SrcLength, 3);
    DataHandle_OutputMonitorMsg(Gprs_Connect_Msg, bufPtr, len);
    OSMemPut(LargeMemoryPtr, bufPtr);
    return;
}

uint8 DataHandle_IncreaseSaveMeterDataProc(uint8 *dat)
{
    DATA_FRAME_STRUCT *DataFrmPtr = (DATA_FRAME_STRUCT *)dat;
    uint8 ret = 0;
    bool status;
    uint8 PreambleLen;

    if(( 0x23 == DataFrmPtr->Command )
            || (0x11 == DataFrmPtr->Command && ACK_PKG == DataFrmPtr->PkgProp.PkgType)
            || (0x14 == DataFrmPtr->Command && CMD_PKG == DataFrmPtr->PkgProp.PkgType)){

        static uint8 TotalPackNum = 0, PackNum = 0, BeforePackNum = 0;

        TotalPackNum = DataFrmPtr->DataBuf[0] & 0x0F;		//总包长
        PackNum = (DataFrmPtr->DataBuf[0]>> 4) & 0x0F ;		//当前包长
        if( BeforePackNum+1 != PackNum || TotalPackNum > 9){
            TotalPackNum = 0;
            PackNum = 0;
            BeforePackNum = 0;
            return 0;
        }else{
            BeforePackNum+=1;
        }

        //分解数据并直接上传到 GPRS
        status = DataHandle_UploadGprsProc(DataFrmPtr);
        if(SUCCESS == status && PackNum == TotalPackNum){
            TotalPackNum = 0;
            PackNum = 0;
            BeforePackNum = 0;
            if( ENABLE == SwitchMonitor_EN ){
                PreambleLen = 0x01;
                if(ENABLE == GprsDebug){
                    Gprs_OutputDebugMsg(FALSE, "\n--SwitchMonitor_enable--\n");
                }
                DataHandle_Switch_Monitor(DataFrmPtr, SwitchMonitor_EN, PreambleLen);
                PreambleLen = 0x1E;
                //if(1){
                if(ENABLE == GprsDebug){
                    // 打印调试，生产时应去除
                    Gprs_OutputDebugMsg(TRUE, "\n--RTC--\n");
                }
                DataHandle_SetRtcTiming(DataFrmPtr, PreambleLen);//发送校时命令到表端
            }else{
                PreambleLen = 0x01;
                DataHandle_SetRtcTiming(DataFrmPtr, PreambleLen);//发送校时命令到表端
                //if(1){
                if(ENABLE == GprsDebug){
                    DebugOutputLength(&DataFrmPtr->DataBuf[3], LONG_ADDR_SIZE);
                    //Gprs_OutputDebugMsg(TRUE, "\n++RTC++\n");
                }
            }
        }
        ret = 1;
    }
    return ret;
}

/************************************************************************************************
* Function Name: DataHandle_Task
* Decription   : 数据处理任务,只处理接收事件
* Input        : *p_arg-参数指针
* Output       : 无
* Others       : 无
************************************************************************************************/
void DataHandle_Task(void *p_arg)
{
    uint8 i, err, *dat;
    OS_FLAGS eventFlag;
    DATA_FRAME_STRUCT *dataFrmPtr;
    EXTRACT_DATA_RESULT ret;

    // 初始化参数
    (void)p_arg;
    PkgNo = CalCrc8(Concentrator.LongAddr, LONG_ADDR_SIZE);
    for (i = 0; i < MAX_DATA_HANDLE_TASK_NUM; i++) {
        DataHandle_TaskArry[i].Prio = TASK_DATAHANDLE_DATA_PRIO + i;
        DataHandle_TaskArry[i].StkPtr = (void *)0;
    }
    TaskRunStatus.DataForward = FALSE;
    TaskRunStatus.DataReplenish = FALSE;
    TaskRunStatus.DataUpload = FALSE;
    TaskRunStatus.RTCService = FALSE;
    TaskRunStatus.RTCTiming = FALSE;

    // 数据初始化
    Data_Init();
    DataHandle_SetRtcTiming((void *)0, 0x1);
    OSTimeDlyHMSM(0, 0, 0, 100);

    while (TRUE) {
        // 获取发生的事件数据
        eventFlag = OSFlagPend(GlobalEventFlag, (OS_FLAGS)DATAHANDLE_EVENT_FILTER, (OS_FLAG_WAIT_SET_ANY | OS_FLAG_CONSUME), TIME_DELAY_MS(5000), &err);

        // 处理这些数据
        while (eventFlag != (OS_FLAGS)0) {
            dat = (void *)0;
            if (eventFlag & FLAG_USART_RF_RX) {
                // Rf模块收到了数据
                dat = OSMboxAccept(SerialPort.Port[Usart_Rf].MboxRx);
                eventFlag &= ~FLAG_USART_RF_RX;
            } else if (eventFlag & FLAG_GPRS_RX) {
                // Gprs模块收到了数据
                dat = OSMboxAccept(Gprs.MboxRx);
                eventFlag &= ~FLAG_GPRS_RX;
                if( (0x0 == GprsSoftUp) && ((void *)0 != dat) ){
                    DataHandle_GprsExtractData(dat);
                }
            } else if (eventFlag & FLAG_USB_RX) {
                // Usb端口收到了数据
                dat = OSMboxAccept(SerialPort.Port[Usb_Port].MboxRx);
                eventFlag &= ~FLAG_USB_RX;
            } else if (eventFlag & FLAG_USART_DEBUG_RX) {
                // Debug端口收到了数据
                dat = OSMboxAccept(SerialPort.Port[Usart_Debug].MboxRx);
                eventFlag &= ~FLAG_USART_DEBUG_RX;
            } else if (eventFlag & FLAG_UART_RS485_RX) {
                // 485端口收到了数据
                dat = OSMboxAccept(SerialPort.Port[Uart_Rs485].MboxRx);
                eventFlag &= ~FLAG_UART_RS485_RX;
            } else if (eventFlag & FLAG_USART_IR_RX) {
                // Ir端口收到了数据
                dat = OSMboxAccept(SerialPort.Port[Usart_Ir].MboxRx);
                eventFlag &= ~FLAG_USART_IR_RX;
            } else if (eventFlag & FLAG_DELAY_SAVE_TIMER) {
                // 数据延时保存
                eventFlag &= ~FLAG_DELAY_SAVE_TIMER;
                DataHandle_DataDelaySaveProc();
            } else if (eventFlag & FLAG_DATA_REPLENISH_TIMER) {
                // 数据补抄处理
                eventFlag &= ~FLAG_DATA_REPLENISH_TIMER;
                //DataHandle_DataReplenishProc();
            } else if (eventFlag & FLAG_DATA_UPLOAD_TIMER) {
                // 数据上传处理
                eventFlag &= ~FLAG_DATA_UPLOAD_TIMER;
                //DataHandle_DataUploadProc();
            } else if (eventFlag & FLAG_RTC_TIMING_TIMER) {
                // 时钟主动校时处理
                eventFlag &= ~FLAG_RTC_TIMING_TIMER;

                if( 1 == GetRtcOK ){
                    RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
                    continue;
                }
                // 检查Gprs是否在线或者任务是否正在运行中
                if (FALSE == Gprs.Online) {
                    RTCTimingTimer = 30;
                    continue;
                }
                RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
                OSFlagPost(GlobalEventFlag, FLAG_GPRS_UPDATA_RTC, OS_FLAG_SET, &err);
            } else if (eventFlag & FLAG_GPRS_DELAY_UPLOAD) {
                // GPRS 缓存数据延时上传处理
                eventFlag &= ~FLAG_GPRS_DELAY_UPLOAD;
                DataHandle_GprsDelayDataUploadProc();
            }

            if ((void *)0 == dat) {
                continue;
            }

            // 从原数据中提取数据
            if (Ok_Data != (ret = DataHandle_ExtractData(dat))) {
                if (Error_DstAddress == ret) {
                    // 如果不是给自己的数据可以监听其他节点并将其加入对应的邻居表中
                    DataHandle_IncreaseSaveMeterDataProc(dat);
                }
                OSMemPut(LargeMemoryPtr, dat);
                continue;
            }
            dataFrmPtr = (DATA_FRAME_STRUCT *)dat;

            // 收到正确的表具数据后，如果集中器中没有档案则直接保存档案信息和表信息到集中器中
            if(DataHandle_IncreaseSaveMeterDataProc(dat)){
                OSMemPut(LargeMemoryPtr, dat);
                continue;
            }

            // 确定监控信息上传的通道
            if (Usart_Debug == dataFrmPtr->PortNo || Usb_Port == dataFrmPtr->PortNo) {
                MonitorPort = (PORT_NO)(dataFrmPtr->PortNo);
            }

            // 如果目标地址不是自己则转发
            if (TRUE == DataHandle_PassProc(dataFrmPtr)) {
                continue;
            }

            // 分别处理命令帧和应答帧指令
            if (CMD_PKG == dataFrmPtr->PkgProp.PkgType) {
                // 如果是命令帧
                DataHandle_RxCmdProc(dataFrmPtr);
            } else {
                // 如果是应答帧
                DataHandle_RxAckProc(dataFrmPtr);
            }
        }

        OSTimeDlyHMSM(0, 0, 0, 50);
    }
}

/***************************************End of file*********************************************/


