/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Gprs.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V2.1        08/15/2016      Zhangxp         SRWF-6009       Second Version
************************************************************************************************/

#define GPRS_GLOBALS
/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Main.h"
#include "Gprs.h"
#include "SerialPort.h"
#include "Led.h"
#include "Rtc.h"
#include "DataHandle.h"
#include "Database.h"
#include <string.h>

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
const char *GprsModuleList[] = {"M35", "EC20"};
RTC_TIME RtcTime;
extern uint8 GprsSoftUp;
uint8 AtQntpErr = 0;
extern uint8 GetRtcOK;
uint8 GprsSignInNum = 0;

/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Gprs_OutputDebugMsg
* Decription   : Gprs���������Ϣ
* Input        : NeedTime-�Ƿ���Ҫ����ʱ��,StrPtr-������Ϣָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Gprs_OutputDebugMsg(bool NeedTime, uint8 *StrPtr)
{
    uint16 i, len;
    RTC_TIME rtcTime;
    uint8 *bufPtr;

    if ((void *)0 == (bufPtr = OSMemGetOpt(LargeMemoryPtr, 20, TIME_DELAY_MS(50)))) {
        return;
    }
    len = 0;
    if (TRUE == NeedTime) {
        Rtc_Get(&rtcTime, Format_Bcd);
        len += BcdToAscii(&rtcTime.Hour, bufPtr + len, 1, 2);
        *(bufPtr + len++) = ':';
        len += BcdToAscii(&rtcTime.Minute, bufPtr + len, 1, 2);
        *(bufPtr + len++) = ':';
        len += BcdToAscii(&rtcTime.Second, bufPtr + len, 1, 2);
        *(bufPtr + len++) = ' ';
    }
    for (i = 0; i < strlen((char *)StrPtr); i++) {
        *(bufPtr + len++) = *(StrPtr + i);
    }
    DataHandle_OutputMonitorMsg(Gprs_Connect_Msg, bufPtr, len);
    OSMemPut(LargeMemoryPtr, bufPtr);
    return;
}

/************************************************************************************************
* Function Name: Gprs_ConnectProc
* Decription   : Gprsģ�����Ӵ������
* Input        : CmdPtr-����ָ��
*                CmdLen-���ݳ���,0-��ʾCmdPtr���ַ���,����strlen����,������0x0D0x0A��β;
*                                ����-����ĳ���,��ʾCmdPtr��ʮ����������,������CTRL+A��β
*                AckStr1-Ӧ��1�ַ���ָ��,#Ϊͨ���
*                AckStr2-Ӧ��2�ַ���ָ��,#Ϊͨ���
*                WaitAckTime-�ȴ�Ӧ���ʱ��
*                PeriodTime-����ʱ�ȴ���ʱ��
*                TryTimes-Ӧ��ʧ�ܻ���Ӧ��ʱ���ԵĴ���
*                PostHandle-�Ƿ���Ҫ��������,TRUE�򽫷�������ָ��,����������ɺ����Ҫ�ͷ����
*                           ������,FALSE���ͷ����ݻ�����
* Output       : ʧ�ܷ���0,�ɹ��򷵻�һ��ָ���ڴ�ռ��ָ��(��PostHandleΪTRUEʱ)��(void *)1
* Others       : ע��,����ɹ�,����øú����ĺ��������ڴ����귵�ص��ڴ����ݺ��ͷŸ��ڴ�
************************************************************************************************/
void *Gprs_ConnectProc(const char *CmdPtr,
                          uint16 CmdLen,
                          const char *AckStr1,
                          const char *AckStr2,
                          uint32 WaitAckTime,
                          uint32 PeriodTime,
                          uint8 TryTimes,
                          bool PostHandle)
{
    uint8 i, count, loop, len, err, *txMsg, *rxMsg;
    uint16 j, k;
    const char *ackStr;
    PORT_BUF_FORMAT *bufPtr;
    SERIALPORT_PORT *portPtr = &SerialPort.Port[Uart_Gprs];

    OSTimeDlyHMSM(0, 0, 0, 50);
    for (loop = 0; loop < TryTimes; ) {
        if ((void *)0 == (txMsg = OSMemGet(LargeMemoryPtr, &err))) {
            OSTimeDlyHMSM(0, 0, 3, 0);
            continue;
        }

        // ���뵽���ڴ�,�򴴽��������ݲ����ݸ�����
        bufPtr = (PORT_BUF_FORMAT *)txMsg;
        if (CmdLen == 0) {
            strcpy((char *)bufPtr->Buffer, CmdPtr);
            bufPtr->Length = strlen(CmdPtr);
            bufPtr->Buffer[bufPtr->Length++] = 0x0D;
            bufPtr->Buffer[bufPtr->Length++] = 0x0A;
        } else {
            memcpy(bufPtr->Buffer, CmdPtr, CmdLen);
            bufPtr->Length = CmdLen;
            bufPtr->Buffer[bufPtr->Length++] = 0x1A;
        }

        // ע��Ҫ�ȴ�����,����ս��ջ�����,���Flag
        if (OSMboxPost(portPtr->MboxTx, txMsg) != OS_ERR_NONE) {
            OSMemPut(LargeMemoryPtr, txMsg);
            OSTimeDlyHMSM(0, 0, 1, 0);
            continue;
        }
        rxMsg = OSMboxAccept(portPtr->MboxRx);                                  // ��ս��ջ�����������,�п�������һ��ͨ������������û�д��������
        if ((void *)0 != rxMsg) {
            OSMemPut(LargeMemoryPtr, rxMsg);
        }
        OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_UART_GPRS_TX, OS_FLAG_SET, &err);

        // �ȴ����ڴ�����Gprsģ���Ӧ������
        rxMsg = OSMboxPend(portPtr->MboxRx, WaitAckTime, &err);

        // ���û���յ�����������
        if ((void *)0 == rxMsg) {
            loop += 1;
            continue;
        }

        // ����յ���������֤���ݵ���ȷ��,��ȷ�򷵻ظ�����ָ��
        for (count = 0; count < 2; count++) {
            bufPtr = (PORT_BUF_FORMAT *)rxMsg;
            for (i = 0; i < 2; i++) {
                ackStr = i == 0 ? AckStr1 : AckStr2;
                if ((void *)0 == ackStr) {
                    continue;
                }
                len = strlen(ackStr);
                for (j = 0; j < bufPtr->Length - len; j++) {
                    for (k = 0; k < len; k++) {
                        if (*(ackStr + k) != bufPtr->Buffer[j + k] &&
                            *(ackStr + k) != '#') {
                            break;
                        }
                    }
                    if (k >= len) {
                        if (TRUE == PostHandle) {
                            return rxMsg;
                        } else {
                            OSMemPut(LargeMemoryPtr, rxMsg);
                            return ((void *)1);
                        }
                    }
                }
            }
            OSMemPut(LargeMemoryPtr, rxMsg);
            if (count >= 1) {
                break;
            }

            // ��ʱ��Ŀ����Ϊ�˵ȴ�Gprs����Ӧʱ�����Gprs�Ĵ���ʱ��
            OSTimeDly(PeriodTime);

            // �����ʱ���ʱ�����Ƿ������ݷ���
            rxMsg = OSMboxAccept(portPtr->MboxRx);
            if ((void *)0 == rxMsg) {
                break;
            }
        }

        loop += 1;
    }

    return ((void *)0);
}

/************************************************************************************************
* Function Name: Gprs_PowerOff
* Decription   : Gprsģ��ػ�
* Input        : ��
* Output       : ��
* Others       : Ӳ�ػ�,Gprsģ���Դ�ϵ�
************************************************************************************************/
static void Gprs_PowerOff(void)
{
    GPRS_POWER_OFF();
    GPRS_PWRKEY_UP();
    GPRS_EMERG_OFF();
}

/************************************************************************************************
* Function Name: Gprs_PowerON
* Decription   : Gprsģ�鿪��
* Input        : ��
* Output       : SUCCESS-�������ɹ�,ERROR-������ʧ��
* Others       : Ӳ����,Gprsģ���Դ����,��ȷ��ģ���Ѿ�����
************************************************************************************************/
static bool Gprs_PowerOn(void)
{
    uint8 i;

    // Gprsģ���ϵ粢��ʼ�������ŵ�ƽ
    GPRS_POWER_ON();
    GPRS_PWRKEY_UP();
    GPRS_EMERG_OFF();

    for (i = 0; i < 5; i++) {
        if ((void *)1 == Gprs_ConnectProc("AT", 0, "OK", "AT", TIME_DELAY_MS(300), TIME_DELAY_MS(300), 50, FALSE)) {
            return SUCCESS;
        }
        GPRS_PWRKEY_DOWN();
        OSTimeDlyHMSM(0, 0, 5, 0);
        GPRS_PWRKEY_UP();
    }
    return ERROR;
}

