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
PORT_NO MonitorPort = Usart_Debug;                      // ��ض˿�
uint16 RTCTimingTimer = 60;                             // RTCУʱ����������ʱ��
uint16 GprsDelayDataUpTimer = 60;						// Gprs ����������ʱ�ϴ���ʱ��
TASK_STATUS_STRUCT TaskRunStatus;                       // ��������״̬
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
* Decription   : ����������������յ�����ָ��
* Input        : ��
* Output       : �����ָ��
* Others       : ��
************************************************************************************************/
DATA_HANDLE_TASK *DataHandle_GetEmptyTaskPtr(void)
{
    uint8 i;

    // ����δ��ռ�õĿռ�,���������ϴ�����
    for (i = 0; i < MAX_DATA_HANDLE_TASK_NUM; i++) {
        if ((void *)0 == DataHandle_TaskArry[i].StkPtr) {
            return (&DataHandle_TaskArry[i]);
        }
    }

    // �������ȫ��ʹ�÷��ؿն���
    return ((void *)0);
}

/************************************************************************************************
* Function Name: DataHandle_SetPkgProperty
* Decription   : ���ð�����ֵ
* Input        : PkgXor-��������Ӫ�̱��벻����־: 0-����� 1-���
*                NeedAck-�Ƿ���Ҫ��ִ 0-�����ִ 1-��Ҫ��ִ
*                PkgType-֡���� 0-����֡ 1-Ӧ��֡
*                Dir-�����б�ʶ 0-���� 1-����
* Output       : ����ֵ
* Others       : ��
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
* Decription   : �������ݰ���·��
* Input        : DataFrmPtr-����ָ��
*                ReversePath-�Ƿ���Ҫ��ת·��
* Output       : ��
* Others       : ��
************************************************************************************************/
void DataHandle_SetPkgPath(DATA_FRAME_STRUCT *DataFrmPtr, bool ReversePath)
{
    uint8 i, tmpBuf[LONG_ADDR_SIZE];

    if (0 == memcmp(BroadcastAddrIn, DataFrmPtr->Route[DataFrmPtr->RouteInfo.CurPos], LONG_ADDR_SIZE)) {
        memcpy(DataFrmPtr->Route[DataFrmPtr->RouteInfo.CurPos], Concentrator.LongAddr, LONG_ADDR_SIZE);
    }
    // ·���Ƿ�ת����
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
* Decription   : �����������ݰ�,������ר��
* Input        : DataFrmPtr-�����͵�����
* Output       : �ɹ������
* Others       : �ú���ִ����Ϻ���ͷ�DataBufPtrָ��Ĵ洢��,���Ὣ·�����ĵ�ַ���з�ת
************************************************************************************************/
ErrorStatus AJL_DataHandle_CreateTxData(DATA_FRAME_STRUCT *DataFrmPtr, uint8 PreambleLen)
{
    uint8 err;
    uint16 tmp;
    PORT_BUF_FORMAT *txPortBufPtr;

    // ������һ���ڴ������м����ݴ���
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
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgProp.Content;         // ���ı�ʶ
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgSn;                   // �����
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Command;                 // ������
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->DeviceType;          // �豸����
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Life_Ack.Content;        // �������ں�Ӧ���ŵ�
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->RouteInfo.Content;       // ·����Ϣ
    memcpy(&txPortBufPtr->Buffer[txPortBufPtr->Length], DataFrmPtr->Route[0], DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE);
    txPortBufPtr->Length += DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE;
    memcpy(txPortBufPtr->Buffer + txPortBufPtr->Length, DataFrmPtr->DataBuf, DataFrmPtr->DataLen);      // ������
    txPortBufPtr->Length += DataFrmPtr->DataLen;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // �����ź�ǿ��
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // �����ź�ǿ��
    txPortBufPtr->Buffer[txPortBufPtr->Length] = CalCrc8((uint8 *)(&txPortBufPtr->Buffer[tmp]), txPortBufPtr->Length - tmp);     // Crc8У��
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

    // ����ת��
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

    DataFrmPtr->DataBuf[0] = 0x09;// �����
    DataFrmPtr->DataBuf[1] = 0x01;// Уʱ����
    DataFrmPtr->DataBuf[2] = BcdToBin((uint8)((rtcTimer.Year>>8)&0xFF));// �� ��ʵ����� - 2000��� Hex ֵ
    DataFrmPtr->DataBuf[3] = BcdToBin(rtcTimer.Month);// ��
    DataFrmPtr->DataBuf[4] = BcdToBin(rtcTimer.Day);// ��
    DataFrmPtr->DataBuf[5] = BcdToBin(rtcTimer.Hour);// ʱ
    DataFrmPtr->DataBuf[6] = BcdToBin(rtcTimer.Minute);// ��
    DataFrmPtr->DataBuf[7] = BcdToBin(rtcTimer.Second);// ��
    crc16 = phyCalCRC16(DataFrmPtr->DataBuf, DataFrmPtr->DataBuf[0] - 1);
    DataFrmPtr->DataBuf[8] = (uint8)((crc16 >> 8)&0xFF);// crc16У���� [2]
    DataFrmPtr->DataBuf[9] = (uint8)((crc16)&0xFF);
    DataFrmPtr->DataLen = 10;

    DataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NONE_ACK, CMD_PKG, DOWN_DIR);
    DataHandle_SetPkgPath(DataFrmPtr, UNREVERSED);
    AJL_DataHandle_CreateTxData(DataFrmPtr, PreambleLen);

    return NONE_ACK;

}