/************************************************************************************************
* Function Name: Gprs_Restart
* Decription   : Gprsģ��������
* Input        : ��
* Output       : SUCCESS-�������ɹ�,ERROR-������ʧ��
* Others       : Gprsģ���Դһֱ����,��PwrKey������ʵ��������
************************************************************************************************/
static bool Gprs_Restart(void)
{
    uint8 i;

    // ȷ���Ƿ��Ѿ��ػ�
    for (i = 0; i < 5; i++) {
        if ((void *)0 == Gprs_ConnectProc("AT", 0, "OK", "AT", TIME_DELAY_MS(300), TIME_DELAY_MS(100), 3, FALSE)) {
            break;
        }
        Gprs_ConnectProc("AT+QPOWD=0", 0, "OK", (void *)0, TIME_DELAY_MS(300), TIME_DELAY_MS(50), 1, FALSE);
    }
    // ͨ���ܽ�ģ�ⰴ������
    GPRS_POWER_ON();
    GPRS_PWRKEY_UP();
    GPRS_EMERG_OFF();
    OSTimeDlyHMSM(0, 0, 1, 0);
    GPRS_PWRKEY_DOWN();
    OSTimeDlyHMSM(0, 0, 5, 0);
    GPRS_PWRKEY_UP();
    if ((void *)1 == Gprs_ConnectProc("AT", 0, "OK", "AT", TIME_DELAY_MS(300), TIME_DELAY_MS(300), 100, FALSE)) {
        return SUCCESS;
    }
    return ERROR;
}

/************************************************************************************************
* Function Name: Gprs_IpFormatConvert
* Decription   : Gprs��Ip��ַ��ʽת��
* Input        : StrPtr-ָ���ַ�����ָ��, BufPtr-ָ�������ָ��, Dir-0:�ַ���ת����,1:����ת�ַ���
* Output       : ��
* Others       : ��
************************************************************************************************/
void Gprs_IpFormatConvert(uint8 *StrPtr, uint8 *BufPtr, uint8 Dir)
{
    uint8 i, j, val, div, flag;

    if (Dir) {
        for (i = 0; i < 4; i++) {
            val = *BufPtr++;
            div = 100;
            flag = 0;
            for (j = 0; j < 2; j++) {
                *StrPtr = val / div;
                if (1 == flag || *StrPtr > 0) {
                    flag = 1;
                    *StrPtr++ += '0';
                }
                val %= div;
                div /= 10;
            }
            *StrPtr++ = val + '0';
            *StrPtr++ = '.';
        }
        *(StrPtr - 1) = 0;
    } else {
        val = 0;
        for (i = 0, j = 0; i < strlen((char *)StrPtr) && j < 4; i++) {
            if ('.' == *(StrPtr + i)) {
                *(BufPtr + j) = val;
                j += 1;
                val = 0;
            } else if (*(StrPtr + i) >= '0' && *(StrPtr + i) <= '9') {
                val *= 10;
                val += *(StrPtr + i) - '0';
            }
        }
        *(BufPtr + j) = val;
    }
}

/************************************************************************************************
* Function Name: Gprs_TxPkgHandle_New
* Decription   : Gprs�������ݰ�֮ǰ�Ĵ������
* Input        : Cmd-��������,BufPtr-ָ�����ݵ�ָ��
* Output       : TRUE-��ȷ,FALSE-����
* Others       : ��
************************************************************************************************/
bool Gprs_TxPkgHandle_New(uint8 Cmd, uint8 *BufPtr)
{
    uint16 i;
    PORT_BUF_FORMAT *ptr;

    // Ҫ��ԭ���ݵĻ���������28���ֽ�
    ptr = (PORT_BUF_FORMAT *)BufPtr;
    if (ptr->Length + 28 > MEM_LARGE_BLOCK_LEN - 3) {
        return FALSE;
    }

    // ԭ���ݺ���27���ֽ�
    for (i = 1; i <= ptr->Length; i++) {
        ptr->Buffer[ptr->Length + 27 - i] = ptr->Buffer[ptr->Length - i];
    }
    ptr->Length += 27;

    // ����ͷ����Ϣ
    // 4 ���ֽڵĵ�¼֡��ʶ���磬�Ϻ����� = SHRQ = 53 48 52 51
    ptr->Buffer[0] = 0x53;
    ptr->Buffer[1] = 0x48;
    ptr->Buffer[2] = 0x52;
    ptr->Buffer[3] = 0x51;

    // 8 ���ֽڵ��豸���к�( MAC ��ַ) = �豸�������� + ���к�
    memcpy(&ptr->Buffer[4], Concentrator.GprsParam.MacAddr, 8);

	// 16 ���ֽڵ� SIM ���ţ�����16�ֽڵĺ�߲��ո�
	memcpy(&ptr->Buffer[12], Concentrator.GprsParam.SimCardId, 16);

    return TRUE;
}

/************************************************************************************************
* Function Name: Gprs_SignInPkgHandle
* Decription   : Gprs ��¼֡
* Input        : BufPtr-ָ�����ݵ�ָ��
* Output       : TRUE-��ȷ,FALSE-����
* Others       : ��¼֡��ʶ[4] + �豸���к�[8] + SIM����[16](11+5�ո�) + �س�[1] + ����[1]
************************************************************************************************/
uint16 Gprs_SignInPkgHandle(uint8 *BufPtr)
{
	uint16 len = 0;
    // �Ϻ����� = SHRQ = 53 48 52 51
	Concentrator.GprsParam.SignInIdentification[0] = 0x53;
	Concentrator.GprsParam.SignInIdentification[1] = 0x48;
	Concentrator.GprsParam.SignInIdentification[2] = 0x52;
	Concentrator.GprsParam.SignInIdentification[3] = 0x51;

    // 4 ���ֽڵĵ�¼֡��ʶ���磬�Ϻ����� = SHRQ = 53 48 52 51
    memcpy(&BufPtr[len], Concentrator.GprsParam.SignInIdentification, 4);
	len += 4;
#if 0
	// 8 ���ֽڵ��豸���к�( MAC ��ַ) = SR + ��������ַ
	memcpy(Concentrator.GprsParam.MacAddr, Concentrator.LongAddr, LONG_ADDR_SIZE);
	Concentrator.GprsParam.MacAddr[6] = 0x53;//S
	Concentrator.GprsParam.MacAddr[7] = 0x52;//R
#else
	// 8 ���ֽڵ��豸���к�( MAC ��ַ) = SR + ��������ַ
	Concentrator.GprsParam.MacAddr[0] = 0x53;//S
	Concentrator.GprsParam.MacAddr[1] = 0x52;//R
	memcpy(Concentrator.GprsParam.MacAddr+2, Concentrator.LongAddr, LONG_ADDR_SIZE);
#endif
    memcpy(&BufPtr[len], Concentrator.GprsParam.MacAddr, 8);
	len += 8;

	// 16 ���ֽڵ� SIM ���ţ�����16�ֽڵĺ�߲��ո�
	memcpy(&BufPtr[len], Concentrator.GprsParam.SimCardId, 16);
	len += 16;

	// 2 ���ֽڵĻس�+����
	BufPtr[len++] = 0x0D;
	BufPtr[len++] = 0x0A;

	return len;
}

/************************************************************************************************
* Function Name: Gprs_TxPkgHandle
* Decription   : Gprs�������ݰ�֮ǰ�Ĵ������
* Input        : Cmd-��������:0x01Ϊ������,0x09Ϊ���ݰ�,BufPtr-ָ�����ݵ�ָ��
* Output       : TRUE-��ȷ,FALSE-����
* Others       : ��
************************************************************************************************/
bool Gprs_TxPkgHandle(uint8 Cmd, uint8 *BufPtr)
{
    uint16 i;
    PORT_BUF_FORMAT *ptr;
	if( GPRS_SOFT_UP_EN == GprsSoftUp ){
	    // Ҫ��ԭ���ݵĻ���������16���ֽ�
	    ptr = (PORT_BUF_FORMAT *)BufPtr;
	    if (ptr->Length + 16 > MEM_LARGE_BLOCK_LEN - 3) {
	        return FALSE;
	    }

	    // β������0x7B��β�ַ�
	    ptr->Buffer[ptr->Length++] = 0x7B;

	    // ԭ���ݺ���15���ֽ�
	    for (i = 1; i <= ptr->Length; i++) {
	        ptr->Buffer[ptr->Length + 15 - i] = ptr->Buffer[ptr->Length - i];
	    }
	    ptr->Length += 15;

	    // ����ͷ����Ϣ
	    ptr->Buffer[0] = 0x7B;
	    ptr->Buffer[1] = Cmd;
	    ptr->Buffer[2] = (uint8)(ptr->Length >> 8);
	    ptr->Buffer[3] = (uint8)(ptr->Length);

	    // �������ʶ����Ϣ
	    BcdToAscii(&Concentrator.LongAddr[0], &ptr->Buffer[4], 1, 2);
	    ptr->Buffer[4] = ptr->Buffer[5];
	    BcdToAscii(&Concentrator.LongAddr[1], &ptr->Buffer[5], 1, 2);
	    BcdToAscii(&Concentrator.LongAddr[2], &ptr->Buffer[7], 1, 2);
	    BcdToAscii(&Concentrator.LongAddr[3], &ptr->Buffer[9], 1, 2);
	    BcdToAscii(&Concentrator.LongAddr[4], &ptr->Buffer[11], 1, 2);
	    BcdToAscii(&Concentrator.LongAddr[5], &ptr->Buffer[13], 1, 2);
	}

    return TRUE;
}

/************************************************************************************************
* Function Name: Gprs_GetCSQ
* Decription   : Gprsģ�����ź�����
* Input        : ��
* Output       : �ź����� 0-31��99
* Others       : ��
************************************************************************************************/
uint8 Gprs_GetCSQ(void)
{
    uint8 csq, *msg, *p;

    msg = Gprs_ConnectProc("AT+CSQ", 0, "+CSQ: ", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 2, TRUE);
    if ((void *)0 != msg) {
        p = (uint8 *)strstr((char *)(((PORT_BUF_FORMAT *)msg)->Buffer), "+CSQ: ") + strlen("+CSQ: ");
        csq = *p++ - '0';
        if (*p >= '0' && *p <= '9') {
            csq *= 10;
            csq += *p - '0';
        }
    } else {
        csq = 99;
    }
    OSMemPut(LargeMemoryPtr, msg);

    return csq;
}

/************************************************************************************************
* Function Name: Gprs_GetIMSI
* Decription   : Gprsģ����SIM����IMSI
* Input        : BufPtr-�������ݵ�ָ��
* Output       : �������ݵĳ���
* Others       : ��
************************************************************************************************/
uint8 Gprs_GetIMSI(uint8 *BufPtr)
{
    uint8 i, len;
    PORT_BUF_FORMAT *portBufPtr;

    len = 0;
    portBufPtr = Gprs_ConnectProc("AT+CIMI", 0, "OK", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 3, TRUE);
    if ((void *)0 != portBufPtr) {
        for (i = 1; i < portBufPtr->Length; i++) {
            if (portBufPtr->Buffer[i - 1] == 0x0D && portBufPtr->Buffer[i] == 0x0A) {
                i += 1;
                break;
            }
        }
        for (; i < portBufPtr->Length; i++) {
            if (portBufPtr->Buffer[i] >= '0' && portBufPtr->Buffer[i] <= '9') {
                *(BufPtr + 1 + len++) = portBufPtr->Buffer[i];
            } else {
                break;
            }
        }
    }
    *BufPtr = len;
    OSMemPut(LargeMemoryPtr, portBufPtr);
    return len + 1;
}

/************************************************************************************************
* Function Name: Gprs_GetGMM
* Decription   : Gprsģ����ģ���ͺ�
* Input        : BufPtr-�������ݵ�ָ��
* Output       : �������ݵĳ���
* Others       : ��
************************************************************************************************/
uint8 Gprs_GetGMM(uint8 *BufPtr)
{
    uint8 i, len;
    PORT_BUF_FORMAT *portBufPtr;

    len = 0;
    portBufPtr = Gprs_ConnectProc("AT+GMM", 0, "OK", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(300), 5, TRUE);
    if ((void *)0 != portBufPtr) {
        for (i = 1; i < portBufPtr->Length; i++) {
            if (portBufPtr->Buffer[i - 1] == 0x0D && portBufPtr->Buffer[i] == 0x0A) {
                i += 1;
                break;
            }
        }
        for (; i < portBufPtr->Length - 1; i++) {
            if (portBufPtr->Buffer[i] == 0x0D && portBufPtr->Buffer[i + 1] == 0x0A) {
                break;
            } else {
                *(BufPtr + 1 + len++) = portBufPtr->Buffer[i];
            }
        }
    }
    *BufPtr = len;
    OSMemPut(LargeMemoryPtr, portBufPtr);
    return len + 1;
}

/************************************************************************************************
* Function Name: Gprs_SetCNUM
* Decription   : Gprsģ������SIM����
* Others       : ��
************************************************************************************************/
uint8 Gprs_SetCNUM(void)
{
	uint8 *ret;

	ret = Gprs_ConnectProc("AT+CPBS=?", 0, "+CPBS:", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 3, FALSE);
	if ((void *)0 == ret) {
		Gprs_OutputDebugMsg(FALSE, "\nAT+CPBS=?\n");
	}
	ret = Gprs_ConnectProc("AT+CPBS?", 0, "+CPBS:", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 3, FALSE);
	if ((void *)0 == ret) {
		Gprs_OutputDebugMsg(FALSE, "\nAT+CPBS?\n");
	}

	ret = Gprs_ConnectProc("AT+CPBS=\"ON\"", 0, "OK", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 3, FALSE);
	if ((void *)0 == ret) {
		Gprs_OutputDebugMsg(FALSE, "\nAT+CPBS=ON\n");
	}
	ret = Gprs_ConnectProc("AT+CPBW=1,\"13901234567\",161,\"cell\"", 0, "OK", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 3, FALSE);
	if ((void *)0 == ret) {
		Gprs_OutputDebugMsg(FALSE, "\nAT+CPBW=1\n");
	}
	return *ret;
}

/************************************************************************************************
* Function Name: Gprs_GetCNUM
* Decription   : Gprsģ����SIM����
* Input        : BufPtr-�������ݵ�ָ��
* Output       : �������ݵĳ���
* Others       : ��
************************************************************************************************/
void* Gprs_GetCNUM(void)
{
    uint8 i, ret = 0;
    PORT_BUF_FORMAT *portBufPtr;

    portBufPtr = Gprs_ConnectProc("AT+CNUM", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(2000), 3, TRUE);
    if ((void *)0 != portBufPtr) {
        if(portBufPtr->Length == 0x06){
                OSMemPut(LargeMemoryPtr, portBufPtr);
                ret = 2;
                return ((void *)ret);
        }
        if (Quectel_M35 == Gprs.ModuleType) {
            for (i = portBufPtr->Length; i < portBufPtr->Length + 100; i++) {
                if ((portBufPtr->Buffer[i] == 0x2c) && (portBufPtr->Buffer[i + 1] == 0x22)
                    && (portBufPtr->Buffer[i+13] == 0x22) && (portBufPtr->Buffer[i+14] == 0x2c)) {// ,"xxxxxxxxxxx",
                    memset(Concentrator.GprsParam.SimCardId, 0x20, 16);
                    memcpy(Concentrator.GprsParam.SimCardId, &portBufPtr->Buffer[i+2], 11);
                    ret = 1;
                    break;
                }
                if ((portBufPtr->Buffer[i] == 0x2c) && (portBufPtr->Buffer[i + 1] == 0x22) && (portBufPtr->Buffer[i + 2] == 0x2B)
                    && (portBufPtr->Buffer[i+16] == 0x22) && (portBufPtr->Buffer[i+17] == 0x2c)) {// ,"+86xxxxxxxxxxx",
                    memset(Concentrator.GprsParam.SimCardId, 0x20, 16);
                    memcpy(Concentrator.GprsParam.SimCardId, &portBufPtr->Buffer[i+5], 11);
                    ret = 1;
                    break;
                }
            }
        }else if(Quectel_Ec20 == Gprs.ModuleType){
            for (i = 0; i < portBufPtr->Length; i++) {
                if ((portBufPtr->Buffer[i] == 0x2c) && (portBufPtr->Buffer[i + 1] == 0x22)
                    && (portBufPtr->Buffer[i+13] == 0x22) && (portBufPtr->Buffer[i+14] == 0x2c)) {// ,"xxxxxxxxxxx",
                    memset(Concentrator.GprsParam.SimCardId, 0x20, 16);
                    memcpy(Concentrator.GprsParam.SimCardId, &portBufPtr->Buffer[i+2], 11);
                    ret = 1;
                    break;
                }
                if ((portBufPtr->Buffer[i] == 0x2c) && (portBufPtr->Buffer[i + 1] == 0x22) && (portBufPtr->Buffer[i + 2] == 0x2B)
                    && (portBufPtr->Buffer[i+16] == 0x22) && (portBufPtr->Buffer[i+17] == 0x2c)) {// ,"+86xxxxxxxxxxx",
                    memset(Concentrator.GprsParam.SimCardId, 0x20, 16);
                    memcpy(Concentrator.GprsParam.SimCardId, &portBufPtr->Buffer[i+5], 11);
                    ret = 1;
                    break;
                }
            }
        }
    }

    OSMemPut(LargeMemoryPtr, portBufPtr);
    return ((void *)ret);
}