/************************************************************************************************
* Function Name: DataHandle_GprsExtractData
* Decription   : ��Э����ȡ�����ݲ��������ݵ���ȷ��
* Input        : BufPtr-ԭ����ָ��
* Output       : �ɹ������˵��
* Others       : ע��-�ɹ����ô˺�����BufPtrָ����ȡ���ݺ���ڴ�
************************************************************************************************/
bool DataHandle_GprsExtractData(uint8 *BufPtr)
{
	uint8 len, err, ret;
	GPRS_PORT_BUF_FORMAT *gprsPortBufPtr;
	uint16 crc16;

	// ��Э���ʽ��ȡ��Ӧ������
	gprsPortBufPtr = (GPRS_PORT_BUF_FORMAT *)BufPtr;
	len = gprsPortBufPtr->Buf[0];
	crc16 = phyCalCRC16(&gprsPortBufPtr->Buf[0], len-1);
	if((gprsPortBufPtr->Buf[len-1] != (uint8)((crc16 >> 8)&0xFF)) || (gprsPortBufPtr->Buf[len] != (uint8)((crc16)&0xFF))){
		//crc16 ����
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
				//Gprs_OutputDebugMsg(TRUE, "\n ---- RTC Уʱ�ɹ� ----\n");

				memcpy( Concentrator.CustomerName, &gprsPortBufPtr->Buf[2], 2);
				//��˺ͼ��������ù�˾�����һ���ֽڵĸ�4Ϊ��Ϊ�ŵ��š�
				if( 0x1 == Concentrator.EnableChannelSet ){
					Concentrator.TxRxChannel = (Concentrator.CustomerName[0]>>4) & 0xF;
					//����Уʱ������,�����ڸı� 2e28 ģ���շ��ŵ���
					DataHandle_SetRtcTiming( (void *)0, 0x1 );
					OSTimeDlyHMSM(0, 0, 0, 200);
					// ��ʱ�󱣴汣�漯����������Ϣ
					OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_DELAY_SAVE_TIMER, OS_FLAG_SET, &err);
				}
			} else {
				SetRtcNum++;
				if(SetRtcNum == 3){
					GetRtcOK = 0;
				}
				Gprs_OutputDebugMsg(TRUE, "\n ---- RTC ���µ�½������Уʱ ----\n");
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
* Decription   : ��Э����ȡ�����ݲ��������ݵ���ȷ��
* Input        : BufPtr-ԭ����ָ��
* Output       : �ɹ������˵��
* Others       : ע��-�ɹ����ô˺�����BufPtrָ����ȡ���ݺ���ڴ�
************************************************************************************************/
EXTRACT_DATA_RESULT DataHandle_ExtractData(uint8 *BufPtr)
{
    uint8 i, *msg;
    uint16 tmp;
    PORT_BUF_FORMAT *portBufPtr;
    DATA_FRAME_STRUCT *dataFrmPtr;

    // ��Э���ʽ��ȡ��Ӧ������
    portBufPtr = (PORT_BUF_FORMAT *)BufPtr;
    if (FALSE == portBufPtr->Property.FilterDone) {
        return Error_Data;
    }
    // ����һ���ڴ����ڴ����ȡ�������
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

    // ���Crc8�Ƿ���ȷ
    if (dataFrmPtr->Crc8 != CalCrc8(portBufPtr->Buffer, dataFrmPtr->PkgLength - 2) || portBufPtr->Length < dataFrmPtr->PkgLength) {
        OSMemPut(LargeMemoryPtr, msg);
        return Error_DataCrcCheck;
    }
    // ���������Ƿ��� 0x16
    if ( 0x16 != *(portBufPtr->Buffer + dataFrmPtr->PkgLength - 1)) {
        OSMemPut(LargeMemoryPtr, msg);
        return Error_Data;
    }

    // ����Ƿ�Ϊ�㲥��ַ�򱾻���ַ
    dataFrmPtr->RouteInfo.CurPos += 1;
    if ((0 == memcmp(Concentrator.LongAddr, dataFrmPtr->Route[dataFrmPtr->RouteInfo.CurPos], LONG_ADDR_SIZE) ||
        0 == memcmp(BroadcastAddrIn, dataFrmPtr->Route[dataFrmPtr->RouteInfo.CurPos], LONG_ADDR_SIZE)) &&
        dataFrmPtr->RouteInfo.CurPos < dataFrmPtr->RouteInfo.Level) {
        memcpy(BufPtr, msg, MEM_LARGE_BLOCK_LEN);
        OSMemPut(LargeMemoryPtr, msg);
        return Ok_Data;
    }

    // Ҫ���к�������,���Դ˴�����ȡ�������ݷ���
    memcpy(BufPtr, msg, MEM_LARGE_BLOCK_LEN);
    OSMemPut(LargeMemoryPtr, msg);
    return Error_DstAddress;
}


/************************************************************************************************
* Function Name: DataHandle_CreateTxData
* Decription   : �����������ݰ�
* Input        : DataFrmPtr-�����͵�����
* Output       : �ɹ������
* Others       : �ú���ִ����Ϻ���ͷ�DataBufPtrָ��Ĵ洢��,���Ὣ·�����ĵ�ַ���з�ת
************************************************************************************************/
ErrorStatus DataHandle_CreateTxData(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 err;
    uint16 tmp, nodeId;
    PORT_BUF_FORMAT *txPortBufPtr;

    // ������һ���ڴ������м����ݴ���
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
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgProp.Content;         // ���ı�ʶ
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->PkgSn;                   // �����
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Command;                 // ������
    // Ϊ�˺��ѳ����ĵ�һ�������
    if (0 == DataFrmPtr->Life_Ack.AckChannel) {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = Dev_Server;                      // �豸����
    } else {
        txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->DeviceType;          // �豸����
    }
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->Life_Ack.Content;        // �������ں�Ӧ���ŵ�
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = DataFrmPtr->RouteInfo.Content;       // ·����Ϣ
    memcpy(&txPortBufPtr->Buffer[txPortBufPtr->Length], DataFrmPtr->Route[0], DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE);
    txPortBufPtr->Length += DataFrmPtr->RouteInfo.Level * LONG_ADDR_SIZE;
    memcpy(txPortBufPtr->Buffer + txPortBufPtr->Length, DataFrmPtr->DataBuf, DataFrmPtr->DataLen);      // ������
    txPortBufPtr->Length += DataFrmPtr->DataLen;
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // �����ź�ǿ��
    txPortBufPtr->Buffer[txPortBufPtr->Length++] = 0x55;                                // �����ź�ǿ��
    txPortBufPtr->Buffer[txPortBufPtr->Length] = CalCrc8((uint8 *)(&txPortBufPtr->Buffer[tmp]), txPortBufPtr->Length - tmp);     // Crc8У��
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
        // Ϊ�˺��ѳ����ĵ�һ�������
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
* Decription   : ������ʱ���洦����
* Input        : ��
* Output       : ��
* Others       : ������������Ҫ����ʱ����һ����ʱ����,���ӳ�Flash������
************************************************************************************************/
void DataHandle_DataDelaySaveProc(void)
{
    Flash_SaveConcentratorInfo();
}

/************************************************************************************************
* Function Name: DataHandle_OutputMonitorMsg
* Decription   : ������������������Ϣ
* Input        : MsgType-��Ϣ������,MsgPtr-�����Ϣָ��,MsgLen-��Ϣ�ĳ���
* Output       : ��
* Others       : ��
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
* Decription   : ʵʱʱ��Уʱ��������
* Input        : *p_arg-����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void DataHandle_RTCTimingTask(void *p_arg)
{
    uint8 err;
    DATA_HANDLE_TASK *taskPtr;
    DATA_FRAME_STRUCT *txDataFrmPtr, *rxDataFrmPtr;

    // ��������Уʱ���ݰ�
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

        // �����������ݰ�
        txDataFrmPtr->PkgProp = DataHandle_SetPkgProperty(XOR_OFF, NEED_ACK, CMD_PKG, UP_DIR);
        DataHandle_SetPkgPath(txDataFrmPtr, UNREVERSED);
        DataHandle_CreateTxData(txDataFrmPtr);

        // �ȴ���������Ӧ��
        rxDataFrmPtr = OSMboxPend(taskPtr->Mbox, GPRS_WAIT_ACK_OVERTIME, &err);
        if ((void *)0 == rxDataFrmPtr) {
            RTCTimingTimer = 300;               // �����ʱ��5���Ӻ�����
        } else {
            if (SUCCESS == Rtc_Set(*(RTC_TIME *)(rxDataFrmPtr->DataBuf), Format_Bcd)) {
                RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
            } else {
                RTCTimingTimer = 5;             // ���Уʱʧ����5�������
            }
            OSMemPut(LargeMemoryPtr, rxDataFrmPtr);
        }
    }

    // ���ٱ�����,�˴������Ƚ�ֹ�������,�����޷��ͷű�����ռ�õ��ڴ�ռ�
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
* Decription   : ������ʵʱʱ������Уʱ������
* Input        : ��
* Output       : ��
* Others       : ÿ��һ��ʱ�������һ��Уʱ����
************************************************************************************************/
void DataHandle_RTCTimingProc(void)
{
    uint8 err;
    DATA_HANDLE_TASK *taskPtr;

    // ���Gprs�Ƿ����߻��������Ƿ�����������
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
* Decription   : Gprs �����ϴ���������
* Input        : *p_arg-����ָ��
* Output       : ��
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

	// ѭ������ MemoryGprsBlock �ռ䣬����Ҫ�ϴ��������ϴ�
	for(i = 0; i < TOTAL_GPRS_BLOCK; i++){
		// У���Ƿ�Ϊ GPRS ��Ҫ��ʱ�ϴ������ݰ�
		if(GPRS_DELAY_UP_CRC0 != MemoryGprsBlock[i][0] || GPRS_DELAY_UP_CRC1 != MemoryGprsBlock[i][1]
		|| GPRS_DELAY_UP_CRC2 != MemoryGprsBlock[i][2] || GPRS_DELAY_UP_CRC3 != MemoryGprsBlock[i][3]){
			continue;
		}

		if ((void *)0 == (txPortBufPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
			goto GprsErrorTask;
		}

		// ������Ҫ�ϴ������ݵ����ڴ��
		memcpy(txPortBufPtr, &MemoryGprsBlock[i][4], MEM_GPRS_BLOCK_LEN-4);
		// ����У��ͷ����ֹ�ظ��������ݡ�
		MemoryGprsBlock[i][0] = GPRS_DELAY_UP_CRCX;
		MemoryGprsBlock[i][1] = GPRS_DELAY_UP_CRCX;
		MemoryGprsBlock[i][2] = GPRS_DELAY_UP_CRCX;
		MemoryGprsBlock[i][3] = GPRS_DELAY_UP_CRCX;
		// �ͷŴ��ڴ��
		OSMemPut(GprsMemoryPtr, &MemoryGprsBlock[i][0]);

		// crc16 У�飬У��ʧ�ܺ��ͷų��ڴ�鲢���������һ��gprs�ڴ�
		crc16 = phyCalCRC16(&txPortBufPtr->Buffer[8], txPortBufPtr->Buffer[8]-1);
		if((txPortBufPtr->Buffer[8+txPortBufPtr->Buffer[8]-1] != (uint8)((crc16 >> 8)&0xFF))
			|| (txPortBufPtr->Buffer[8+txPortBufPtr->Buffer[8]] != (uint8)((crc16)&0xFF))){
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "++Crc16 error\n");
			continue;
		}

		if(ENABLE == GprsDebug){
			// ��ӡ���ԣ�����ʱӦȥ��
			if(0x19 == txPortBufPtr->Buffer[8] && 0x01 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++����������Ϣ++\n");
			}else if(0x13 == txPortBufPtr->Buffer[8] && 0x02 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++�����û���Ϣ++\n");
			}else if(0x21 == txPortBufPtr->Buffer[8] && 0x03 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++���ڲ�����Ϣ++\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x04 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++�������ؼ�����Ϣ++\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x05 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++���ڸ���������Ϣ++\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x06 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++���ڳ�ֵ��Ϣ++\n");
			}else if(0x35 == txPortBufPtr->Buffer[8] && 0x07 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++����������Ϣ++\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x08 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++������ʷ��Ϣ++\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x09 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++����������Ϣ++\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0A == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++����������Ϣ++\n");
			}else if(0x09 == txPortBufPtr->Buffer[8] && 0x0B == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++����ֱ����Ϣ++\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0C == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++����ʵʱ��Ϣ++\n");
			}else if(0x0b == txPortBufPtr->Buffer[8] && 0x0D == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++�����¼���Ϣ++\n");
			}else if(0x11 == txPortBufPtr->Buffer[8] && 0x0E == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n++�����Զ�����++\n");
			}else if(0x43 == txPortBufPtr->Buffer[8] && (0x10 == txPortBufPtr->Buffer[9] || 0x11 == txPortBufPtr->Buffer[9])){
				Gprs_OutputDebugMsg(FALSE, "\n++ZigBee��������++\n");
			}else{
				Gprs_OutputDebugMsg(FALSE, "\n++������Ϣ++\n");
			}
			gprs_print(&txPortBufPtr->Buffer[0], txPortBufPtr->Length);
		}

		// �������ݵ�gprs�����ʧ���򱣴������´��ط�
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

    // ���ٱ�����,�˴������Ƚ�ֹ�������,�����޷��ͷű�����ռ�õ��ڴ�ռ�
    if ((void *)0 != txPortBufPtr) {
        //OSMemPut(LargeMemoryPtr, txPortBufPtr);
    }

    // ���ٱ�����,�˴������Ƚ�ֹ�������,�����޷��ͷű�����ռ�õ��ڴ�ռ�
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
* Decription   : Gprs ����������ʱ�ϴ�������������
* Input        : ��
* Output       : ��
* Others       : �ж��Ƿ���������Ҫ�ϴ��������ϴ�����
************************************************************************************************/
void DataHandle_GprsDelayDataUploadProc(void)
{
    uint8 err;
    DATA_HANDLE_TASK *taskPtr;

    GprsDelayDataUpTimer = 30;

    // Gprs��������,�����ϴ�����û������
    if (FALSE == Gprs.Online || TRUE == TaskRunStatus.DataUpload || TRUE == TaskRunStatus.DataForward) {
        return;
    }
    // ����δ��ռ�õĿռ�,���������ϴ�����
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

	//���� MAC ��ַ���Ա��������ÿ���������� MAC
	memcpy(MacAddr, &DataFrmPtr->DataBuf[1], MAC_ADDR_SIZE);
	BufLen = 1 + MAC_ADDR_SIZE;//ǰ 8 ���ֽڸ���� MAC ��ַ

	if( ENABLE == GprsDebug ){
		// ��ӡ���ԣ�����ʱӦȥ��
		// ��ӡ��ͷ��Ϣ
		gprs_print((uint8*)DataFrmPtr, 27);
		///Gprs_OutputDebugMsg(FALSE, "\ndata[0-19]: ");
		OSTimeDlyHMSM(0, 0, 0, 500);
		// ��ӡ��������Ϣ
		//gprs_print(&DataFrmPtr->DataBuf[0], DataFrmPtr->PkgLength-24);
		Gprs_OutputDebugMsg(FALSE, "\ndata[0-19]: ");
		gprs_print(&DataFrmPtr->DataBuf[0], 20);
		Gprs_OutputDebugMsg(FALSE, "\n");
		OSTimeDlyHMSM(0, 0, 1, 0);
	}

	//	�����򳤶�� 255 �ֽ�
	if( DataFrmPtr->DataLen > 255 ){
		return ERROR;
	}

	while( BufLen < DataFrmPtr->DataLen ){
		// ������һ���ڴ������м����ݴ���ÿ�����ݶ���Ҫ��������һ���ռ䡣
		if ((void *)0 == (txPortBufPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
			return ERROR;
		}
		memcpy(txPortBufPtr->Buffer, MacAddr, MAC_ADDR_SIZE);	//ǰ 8 ���ֽڸ���� MAC ��ַ
		OneBufLen = DataFrmPtr->DataBuf[BufLen] + 1;			// ���ݰ�����
		if( OneBufLen < 5 ){
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "-- error,���ݳ��ȴ��� --\n");
			return ERROR;
		}
		txPortBufPtr->Length = OneBufLen + MAC_ADDR_SIZE;		//���͸� gprs �����ݰ����� MAC ��ַ������ 8 �ֽ�
		//�����ݰ������� MAC ��ַ��
		memcpy(&txPortBufPtr->Buffer[MAC_ADDR_SIZE], &DataFrmPtr->DataBuf[BufLen], OneBufLen);

		if(ENABLE == GprsDebug){
			// ��ӡ���ԣ�����ʱӦȥ��
			if(0x19 == txPortBufPtr->Buffer[8] && 0x01 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--����������Ϣ--\n");
			}else if(0x13 == txPortBufPtr->Buffer[8] && 0x02 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--�����û���Ϣ--\n");
			}else if(0x21 == txPortBufPtr->Buffer[8] && 0x03 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--���ڲ�����Ϣ--\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x04 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--�������ؼ�����Ϣ--\n");
			}else if(0x31 == txPortBufPtr->Buffer[8] && 0x05 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--���ڸ���������Ϣ--\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x06 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--���ڳ�ֵ��Ϣ--\n");
			}else if(0x35 == txPortBufPtr->Buffer[8] && 0x07 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--����������Ϣ--\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x08 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--������ʷ��Ϣ--\n");
			}else if(0x19 == txPortBufPtr->Buffer[8] && 0x09 == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--����������Ϣ--\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0A == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--����������Ϣ--\n");
			}else if(0x09 == txPortBufPtr->Buffer[8] && 0x0B == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--����ֱ����Ϣ--\n");
			}else if(0x33 == txPortBufPtr->Buffer[8] && 0x0C == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--����ʵʱ��Ϣ--\n");
			}else if(0x0b == txPortBufPtr->Buffer[8] && 0x0D == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--�����¼���Ϣ--\n");
			}else if(0x11 == txPortBufPtr->Buffer[8] && 0x0E == txPortBufPtr->Buffer[9]){
				Gprs_OutputDebugMsg(FALSE, "\n--�����Զ�����--\n");
			}else if(0x43 == txPortBufPtr->Buffer[8] && (0x10 == txPortBufPtr->Buffer[9] || 0x11 == txPortBufPtr->Buffer[9])){
				Gprs_OutputDebugMsg(FALSE, "\n--ZigBee��������--\n");
			}else{
				Gprs_OutputDebugMsg(FALSE, "\n--������Ϣ--\n");
			}
			gprs_print(&txPortBufPtr->Buffer[0], txPortBufPtr->Length);
		}

		// ÿ�����ݵ� CRC16 У�飬���ʧ�ܣ��˰�����������У����һ�����ݡ�
		crc16 = phyCalCRC16(&txPortBufPtr->Buffer[MAC_ADDR_SIZE], DataFrmPtr->DataBuf[BufLen]-1);
		if((txPortBufPtr->Buffer[MAC_ADDR_SIZE+DataFrmPtr->DataBuf[BufLen]-1] != (uint8)((crc16 >> 8)&0xFF))
			|| (txPortBufPtr->Buffer[MAC_ADDR_SIZE+DataFrmPtr->DataBuf[BufLen]] != (uint8)((crc16)&0xFF))){
			// У��ʧ�ܺ���Ҫ�ͷ����õĿռ䣬����ָ��ָ����һ������ͷ��
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "Crc16 error\n");
			BufLen += OneBufLen;
			return ERROR;
		}

		// ���ݰ����ԣ��˴�Ϊ���͵� GPRS
		txPortBufPtr->Property.FilterDone = 1;
		txPortBufPtr->Property.PortNo = Uart_Gprs;

		// ��� Gprs δ���ӣ�Ӧ�����ݰ��洢����ʱ���䡣
		if (FALSE == Gprs.Online) {
			if ((void *)0 == (gprsTxBufPtr = OSMemGetOpt(GprsMemoryPtr, 10, TIME_DELAY_MS(50)))) {
				OSMemPut(LargeMemoryPtr, txPortBufPtr);
				Gprs_OutputDebugMsg(TRUE, "GprsMemoryPtr get error\n");
				return ERROR;
			}
			// ����һ��ͷ��ΪУ�飬��ֹ�������͡�
			gprsTxBufPtr[0] = GPRS_DELAY_UP_CRC0;
			gprsTxBufPtr[1] = GPRS_DELAY_UP_CRC1;
			gprsTxBufPtr[2] = GPRS_DELAY_UP_CRC2;
			gprsTxBufPtr[3] = GPRS_DELAY_UP_CRC3;

			// ����Ҫ���͵����ݷ��õ�У���
			memcpy(&gprsTxBufPtr[4], txPortBufPtr, MEM_GPRS_BLOCK_LEN - 4);

			// ��ʱ����ʱ��, ��λ��(s)
			GprsDelayDataUpTimer = 10;
			OSMemPut(LargeMemoryPtr, txPortBufPtr);
			Gprs_OutputDebugMsg(TRUE, "Gprs not online\n");
			BufLen += OneBufLen;
			continue;
		}else{
			// ���� gprs ������Ϣ
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
				// �����ź��� GPRS ģ�鴦������
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
* Decription   : ��������ģ����ز��������ܡ�
************************************************************************************************/
bool DataHandle_Switch_Monitor (DATA_FRAME_STRUCT *DataFrmPtr_SM, uint8 enable, uint8 PreambleLen)
{
	DATA_FRAME_STRUCT *DataFrmPtr;

	// ������һ���ڴ������м����ݴ���
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
* Decription   : �ɽ���������
* Input        : DataFrmPtr-���յ�������ָ��
* Output       : �Ƿ���Ҫ��������
* Others       : ������: ����[1]+����[1]+��˾����2��������1���ӱ��1
				���û�����4����ˮ���ʴ���2���������ݲ���4 + crc16У��[2]
************************************************************************************************/
bool ReadMeter_DataForwardProc(DATA_FRAME_STRUCT *dat)
{
	uint16 crc16 = 0;
	DATA_FRAME_STRUCT *DataFrmPtr;
	uint8 LongAddr1[6] = {0x01, 0x00, 0x13, 0x07, 0x17, 0x20};

    // ������һ���ڴ������м����ݴ���
    if ((void *)0 == (DataFrmPtr = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return ERROR;
    }
	memcpy(DataFrmPtr, dat, MEM_LARGE_BLOCK_LEN);

    // �ڵ��ڼ������������򴴽�ת��
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

	//�˴��� crc16 У�� 1021 ģʽУ��
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
* Decription   : ���ݴ�������,ֻ������յ��������¼�
* Input        : DataFrmPtr-����֡��ָ��
* Output       : ��
* Others       : �ú����������Ա�˻��������PC�����ֳֻ����͹�����ָ�����ָ����Ӧ��
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
        // �ɽ����� 0x31
        case Read_Songjiang_Meter_data:
            //�ɽ���������
            ReadMeter_DataForwardProc(DataFrmPtr);
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            break;

            // ������ģ����ز��������� 0x32
        case Open_SwitchMonitor:
            Gprs_OutputDebugMsg(FALSE, "\n ==== ������ģ����ز��������� ====\n");
            // ������ģ����ز���������
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            SwitchMonitor_EN = ENABLE;
            break;

            //�����ر�����ģ����ز��������� 0x33
        case Close_SwitchMonitor:
            Gprs_OutputDebugMsg(FALSE, "\n ==== �����ر�����ģ����ز��������� ====\n");
            DataHandle_Switch_Monitor(DataFrmPtr, DISABLE, 0x1E);
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            SwitchMonitor_EN = DISABLE;
            break;

            //��GprsDebug���� 0x34
        case Open_GprsDebug:
            Gprs_OutputDebugMsg(FALSE, "\n ==== ��GprsDebug���� ====\n");
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            GprsDebug = ENABLE;
            break;

            //�ر�GprsDebug���� 0x35
        case Close_GprsDebug:
            Gprs_OutputDebugMsg(FALSE, "\n ==== �ر�GprsDebug���� ====\n");
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DataFrmPtr->DataLen = 1;
            GprsDebug = DISABLE;
            break;

        // ����SIM���� 0x36
        case Write_SIM_NUM:
            Data_SetSimNum(DataFrmPtr);
            break;

        // ��ȡSIM���� 0x74
        case Read_SIM_NUM:
            memcpy(DataFrmPtr->DataBuf, Concentrator.GprsParam.SimCardId, 13);
            DataFrmPtr->DataLen = 13;
            break;

        // ���������ŵ����÷�ʽ 0x72
        case Read_CONC_Channel_Param:
            Data_RdWrConcChannelParam(DataFrmPtr);
            break;

        // д�������ŵ����÷�ʽ 0x73
        case Write_CONC_Channel_Param:
            Data_RdWrConcChannelParam(DataFrmPtr);
            break;

        // ��������ID 0x41
        case Read_CONC_ID:
            // ����:��������
            // ����:������ID��BCD��(6)
            memcpy(DataFrmPtr->DataBuf, Concentrator.LongAddr, LONG_ADDR_SIZE);
            DataFrmPtr->DataLen = LONG_ADDR_SIZE;
            break;

        // ��Gprs���� 0x45
        case Read_GPRS_Param:
            Data_GprsParameter(DataFrmPtr);
            break;

        // дGprs���� 0x46
        case Write_GPRS_Param:
            Data_GprsParameter(DataFrmPtr);
            break;

		// ��Gprs�ź�ǿ�� 0x47
		case Read_GPRS_RSSI:
			// ����:��
			// ����:�ź�ǿ��
			DataFrmPtr->DataBuf[0] = Gprs_GetCSQ();
			DataFrmPtr->DataBuf[1] = Gprs.Online ? 0x01 : 0x00;
			DataFrmPtr->DataLen = 2;
			DataFrmPtr->DataLen += Gprs_GetIMSI(&DataFrmPtr->DataBuf[DataFrmPtr->DataLen]);
			DataFrmPtr->DataLen += Gprs_GetGMM(&DataFrmPtr->DataBuf[DataFrmPtr->DataLen]);
			break;

		// �������������� 0x4C
		case Restart_CONC_Cmd:
			// ����:��
			// ����:����״̬
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			DevResetTimer = 5000;
			break;

		// ���������汾��Ϣ 0x40
		case Read_CONC_Version:
			// ����:��������
			// ����:����汾(2)+Ӳ���汾(2)+Э��汾(2)
			DataFrmPtr->DataLen = 0;
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)(SW_VERSION >> 8);
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)SW_VERSION;
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)(HW_VERSION >> 8);
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)HW_VERSION;
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)(PT_VERSION >> 8);
			DataFrmPtr->DataBuf[DataFrmPtr->DataLen++] = (uint8)PT_VERSION;
			break;

		// д������ID 0x42
		case Write_CONC_ID:
			Data_SetConcentratorAddr(DataFrmPtr);
			break;

		// ��������ʱ�� 0x43
		case Read_CONC_RTC:
			// ����:��������
			// ����:������ʱ��(7)
			Rtc_Get((RTC_TIME *)DataFrmPtr->DataBuf, Format_Bcd);
			DataFrmPtr->DataLen = 7;
			break;

		// д������ʱ�� 0x44
		case Write_CONC_RTC:
			// ����:������ʱ��(7)
			// ����:����״̬(1)
			if (SUCCESS == Rtc_Set(*(RTC_TIME *)(DataFrmPtr->DataBuf), Format_Bcd)) {
				DataFrmPtr->DataBuf[0] = OP_Succeed;
				GprsDelayDataUpTimer = 10;
				RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
			} else {
				DataFrmPtr->DataBuf[0] = OP_TimeAbnormal;
			}
			DataFrmPtr->DataLen = 1;
			break;

			//��Gprs������������ 0x37
		case Open_GprsSoftUp:
			Gprs_OutputDebugMsg(FALSE, "\n ==== ��Gprs������������ ====\n");
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			GprsSoftUp = GPRS_SOFT_UP_EN;
			OSFlagPost(GlobalEventFlag, FLAG_LOGOFF_EVENT, OS_FLAG_SET, &err);
			break;

			//�ر�Gprs������������ 0x38
		case Close_GprsSoftUp:
			Gprs_OutputDebugMsg(FALSE, "\n ==== �ر�Gprs������������ ====\n");
			DataFrmPtr->DataBuf[0] = OP_Succeed;
			DataFrmPtr->DataLen = 1;
			GprsSoftUp = 0x0;
			OSFlagPost(GlobalEventFlag, FLAG_LOGOFF_EVENT, OS_FLAG_SET, &err);
			break;

		// �������������� 0xF1
		case Software_Update_Cmd:
			Data_SwUpdate(DataFrmPtr);
			break;

		// ����ָ�֧��
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
* Decription   : ���ݴ�������,ֻ������յ���Ӧ���¼�
* Input        : DataBufPtr-��������ָ��
* Output       : ��
* Others       : �ú����������Ա�˻��������PC�����ֳֻ����͹�����Ӧ��
************************************************************************************************/
void DataHandle_RxAckProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 i;
    uint16 nodeId;
    DATA_HANDLE_TASK *taskPtr;

    // ����Ӧ��ڵ��Ƿ��ڱ�������
    nodeId = Data_FindNodeId(0, DataFrmPtr->Route[0]);

    // �жϸ�Ӧ��֡Ӧ�ô��ݸ�˭
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
* Decription   : ͸����������
* Input        : DataFrmPtr-���յ�������ָ��
* Output       : TRUE-�Ѿ�����,FALSE-û�д���
* Others       : Ŀ���ַ�����Լ�ʱ,���ݵ���һ���ڵ�
************************************************************************************************/
bool DataHandle_PassProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    // �����Ŀ��ڵ���������һ������
    if (DataFrmPtr->RouteInfo.CurPos == DataFrmPtr->RouteInfo.Level - 1) {
        return FALSE;
    }
    // ������������ǵ����ڶ��������豸����ѡ��ͨѶ�˿�,�����������RF�˿�
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

        TotalPackNum = DataFrmPtr->DataBuf[0] & 0x0F;		//�ܰ���
        PackNum = (DataFrmPtr->DataBuf[0]>> 4) & 0x0F ;		//��ǰ����
        if( BeforePackNum+1 != PackNum || TotalPackNum > 9){
            TotalPackNum = 0;
            PackNum = 0;
            BeforePackNum = 0;
            return 0;
        }else{
            BeforePackNum+=1;
        }

        //�ֽ����ݲ�ֱ���ϴ��� GPRS
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
                    // ��ӡ���ԣ�����ʱӦȥ��
                    Gprs_OutputDebugMsg(TRUE, "\n--RTC--\n");
                }
                DataHandle_SetRtcTiming(DataFrmPtr, PreambleLen);//����Уʱ������
            }else{
                PreambleLen = 0x01;
                DataHandle_SetRtcTiming(DataFrmPtr, PreambleLen);//����Уʱ������
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
* Decription   : ���ݴ�������,ֻ��������¼�
* Input        : *p_arg-����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void DataHandle_Task(void *p_arg)
{
    uint8 i, err, *dat;
    OS_FLAGS eventFlag;
    DATA_FRAME_STRUCT *dataFrmPtr;
    EXTRACT_DATA_RESULT ret;

    // ��ʼ������
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

    // ���ݳ�ʼ��
    Data_Init();
    DataHandle_SetRtcTiming((void *)0, 0x1);
    OSTimeDlyHMSM(0, 0, 0, 100);

    while (TRUE) {
        // ��ȡ�������¼�����
        eventFlag = OSFlagPend(GlobalEventFlag, (OS_FLAGS)DATAHANDLE_EVENT_FILTER, (OS_FLAG_WAIT_SET_ANY | OS_FLAG_CONSUME), TIME_DELAY_MS(5000), &err);

        // ������Щ����
        while (eventFlag != (OS_FLAGS)0) {
            dat = (void *)0;
            if (eventFlag & FLAG_USART_RF_RX) {
                // Rfģ���յ�������
                dat = OSMboxAccept(SerialPort.Port[Usart_Rf].MboxRx);
                eventFlag &= ~FLAG_USART_RF_RX;
            } else if (eventFlag & FLAG_GPRS_RX) {
                // Gprsģ���յ�������
                dat = OSMboxAccept(Gprs.MboxRx);
                eventFlag &= ~FLAG_GPRS_RX;
                if( (0x0 == GprsSoftUp) && ((void *)0 != dat) ){
                    DataHandle_GprsExtractData(dat);
                }
            } else if (eventFlag & FLAG_USB_RX) {
                // Usb�˿��յ�������
                dat = OSMboxAccept(SerialPort.Port[Usb_Port].MboxRx);
                eventFlag &= ~FLAG_USB_RX;
            } else if (eventFlag & FLAG_USART_DEBUG_RX) {
                // Debug�˿��յ�������
                dat = OSMboxAccept(SerialPort.Port[Usart_Debug].MboxRx);
                eventFlag &= ~FLAG_USART_DEBUG_RX;
            } else if (eventFlag & FLAG_UART_RS485_RX) {
                // 485�˿��յ�������
                dat = OSMboxAccept(SerialPort.Port[Uart_Rs485].MboxRx);
                eventFlag &= ~FLAG_UART_RS485_RX;
            } else if (eventFlag & FLAG_USART_IR_RX) {
                // Ir�˿��յ�������
                dat = OSMboxAccept(SerialPort.Port[Usart_Ir].MboxRx);
                eventFlag &= ~FLAG_USART_IR_RX;
            } else if (eventFlag & FLAG_DELAY_SAVE_TIMER) {
                // ������ʱ����
                eventFlag &= ~FLAG_DELAY_SAVE_TIMER;
                DataHandle_DataDelaySaveProc();
            } else if (eventFlag & FLAG_DATA_REPLENISH_TIMER) {
                // ���ݲ�������
                eventFlag &= ~FLAG_DATA_REPLENISH_TIMER;
                //DataHandle_DataReplenishProc();
            } else if (eventFlag & FLAG_DATA_UPLOAD_TIMER) {
                // �����ϴ�����
                eventFlag &= ~FLAG_DATA_UPLOAD_TIMER;
                //DataHandle_DataUploadProc();
            } else if (eventFlag & FLAG_RTC_TIMING_TIMER) {
                // ʱ������Уʱ����
                eventFlag &= ~FLAG_RTC_TIMING_TIMER;

                if( 1 == GetRtcOK ){
                    RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
                    continue;
                }
                // ���Gprs�Ƿ����߻��������Ƿ�����������
                if (FALSE == Gprs.Online) {
                    RTCTimingTimer = 30;
                    continue;
                }
                RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
                OSFlagPost(GlobalEventFlag, FLAG_GPRS_UPDATA_RTC, OS_FLAG_SET, &err);
            } else if (eventFlag & FLAG_GPRS_DELAY_UPLOAD) {
                // GPRS ����������ʱ�ϴ�����
                eventFlag &= ~FLAG_GPRS_DELAY_UPLOAD;
                DataHandle_GprsDelayDataUploadProc();
            }

            if ((void *)0 == dat) {
                continue;
            }

            // ��ԭ��������ȡ����
            if (Ok_Data != (ret = DataHandle_ExtractData(dat))) {
                if (Error_DstAddress == ret) {
                    // ������Ǹ��Լ������ݿ��Լ��������ڵ㲢��������Ӧ���ھӱ���
                    DataHandle_IncreaseSaveMeterDataProc(dat);
                }
                OSMemPut(LargeMemoryPtr, dat);
                continue;
            }
            dataFrmPtr = (DATA_FRAME_STRUCT *)dat;

            // �յ���ȷ�ı�����ݺ������������û�е�����ֱ�ӱ��浵����Ϣ�ͱ���Ϣ����������
            if(DataHandle_IncreaseSaveMeterDataProc(dat)){
                OSMemPut(LargeMemoryPtr, dat);
                continue;
            }

            // ȷ�������Ϣ�ϴ���ͨ��
            if (Usart_Debug == dataFrmPtr->PortNo || Usb_Port == dataFrmPtr->PortNo) {
                MonitorPort = (PORT_NO)(dataFrmPtr->PortNo);
            }

            // ���Ŀ���ַ�����Լ���ת��
            if (TRUE == DataHandle_PassProc(dataFrmPtr)) {
                continue;
            }

            // �ֱ�������֡��Ӧ��ָ֡��
            if (CMD_PKG == dataFrmPtr->PkgProp.PkgType) {
                // ���������֡
                DataHandle_RxCmdProc(dataFrmPtr);
            } else {
                // �����Ӧ��֡
                DataHandle_RxAckProc(dataFrmPtr);
            }
        }

        OSTimeDlyHMSM(0, 0, 0, 50);
    }
}

/***************************************End of file*********************************************/