/************************************************************************************************
* Function Name: Gprs_Init
* Decription   : Gprsģ���ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Gprs_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    // Gprs���ƶ˿ڳ�ʼ��
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;                                  // Gprsģ�������״̬PB5:H-����,L-�ػ�
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;                     // Gprsģ��Pwr_Key(PB8)��Emerg_Off(PB9)
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;                                  // Gprs��Դ��������PC9
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Gprs�������ŵ�ƽ��ʼ��,������ʼ��,ע����ʱ��OS��û������
    Gprs.Cmd = GprsPowerOn;
    Gprs.RestartCount = 0;
    Gprs.ClosePDPCount = 0;
    Gprs.ConnectCount = 0;
    Gprs.HeartbeatRetryTime = 0;
    Gprs.WorkTime = 0;
    Gprs.Online = FALSE;
    Gprs.HeartbeatInterval = Concentrator.GprsParam.HeatBeatPeriod * 10;
}


/************************************************************************************************
* Function Name: Gprs_Task
* Decription   : Gprs���ӹ���������
* Input        : *p_arg-����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Gprs_Task(void *p_arg)
{
    uint8 err, *ret, *msg, *cmd, *dscIpPtr, RtcErr;
    uint16 i, loop, dscPort, len;
    PORT_BUF_FORMAT *bufPtr;
    OS_FLAGS eventFlag;
    uint8 timezone, addsub;

    (void)p_arg;

    while (TRUE) {
        switch (Gprs.Cmd) {
            case GprsPowerOff:
                Gprs_OutputDebugMsg(TRUE, "Gprsģ��ػ�...\n");
                Led_FlashTime(GPRS_LED, Delay100ms, AlwaysOff, FALSE);
                Gprs_PowerOff();
                OSTimeDlyHMSM(0, 0, 1, 0);
                Gprs.Cmd = GprsPowerOn;
                break;

            case GprsPowerOn:
                Gprs_OutputDebugMsg(TRUE, "Gprsģ�鿪��...\n");
                Led_FlashTime(GPRS_LED, Delay50ms, Delay1000ms, FALSE);
                Gprs_PowerOn();
                Gprs.Cmd = GprsConfig;
                break;

			case GprsRestart:
				// ���δ����Ŀ����Ϊ�˷�ֹGprsģ��Ƶ������������ģ��
				Gprs.Online = FALSE;
				if (Gprs.WorkTime > GPRS_NORMALWORKTIME) {
					Gprs.RestartCount = 0;
				}
				if (Gprs.RestartCount < GPRS_CONTINUERESTARTTIME &&
					Gprs.ClosePDPCount < GPRS_CONTINUECLOSEPDPTIME) {
					Gprs_OutputDebugMsg(TRUE, "Gprsģ����������,���Ժ�...\n");
					Led_FlashTime(GPRS_LED, Delay50ms, Delay1000ms, TRUE);
					Gprs_Restart();
					Gprs.Cmd = GprsConfig;
					Gprs.RestartCount++;
				} else {
					Gprs_OutputDebugMsg(TRUE, "Gprsģ����������������������,���ػ�10���Ӻ�����,���Ժ�...\n");
					Led_FlashTime(GPRS_LED, Delay100ms, AlwaysOff, TRUE);
					Gprs_PowerOff();
					OSTimeDlyHMSM(0, GPRS_POWERDOWNTIME, 0, 0);
					Gprs.Cmd = GprsPowerOn;
					Gprs.RestartCount = 0;
					Gprs.ClosePDPCount = 0;
					Gprs.ConnectCount = 0;
				}
				Gprs.WorkTime = 0;
				break;

			case GprsConfig:
                Gprs.Online = FALSE;
                // �ı�ָʾ����˸״̬-δע�ᵽ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay800ms, FALSE);
                // ����ATָ��ʹģ���ͨ�Ų������봮��ƥ��
                Gprs_OutputDebugMsg(FALSE, "Gprsģ����������...");
                ret = Gprs_ConnectProc("AT", 0, "OK", (void *)0, TIME_DELAY_MS(500), TIME_DELAY_MS(500), 10, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   δ��⵽Gprsģ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ����ATE0ָ��ػ���
                ret = Gprs_ConnectProc("ATE0", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 5, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   �رջ��Թ���ʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ����ͨ�Ų�����,���ǲ�Ҫд��,�˲������ʡ��
                ret = Gprs_ConnectProc("AT+IPR=115200", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 5, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ����ͨ�Ų�����ʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ��ȡGPRSģ���ͺ�
                Gprs.ModuleType = Unknow_Type;
                msg = Gprs_ConnectProc("AT+GMM", 0, "OK", (void *)0, TIME_DELAY_MS(1000), TIME_DELAY_MS(300), 5, TRUE);
                if ((void *)0 != msg) {
                    bufPtr = (PORT_BUF_FORMAT *)msg;
                    for (Gprs.ModuleType = Quectel_M35; Gprs.ModuleType < Unknow_Type; Gprs.ModuleType++) {
                        for (i = 0; i < bufPtr->Length - strlen(GprsModuleList[Gprs.ModuleType]); i++) {
                            if (0 == strncmp((const char *)(&bufPtr->Buffer[i]), GprsModuleList[Gprs.ModuleType], strlen(GprsModuleList[Gprs.ModuleType]))) {
                                i = 1000;
                                break;
                            }
                        }
                        if (i >= 1000) {
                            break;
                        }
                    }
                    OSMemPut(LargeMemoryPtr, msg);
                }
                if (Gprs.ModuleType >= Unknow_Type) {
                    Gprs_OutputDebugMsg(FALSE, "\n    δ֪��GPRSģ������\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ��ѯSIM���Ƿ����
                ret = Gprs_ConnectProc("AT+CPIN?", 0, "+CPIN: READY", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(2000), 5, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   SIM������쳣\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                Gprs_OutputDebugMsg(FALSE, "�ɹ�\n��⵽GPRSģ������Ϊ");
                Gprs_OutputDebugMsg(FALSE, (uint8 *)(GprsModuleList[Gprs.ModuleType]));
                Gprs_OutputDebugMsg(FALSE, "\n");
                Gprs.Cmd = GprsGsmStatus;
                OSTimeDlyHMSM(0, 0, 0, 500);
                break;

			case GprsGsmStatus:
                // �ı�ָʾ����˸״̬-δע�ᵽ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay800ms, FALSE);
                // Gsm����ע��״̬�ж�
                Gprs_OutputDebugMsg(FALSE, "�������ע��״̬...");
                //ret = Gprs_ConnectProc("AT+CREG?", 0, "+CREG: #,1", "+CREG: #,5", TIME_DELAY_MS(5000), TIME_DELAY_MS(2000), 10, FALSE);
                /**
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ����δע��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                **/
                // Gprs����ע��״̬
                ret = Gprs_ConnectProc("AT+CGREG?", 0, "+CGREG: #,1", "+CGREG: #,5", TIME_DELAY_MS(5000), TIME_DELAY_MS(2000), 20, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   Gprs����δע��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                Gprs_OutputDebugMsg(FALSE, "�ɹ�\n");
                Gprs.Cmd = Quectel_M35 == Gprs.ModuleType ? GprsDataFormat : GprsOpenPdp;
                // �ı�ָʾ����˸״̬(����״̬,��ע�ᵽ����)
                Led_FlashTime(GPRS_LED, Delay50ms, Delay500ms, FALSE);
                break;

			case GprsDataFormat:
                // �ı�ָʾ����˸״̬-δע�ᵽ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay500ms, FALSE);
                Gprs_OutputDebugMsg(FALSE, "����Gprs����ͨѶ��ʽ...");
                // ����ǰ�ó���
                ret = Gprs_ConnectProc("AT+QIFGCNT=0", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 2, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ����ǰ�ó���ʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ���ó�Gprs����ģʽ
                ret = Gprs_ConnectProc("AT+QICSGP=1", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 2, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ����ΪGprs���ӷ�ʽʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ��������ʱ����ʾIPͷ
                ret = Gprs_ConnectProc("AT+QIHEAD=0", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 2, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ���ý�������ʱ����ʾIPͷʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ���óɽ�������ʱ����ʾIP��ַ�Ͷ˿ں�
                ret = Gprs_ConnectProc("AT+QISHOWRA=0", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 2, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ���ý�������ʱ����ʾIP��ַʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ���ý�������ʱIPͷ����ʾIPЭ��
                ret = Gprs_ConnectProc("AT+QISHOWPT=0", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 2, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ���ò���ʾ����Э��ʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // ����TCPӦ��ģʽΪ��͸��
                ret = Gprs_ConnectProc("AT+QIMODE=0", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 5, FALSE);
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "\n   ����TCPΪ��͸��ģʽʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                Gprs_OutputDebugMsg(FALSE, "�ɹ�\n");
                Gprs.Cmd = GprsOpenPdp;
                break;

            case GprsOpenPdp:
                // �ı�ָʾ����˸״̬-δע�ᵽ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay500ms, FALSE);
                Gprs_OutputDebugMsg(FALSE, "����TCP���ӽ���...");
                // �����ڴ�ռ�,�������ʧ��,��ÿ��2����������һ��
                if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
                    Gprs_OutputDebugMsg(FALSE, "\n   �����ڴ�ռ�ʧ��\n");
                    OSTimeDlyHMSM(0, 0, 2, 0);
                    break;
                }
                if (Quectel_M35 == Gprs.ModuleType) {
                    // ���ñ��ض˿ں�
                    strcpy((char *)cmd, "AT+QILPORT=\"TCP\",");
                    Uint16ToString(GPRS_LOCAL_PORT, &cmd[strlen((char *)cmd)]);
                    ret = Gprs_ConnectProc((char *)cmd, 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(500), 2, FALSE);
                    if ((void *)0 == ret) {
                        Gprs_OutputDebugMsg(FALSE, "\n   ���ñ��ض˿�ʧ��\n");
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsRestart;
                        break;
                    }
                    // ���ý����Apn,Username��Password
                    if (strlen(Concentrator.GprsParam.Apn) > 0) {
                        strcpy((char *)cmd, "AT+QIREGAPP=\"");
                        strcat((char *)cmd, Concentrator.GprsParam.Apn);
                        strcat((char *)cmd, "\",\"");
                        strcat((char *)cmd, Concentrator.GprsParam.Username);
                        strcat((char *)cmd, "\",\"");
                        strcat((char *)cmd, Concentrator.GprsParam.Password);
                        strcat((char *)cmd, "\"");
                    } else {
                        strcpy((char *)cmd, "AT+QIREGAPP");
                    }
                    ret = Gprs_ConnectProc((char *)cmd, 0, "OK", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 3, FALSE);
                    if ((void *)0 == ret) {
                        Gprs_OutputDebugMsg(FALSE, "\n   ���ý������Ϣʱʧ��\n");
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsRestart;
                        break;
                    }
                    // �����ƶ�����,����Gprs��������
                    ret = Gprs_ConnectProc("AT+QIACT", 0, "OK", (void *)0, TIME_DELAY_MS(6000), TIME_DELAY_MS(1000), 2, FALSE);
                    if ((void *)0 == ret) {
                        Gprs_OutputDebugMsg(FALSE, "\n   ����Gprs��������ʧ��\n");
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsRestart;
                        break;
                    }
                    // ��ȡ����IP��ַ
                    msg = Gprs_ConnectProc("AT+QILOCIP", 0, "###", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(1000), 3, TRUE);
                    if ((void *)0 == msg) {
                        Gprs_OutputDebugMsg(FALSE, "\n   ��ȡ����IP��ַʧ��\n");
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsClosePdp;
                        break;
                    }
                    bufPtr = (PORT_BUF_FORMAT *)msg;
                    bufPtr->Buffer[bufPtr->Length] = 0;
                    Gprs_IpFormatConvert(bufPtr->Buffer, Gprs.LocalIp, 0);
                    OSMemPut(LargeMemoryPtr, msg);
                } else {
                    // ���ý����Apn,Username��Password
                    if (strlen(Concentrator.GprsParam.Apn) > 0) {
                        strcpy((char *)cmd, "AT+QICSGP=1,1,\"");
                        strcat((char *)cmd, Concentrator.GprsParam.Apn);
                        strcat((char *)cmd, "\",\"");
                        strcat((char *)cmd, Concentrator.GprsParam.Username);
                        strcat((char *)cmd, "\",\"");
                        strcat((char *)cmd, Concentrator.GprsParam.Password);
                        strcat((char *)cmd, "\",1");
                    } else {
                        strcpy((char *)cmd, "AT+QICSGP=1,1,\"\",\"\",\"\",1");
                    }
                    ret = Gprs_ConnectProc((char *)cmd, 0, "OK", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 3, FALSE);
                    if ((void *)0 == ret) {
                        Gprs_OutputDebugMsg(FALSE, "\n   ���ý������Ϣʱʧ��\n");
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsRestart;
                        break;
                    }
                    // �����ƶ�����,����Gprs��������
                    for (loop = 0; loop < 5; loop++) {
                        msg = Gprs_ConnectProc("AT+QIACT?", 0, "+QIACT: ", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 3, TRUE);
                        if ((void *)0 != msg) {
                            bufPtr = (PORT_BUF_FORMAT *)msg;
                            for (i = bufPtr->Length - 1; i > 0; i--) {
                                if (bufPtr->Buffer[i] == 0x22 && bufPtr->Buffer[i - 1] == 0x2C) {
                                    Gprs_IpFormatConvert(&bufPtr->Buffer[i], Gprs.LocalIp, 0);
                                    break;
                                }
                                if (bufPtr->Buffer[i] == 0x22) {
                                    bufPtr->Buffer[i] = 0;
                                }
                            }
                            OSMemPut(LargeMemoryPtr, msg);
                            if (i > 0) {
                                break;
                            }
                        }
                        Gprs_ConnectProc("AT+QIDEACT=1", 0, "OK", (void *)0, TIME_DELAY_MS(2000), TIME_DELAY_MS(200), 3, FALSE);
                        OSTimeDlyHMSM(0, 0, 1, 0);
                        Gprs_ConnectProc("AT+QIACT=1", 0, "OK", (void *)0, TIME_DELAY_MS(50000), TIME_DELAY_MS(200), 1, FALSE);
                    }
                    if (loop >= 5) {
                        Gprs_OutputDebugMsg(FALSE, "\n   ����Gprs��������ʧ��\n");
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsRestart;
                        break;
                    }
                }
                if (strlen(Concentrator.GprsParam.Apn) > 0) {
                    strcpy((char *)cmd, "�ɹ�\n   �������Ϣ APN:\"");
                    strcat((char *)cmd, Concentrator.GprsParam.Apn);
                    strcat((char *)cmd, "\", �û���:\"");
                    strcat((char *)cmd, Concentrator.GprsParam.Username);
                    strcat((char *)cmd, "\", ����:\"");
                    strcat((char *)cmd, Concentrator.GprsParam.Password);
                    strcat((char *)cmd, "\"\n");
                    Gprs_OutputDebugMsg(FALSE, cmd);
                } else {
                    Gprs_OutputDebugMsg(FALSE, "�ɹ�\n   δ���ý������Ϣ!\n");
                }
                OSMemPut(SmallMemoryPtr, cmd);
                Gprs.Cmd= GprsOpenTcp;
                break;

            case GprsOpenTcp:
                // �ı�ָʾ����˸״̬-δע�ᵽ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay200ms, FALSE);
                Gprs_OutputDebugMsg(FALSE, "��������������...");

                // �����ڴ�ռ�,�������ʧ��,��ÿ��2����������һ��
                if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
                    Gprs_OutputDebugMsg(FALSE, "\n   �����ڴ�ռ�ʱʧ��\n");
                    OSTimeDlyHMSM(0, 0, 2, 0);
                    break;
                }
                // ����Tcp����
                if (Gprs.ConnectCount < GPRS_BACKUP_IP_CONNECT) {
                    Gprs.ConnectCount += 1;
                    dscIpPtr = Concentrator.GprsParam.PriorDscIp;
                    dscPort = Concentrator.GprsParam.PriorDscPort;
                } else {
                    Gprs.ConnectCount = 0;
                    dscIpPtr = Concentrator.GprsParam.BackupDscIp;
                    dscPort = Concentrator.GprsParam.BackupDscPort;
                }
                if (Quectel_M35 == Gprs.ModuleType) {
                    strcpy((char *)cmd, "AT+QIOPEN=\"TCP\",\"");
                    Gprs_IpFormatConvert(&cmd[strlen((char *)cmd)], dscIpPtr, 1);
                    strcat((char *)cmd, "\",\"");
                    Uint16ToString(dscPort, &cmd[strlen((char *)cmd)]);
                    strcat((char *)cmd, "\"");
                    ret = Gprs_ConnectProc((char *)cmd, 0, "CONNECT OK", "ALREADY CONNECT", TIME_DELAY_MS(5000), TIME_DELAY_MS(3500), 5, FALSE);
                } else {
                    strcpy((char *)cmd, "AT+QIOPEN=1,0,\"TCP\",\"");
                    Gprs_IpFormatConvert(&cmd[strlen((char *)cmd)], dscIpPtr, 1);
                    strcat((char *)cmd, "\",");
                    Uint16ToString(dscPort, &cmd[strlen((char *)cmd)]);
                    strcat((char *)cmd, ",0,1");
                    ret = Gprs_ConnectProc((char *)cmd, 0, "+QIOPEN: 0,0", "+QIOPEN: 0,563", TIME_DELAY_MS(10000), TIME_DELAY_MS(3500), 5, FALSE);
                }
                strcpy((char *)cmd, "   ��������Ϣ IP:");
                Gprs_IpFormatConvert(&cmd[strlen((char *)cmd)], dscIpPtr, 1);
                strcat((char *)cmd, ", Port:");
                Uint16ToString(dscPort, &cmd[strlen((char *)cmd)]);
                strcat((char *)cmd, "\n");
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "ʧ��\n");
                    Gprs_OutputDebugMsg(FALSE, cmd);
                    OSMemPut(SmallMemoryPtr, cmd);
                    Gprs.Cmd = GprsClosePdp;
                    break;
                }
                Gprs.ClosePDPCount = 0;
                Gprs.ConnectCount = 0;
                Gprs_OutputDebugMsg(FALSE, "�ɹ�\n");
                Gprs_OutputDebugMsg(FALSE, cmd);
                OSMemPut(SmallMemoryPtr, cmd);
                Gprs_OutputDebugMsg(FALSE, "-------------------------------------------------\n");
                // ��������������Ƿ��Ѿ�������
                Gprs.HeartbeatRetryTime = 0;
				if( GPRS_SOFT_UP_EN == GprsSoftUp ){
                	Gprs.Cmd = GprsCheckConnect;
				}else{
					Gprs.Cmd = GprsSignIn;
                }break;

			case GprsClosePdp:
                Gprs.Online = FALSE;
                // �ı�ָʾ����˸״̬-δ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay1000ms, TRUE);
                // �ر�PDP����
                Gprs_OutputDebugMsg(FALSE, "�ر�PDP����...");
                if (Quectel_M35 == Gprs.ModuleType) {
                    ret = Gprs_ConnectProc("AT+QIDEACT", 0, "DEACT OK", (void *)0, TIME_DELAY_MS(60000), TIME_DELAY_MS(500), 1, FALSE);
                } else {
                    ret = Gprs_ConnectProc("AT+QIDEACT=1", 0, "OK", (void *)0, TIME_DELAY_MS(40000), TIME_DELAY_MS(500), 1, FALSE);
                }
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(FALSE, "ʧ��\n");
                    Gprs.Cmd = GprsRestart;
                    break;
                }
                // �����������5��������ģ��
                Gprs_OutputDebugMsg(FALSE, "�ɹ�\n");
                if (Gprs.ClosePDPCount < GPRS_CONTINUECLOSEPDPTIME) {
                    Gprs.ClosePDPCount++;
                    Gprs.Cmd = GprsConfig;
                } else {
                    Gprs.Cmd = GprsRestart;
                }
                break;

			case GprsCloseTcp:
                Gprs.Online = FALSE;
                // �ı�ָʾ����˸״̬-δ����
                Led_FlashTime(GPRS_LED, Delay50ms, Delay1000ms, TRUE);
                // �ر�TCP����
                Gprs_OutputDebugMsg(FALSE, "�ر�TCP����...");
                if (Quectel_M35 == Gprs.ModuleType) {
                    ret = Gprs_ConnectProc("AT+QISTATE", 0, "IP CLOSE", "PDP DEACT", TIME_DELAY_MS(1000), TIME_DELAY_MS(500), 1, FALSE);
                    if ((void *)0 == ret) {
                        ret = Gprs_ConnectProc("AT+QICLOSE", 0, "CLOSE OK", (void *)0, TIME_DELAY_MS(30000), TIME_DELAY_MS(500), 1, FALSE);
                        if ((void *)0 == ret) {
                            Gprs_OutputDebugMsg(FALSE, "ʧ��\n");
                            Gprs.Cmd = GprsRestart;
                            break;
                        }
                    }
                } else {
                    ret = Gprs_ConnectProc("AT+QISTATE=1,0", 0, "+QISTATE:", (void *)0, TIME_DELAY_MS(1500), TIME_DELAY_MS(500), 1, FALSE);
                    if ((void *)0 != ret) {
                        ret = Gprs_ConnectProc("AT+QICLOSE=0", 0, "OK", (void *)0, TIME_DELAY_MS(1500), TIME_DELAY_MS(500), 1, FALSE);
                        if ((void *)0 == ret) {
                            Gprs_OutputDebugMsg(FALSE, "ʧ��\n");
                            Gprs.Cmd = GprsRestart;
                            break;
                        }
                    }
                }
                Gprs_OutputDebugMsg(FALSE, "�ɹ�\n");
                Gprs.Cmd = GprsOpenTcp;
                break;

			case GprsSignIn:
				// �ı�ָʾ����˸״̬-����ͨѶ
				Led_FlashTime(GPRS_LED, AlwaysOn, Delay50ms, TRUE);

				// �����ڴ�ռ�,�������ʧ��,��ÿ��1����������һ��
				if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
					OSTimeDlyHMSM(0, 0, 1, 0);
					break;
				}

				len = Gprs_SignInPkgHandle(cmd); // ��¼֡
				if (Quectel_M35 == Gprs.ModuleType) {
					strcpy((char *)cmd, "AT+QISEND=");
				} else {
					strcpy((char *)cmd, "AT+QISEND=0,");
				}
				Uint16ToString(len, &cmd[strlen((char *)cmd)]);
				Gprs_ConnectProc((char *)cmd, 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
				bufPtr = (PORT_BUF_FORMAT *)cmd;
				bufPtr->Length = Gprs_SignInPkgHandle(bufPtr->Buffer); // ��¼֡
				Gprs_OutputDebugMsg(TRUE, ">>>��¼��");
				ret = Gprs_ConnectProc((char *)(bufPtr->Buffer), bufPtr->Length, "SEND OK", (void *)0, TIME_DELAY_MS(5000), TIME_DELAY_MS(20), 1, FALSE);
				if ((void *)0 == ret) {
					OSMemPut(SmallMemoryPtr, cmd);
					Gprs.Cmd = GprsCloseTcp;
					Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
					if( ++GprsSignInNum > 10 ){
						GprsSignInNum = 0;
						Gprs.Cmd = GprsRestart;
					}
					break;
				}

				OSMemPut(SmallMemoryPtr, cmd);
				Gprs_OutputDebugMsg(FALSE, "-�ɹ�<<<\n");

				Gprs.Cmd = GprsConnectIdleStatus;
				Gprs.HeartbeatInterval = 10;
				RTCTimingTimer = 15;
				GetRtcOK = 0;
				break;

			//	�������ȡʱ��
            case GprsUpdataRtc:
                if( AtQntpErr > 5 ){
                    eventFlag &= ~FLAG_GPRS_UPDATA_RTC;
                    RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
                    Gprs.Cmd = GprsConnectIdleStatus;
                    break;
                }

                // �й�Ĭ�Ϲ���ʱ���������ַΪ"210.72.145.44" ���˿ں��� 123
                // �Ϻ���ͨ��ѧУʱ������ "202.120.2.101",123
                // windows�ϲ鿴����Уʱ������ "time.nist.gov",13
                // AT+QNTP ָ���������64s���ϣ���������ʧ�ܣ�
                if (Quectel_M35 == Gprs.ModuleType) {
                    ret = Gprs_ConnectProc("AT+QNTP=\"time.nist.gov\",13", 0, "OK", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                }else{
                    ret = Gprs_ConnectProc("AT+QNTP=1,\"time.nist.gov\",13", 0, "OK", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                }
                if ((void *)0 == ret) {
                    Gprs_OutputDebugMsg(TRUE, "\n -- RTC AT+QNTP error --\n");
                    eventFlag &= ~FLAG_GPRS_UPDATA_RTC;
                    AtQntpErr++;
                    RTCTimingTimer = 100; 			// ���Уʱʧ����100s������
                    Gprs.Cmd = GprsConnectIdleStatus;
                    break;
                }
                msg = Gprs_ConnectProc("AT+CCLK?", 0, "+CCLK:", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, TRUE);
                if ((void *)0 != msg) {
                    bufPtr = (PORT_BUF_FORMAT *)msg;
                    for( i = 0; i < bufPtr->Length; i++ ){
                        if( (bufPtr->Buffer[i] == 0x22) && (bufPtr->Buffer[i+3] == 0x2f) && (bufPtr->Buffer[i+6] == 0x2f)){
                            len = i+1;
                            RtcTime.Year = 2000 + (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                            len+=3;
                            RtcTime.Month = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                            len+=3;
                            RtcTime.Day = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                            len+=3;
                            RtcTime.Hour = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                            len+=3;
                            RtcTime.Minute = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                            len+=3;
                            RtcTime.Second = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                            len+=3;
                            if(0x2B == bufPtr->Buffer[len-1]){//��ʱ������Ҫ������ʱ�� + ʱ��/4
                                addsub = 1;
                                if (Quectel_M35 == Gprs.ModuleType) {
                                    timezone = 32;// M35������Ϣ��ʱ����ϢΪ0���ֶ������й�ʱ������Ҫ������ʱ�� + ʱ��/4
                                }else{
                                    timezone = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                                }
                            }else if(0x2D == bufPtr->Buffer[len-1]){//��ʱ��
                                addsub = 0;
                                if (Quectel_M35 == Gprs.ModuleType) {
                                    timezone = 32;// M35������Ϣ��ʱ����ϢΪ0���ֶ������й�ʱ������Ҫ������ʱ�� + ʱ��/4
                                }else{
                                    timezone = (BcdToBin(bufPtr->Buffer[len])-30)*10 + (BcdToBin(bufPtr->Buffer[len+1])-30);
                                }
                            }else{
                                addsub = 1;
                                timezone = 32;
                            }
                        }
                    }

                    RtcErr = 0;
                    if (RtcTime.Year < 2017 || RtcTime.Year > 2027 ||
                        RtcTime.Month == 0 || RtcTime.Month > 12 ||
                        RtcTime.Day == 0 || RtcTime.Day > 31 ||
                        RtcTime.Hour >= 24 || RtcTime.Minute >= 60 || RtcTime.Second >= 60) {
                        RtcErr = 1;
                    }

                    if ((SUCCESS == Gprs_Rtc_Set(RtcTime, Format_Bin, timezone, addsub)) && (0 == RtcErr)) {
                        Gprs_OutputDebugMsg(TRUE, "\n -- RTC Уʱ�ɹ� --\n");
                        RTCTimingTimer = RTCTIMING_INTERVAL_TIME;
                        AtQntpErr = 0;
                        GetRtcOK = 1;
                    } else {
                        Gprs_OutputDebugMsg(TRUE, "\n -- RTC Уʱʧ�� --\n");
                        RTCTimingTimer = 100; 			// ���Уʱʧ����100s������
                        AtQntpErr++;
                    }

                    OSMemPut(LargeMemoryPtr, msg);
                    eventFlag &= ~FLAG_GPRS_UPDATA_RTC;
                    Gprs.Cmd = (eventFlag & FLAG_UART_GPRS_RX) ? GprsReceiveData : GprsConnectIdleStatus;
                    break;
                }

                Gprs_OutputDebugMsg(TRUE, "\n ++ RTC Уʱʧ�� ++\n");
				RTCTimingTimer = 100; 			// ���Уʱʧ����100s������
                AtQntpErr++;
                eventFlag &= ~FLAG_GPRS_UPDATA_RTC;
                Gprs.Cmd = (eventFlag & FLAG_UART_GPRS_RX) ? GprsReceiveData : GprsConnectIdleStatus;

                break;

            case GprsCheckConnect:
                if( GPRS_SOFT_UP_EN == GprsSoftUp ){
                    // �����ڴ�ռ�,�������ʧ��,��ÿ��1����������һ��
                    if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
                        OSTimeDlyHMSM(0, 0, 1, 0);
                        break;
                    }
                    // ��������������
                    if (Quectel_M35 == Gprs.ModuleType) {
                        Gprs_ConnectProc("AT+QISEND=22", 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                    } else {
                        Gprs_ConnectProc("AT+QISEND=0,22", 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                    }
                    Gprs_OutputDebugMsg(TRUE, ">>>������");
                    eventFlag &= ~FLAG_HEATBEAT_TIMER;
                    bufPtr = (PORT_BUF_FORMAT *)cmd;
                    bufPtr->Length = 0;
                    bufPtr->Property.PortNo = Uart_Gprs;
                    bufPtr->Buffer[bufPtr->Length++] = Gprs.LocalIp[0];
                    bufPtr->Buffer[bufPtr->Length++] = Gprs.LocalIp[1];
                    bufPtr->Buffer[bufPtr->Length++] = Gprs.LocalIp[2];
                    bufPtr->Buffer[bufPtr->Length++] = Gprs.LocalIp[3];
                    bufPtr->Buffer[bufPtr->Length++] = (uint8)(GPRS_LOCAL_PORT >> 8);
                    bufPtr->Buffer[bufPtr->Length++] = (uint8)GPRS_LOCAL_PORT;
                    Gprs_TxPkgHandle(GPRS_HEATBEAT_CMD, cmd);
                    ret = Gprs_ConnectProc((char *)(bufPtr->Buffer), bufPtr->Length, "SEND OK", (void *)0, TIME_DELAY_MS(5000), TIME_DELAY_MS(500), 1, FALSE);
                    if ((void *)0 == ret) {
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsCloseTcp;
                        Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
                        break;
                    }
                    // �ȴ���������������Ӧ������
                    for (loop = 0; loop < 5; loop++) {
                        msg = OSMboxPend(SerialPort.Port[Uart_Gprs].MboxRx, TIME_DELAY_MS(5000), &err);
                        if ((void *)0 == msg) {
                            if (++Gprs.HeartbeatRetryTime > 3) {
                                Gprs.Cmd = GprsCloseTcp;
                            }
                            Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
                            break;
                        }
                        bufPtr = (PORT_BUF_FORMAT *)msg;
                        for (i = 0; i < LONG_ADDR_SIZE; i++) {
                            BcdToAscii(&Concentrator.LongAddr[i], &cmd[i * 2], 1, 2);
                        }
                        if (bufPtr->Length > LONG_ADDR_SIZE * 2) {
                            for (i = 0; i < bufPtr->Length - LONG_ADDR_SIZE * 2 + 1; i++) {
                                if (0 == memcmp(&cmd[1], &bufPtr->Buffer[i], LONG_ADDR_SIZE * 2 - 1)) {
                                    Gprs.HeartbeatRetryTime = 0;
                                    Gprs.Cmd = GprsConnectIdleStatus;
                                    Gprs.HeartbeatInterval = Concentrator.GprsParam.HeatBeatPeriod * 10;
                                    if (FALSE == Gprs.Online) {
                                        Gprs.Online = TRUE;
                                    }
                                    loop = 200;
                                    Gprs_OutputDebugMsg(FALSE, "-�ɹ�<<<\n");
                                    break;
                                }
                            }
                        }
                        OSMemPut(LargeMemoryPtr, msg);
                    }
                    OSMemPut(SmallMemoryPtr, cmd);
                    if (loop == 5) {
                        Gprs.Cmd = GprsCloseTcp;
                        Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
                    }
                    break;
                }else{
                    // �����ڴ�ռ�,�������ʧ��,��ÿ��1����������һ��
                    if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
                        OSTimeDlyHMSM(0, 0, 1, 0);
                        break;
                    }
                    // ��������������
                    if (Quectel_M35 == Gprs.ModuleType) {
                        Gprs_ConnectProc("AT+QISEND=1", 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                    } else {
                        Gprs_ConnectProc("AT+QISEND=0,1", 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                    }
                    Gprs_OutputDebugMsg(TRUE, ">>>������");
                    eventFlag &= ~FLAG_HEATBEAT_TIMER;
                    bufPtr = (PORT_BUF_FORMAT *)cmd;
                    bufPtr->Length = 0;
                    bufPtr->Property.PortNo = Uart_Gprs;
                    bufPtr->Buffer[bufPtr->Length++] = 0x24;// ����Ϊһ��Ӣ�ĵ���Ԫ�ַ� "$" = 0x24
                    ret = Gprs_ConnectProc((char *)(bufPtr->Buffer), bufPtr->Length, "SEND OK", (void *)0, TIME_DELAY_MS(5000), TIME_DELAY_MS(500), 1, FALSE);
                    if ((void *)0 == ret) {
                        OSMemPut(SmallMemoryPtr, cmd);
                        Gprs.Cmd = GprsCloseTcp;
                        Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
                        break;
                    }

                    // �ȴ���������������Ӧ������
                    for (loop = 0; loop < 5; loop++) {
                        msg = OSMboxPend(SerialPort.Port[Uart_Gprs].MboxRx, TIME_DELAY_MS(5000), &err);
                        if ((void *)0 == msg) {
                            if (++Gprs.HeartbeatRetryTime > 3) {
                                Gprs.Cmd = GprsCloseTcp;
                            }
                            Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
                            break;
                        }
                        bufPtr = (PORT_BUF_FORMAT *)msg;
                        if (0x25 == bufPtr->Buffer[bufPtr->Length-3] || 0x25 == bufPtr->Buffer[0]) {// �����ɹ�����һ��Ӣ�İٷֺ��ַ� "%" = 0x25
                            Gprs.HeartbeatRetryTime = 0;
                            Gprs.Cmd = GprsConnectIdleStatus;
                            Gprs.HeartbeatInterval = Concentrator.GprsParam.HeatBeatPeriod * 10;
                            if (FALSE == Gprs.Online) {
                                Gprs.Online = TRUE;
                            }
                            loop = 200;
                            Gprs_OutputDebugMsg(FALSE, "-�ɹ�<<<\n");
                            OSMemPut(LargeMemoryPtr, msg);
                            break;
                        }
                        OSMemPut(LargeMemoryPtr, msg);
                    }
                    OSMemPut(SmallMemoryPtr, cmd);
                    if (loop == 5) {
                        Gprs.Cmd = GprsCloseTcp;
                        Gprs_OutputDebugMsg(FALSE, "-ʧ��<<<\n");
                    }
                    break;

                }

            case GprsLogoffConnect:
                // �����ڴ�ռ�,�������ʧ��,��ÿ��1����������һ��
                if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
                    OSTimeDlyHMSM(0, 0, 1, 0);
                    break;
                }
                // �ı�ָʾ����˸״̬-ע��
                Led_FlashTime(GPRS_LED, Delay1000ms, AlwaysOff, FALSE);
                // ����ע��ָ��
                if (Quectel_M35 == Gprs.ModuleType) {
                    Gprs_ConnectProc("AT+QISEND=16", 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                } else {
                    Gprs_ConnectProc("AT+QISEND=0,16", 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                }
                Gprs_OutputDebugMsg(TRUE, ">>>�豸����,����ע��<<<\n");
                bufPtr = (PORT_BUF_FORMAT *)cmd;
                bufPtr->Length = 0;
                bufPtr->Property.PortNo = Uart_Gprs;
                Gprs_TxPkgHandle(GPRS_LOGOFF_CMD, cmd);
                Gprs_ConnectProc((char *)(bufPtr->Buffer), bufPtr->Length, "SEND OK", (void *)0, TIME_DELAY_MS(5000), TIME_DELAY_MS(500), 1, FALSE);
                OSMemPut(SmallMemoryPtr, cmd);
                Gprs.Cmd = GprsPowerOff;
                break;

            case GprsConnectIdleStatus:
                if( GPRS_SOFT_UP_EN == GprsSoftUp ){
                    // �ı�ָʾ����˸״̬-�Ѿ�����
                    Led_FlashTime(GPRS_LED, AlwaysOn, Delay1000ms, FALSE);
                    eventFlag = OSFlagPend(GlobalEventFlag, (OS_FLAGS)GPRS_FLAG_FILTER, (OS_FLAG_WAIT_SET_ANY | OS_FLAG_CONSUME), TIME_DELAY_MS(60000), &err);
                    if (eventFlag & FLAG_UART_GPRS_RX) {
                            Gprs.Cmd = GprsReceiveData;
                    } else if (eventFlag & FLAG_GPRS_TX) {
                            Gprs.Cmd = GprsTransmitData;
                    } else if (eventFlag & FLAG_HEATBEAT_TIMER) {
                            Gprs.Cmd = GprsCheckConnect;
                    } else if (eventFlag & FLAG_LOGOFF_EVENT) {
                            Gprs.Cmd = GprsLogoffConnect;
                    }
                    break;
                }else{
                    // �ı�ָʾ����˸״̬-�Ѿ�����
                    Led_FlashTime(GPRS_LED, AlwaysOn, Delay1000ms, FALSE);
                    eventFlag = OSFlagPend(GlobalEventFlag, (OS_FLAGS)GPRS_FLAG_FILTER, (OS_FLAG_WAIT_SET_ANY | OS_FLAG_CONSUME), TIME_DELAY_MS(60000), &err);
                    if (eventFlag & FLAG_GPRS_UPDATA_RTC) {
                            Gprs.Cmd = GprsUpdataRtc;
                    } else if (eventFlag & FLAG_UART_GPRS_RX) {
                            Gprs.Cmd = GprsReceiveData;
                    } else if (eventFlag & FLAG_GPRS_TX) {
                            Gprs.Cmd = GprsTransmitData;
                    } else if (eventFlag & FLAG_HEATBEAT_TIMER) {
                            Gprs.Cmd = GprsCheckConnect;
                    } else if (eventFlag & FLAG_LOGOFF_EVENT) {
                            Gprs.Cmd = GprsLogoffConnect;
                    } else if (eventFlag & FLAG_GPRS_RESIGN_IN) {
                            Gprs.Cmd = GprsCloseTcp;
                    }
                    break;
                }
            case GprsReceiveData:
                    // �ı�ָʾ����˸״̬-����ͨѶ
                    Led_FlashTime(GPRS_LED, AlwaysOn, Delay50ms, TRUE);
                    msg = OSMboxAccept(SerialPort.Port[Uart_Gprs].MboxRx);
                    if ((void *)0 != msg) {
                            // �Խ������ݵĴ���
                            bufPtr = (PORT_BUF_FORMAT *)msg;
                            if (FALSE == bufPtr->Property.FilterDone && bufPtr->Length > strlen(GPRS_IP_CLOSE)) {
                                    for (loop = 0; loop < bufPtr->Length - strlen(GPRS_IP_CLOSE); loop++) {
                                            if (0 == memcmp((char *)(&bufPtr->Buffer[loop]), GPRS_IP_CLOSE, strlen(GPRS_IP_CLOSE))) {
                                                    Gprs.Cmd = GprsCloseTcp;
                                                    break;
                                            }
                                    }
                            }
                            if (GprsCloseTcp == Gprs.Cmd) {
                                    OSMemPut(LargeMemoryPtr, msg);
                                    break;
                            }
                            // �����ݴ�������������
                            Gprs.Online = TRUE;
                            Gprs.HeartbeatInterval = Concentrator.GprsParam.HeatBeatPeriod * 10;
                            loop = 3;
                            while (loop && OS_ERR_NONE != OSMboxPost(Gprs.MboxRx, msg)) {
                                    loop -= 1;
                                    OSTimeDlyHMSM(0, 0, 0, 100);
                            }
                            if (0 == loop) {
                                    OSMemPut(LargeMemoryPtr, msg);
                            } else {
                                    OSFlagPost(GlobalEventFlag, FLAG_GPRS_RX, OS_FLAG_SET, &err);
                            }
                    }
                    eventFlag &= ~FLAG_UART_GPRS_RX;
                    Gprs.Cmd = (eventFlag & FLAG_GPRS_TX) ? GprsTransmitData : GprsConnectIdleStatus;
                    break;

            case GprsTransmitData:
                    // �ı�ָʾ����˸״̬-����ͨѶ
                    Led_FlashTime(GPRS_LED, AlwaysOn, Delay50ms, TRUE);
                    // �����ڴ�ռ�,�������ʧ��,��ÿ��500������������һ��
                    if ((void *)0 == (cmd = OSMemGet(SmallMemoryPtr, &err))) {
                            OSTimeDlyHMSM(0, 0, 0, 500);
                            break;
                    }
                    msg = OSMboxAccept(Gprs.MboxTx);
                    if ((void *)0 != msg) {
                            if (TRUE== Gprs_TxPkgHandle(GPRS_DATAPKG_CMD, msg)) {
                                    bufPtr = (PORT_BUF_FORMAT *)msg;
                                    if (Quectel_M35 == Gprs.ModuleType) {
                                            strcpy((char *)cmd, "AT+QISEND=");
                                    } else {
                                            strcpy((char *)cmd, "AT+QISEND=0,");
                                    }
                                    Uint16ToString(bufPtr->Length, &cmd[strlen((char *)cmd)]);
                                    Gprs_ConnectProc((char *)cmd, 0, ">", (void *)0, TIME_DELAY_MS(3000), TIME_DELAY_MS(500), 1, FALSE);
                                    ret = Gprs_ConnectProc((char *)(bufPtr->Buffer), bufPtr->Length, "SEND OK", (void *)0, TIME_DELAY_MS(5000), TIME_DELAY_MS(20), 1, FALSE);
                                    Gprs.HeartbeatInterval = 20;
                                    OSMemPut(LargeMemoryPtr, msg);
                                    if ((void *)0 == ret) {
                                            OSMemPut(SmallMemoryPtr, cmd);
                                            eventFlag &= ~FLAG_GPRS_TX;
                                            Gprs.Cmd = (eventFlag & FLAG_UART_GPRS_RX) ? GprsReceiveData : GprsCloseTcp;
                                            break;
                                    }
                            }
                    }
                    OSMemPut(SmallMemoryPtr, cmd);
                    eventFlag &= ~FLAG_GPRS_TX;
                    Gprs.Cmd = (eventFlag & FLAG_UART_GPRS_RX) ? GprsReceiveData : GprsConnectIdleStatus;
                    break;

            default:
                    break;
        }
        OSTimeDlyHMSM(0, 0, 0, 5);
    };
}

/***************************************End of file*********************************************/

