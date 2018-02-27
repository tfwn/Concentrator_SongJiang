/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Common.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        07/16/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define COMMON_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "ucos_ii.h"
#include "Main.h"
#include "SerialPort.h"
#include "DataHandle.h"
#include <string.h>

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/
uint8 BroadcastAddrIn[LONG_ADDR_SIZE] = {0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5};
uint8 BroadcastAddrOut[LONG_ADDR_SIZE] = {0xD4, 0xD4, 0xD4, 0xD4, 0xD4, 0xD4};
uint8 NullAddress[LONG_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Uint16ToString
* Decription   : ��16λ���ȵ���ת�����ַ���,��0x1234ת����"4660"
* Input        : Src-16λ���ȵ���,DstPtr-ָ��ת����Ļ�����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Uint16ToString(uint16 Src, uint8 *DstPtr)
{
    uint8 i, result, flag;
    uint16 div = 10000;

    flag = 0;
    for (i = 0; i < 4; i++) {
        result = (uint8)(Src / div);
        if (result > 0 || 1 == flag) {
            *DstPtr++ = result + '0';
            flag = 1;
        }
        Src %= div;
        div /= 10;
    }
    *DstPtr++ = Src + '0';
    *DstPtr = 0;
}

/************************************************************************************************
* Function Name: Uint8ToString
* Decription   : ��8λ���ȵ���ת�����ַ���,��0x34ת����"52"
* Input        : Src-8λ���ȵ���,DstPtr-ָ��ת����Ļ�����ָ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Uint8ToString(uint8 Src, uint8 *DstPtr)
{
    uint8 i, result, flag;
    uint16 div = 100;

    flag = 0;
    for (i = 0; i < 2; i++) {
        result = Src / div;
        if (result > 0 || 1 == flag) {
            *DstPtr++ = result + '0';
            flag = 1;
        }
        Src %= div;
        div /= 10;
    }
    *DstPtr++ = Src + '0';
    *DstPtr = 0;
}

/************************************************************************************************
* Function Name: StringToByte
* Decription   : ���ַ���ת��������,��"1234"ת����0x1234
* Input        : SrcPtr-ָ���ת���ַ�����ָ��,DstPtr-ָ��ת����Ļ�����ָ��,SrcLen-��ת�����ݳ���
* Output       : ��
* Others       : ��
************************************************************************************************/
void StringToByte(uint8 *SrcPtr, uint8 *DstPtr, uint8 SrcLen)
{
    uint8 a, b;

    SrcLen /= 2;
    while (SrcLen--) {
        a = *SrcPtr++ - '0';
        b = *SrcPtr++ - '0';
        *DstPtr++ = a << 4 & 0xF0 | b;
    }
}

/************************************************************************************************
* Function Name: BinToBcd
* Decription   : ��Binת����BCD����
* Input        : Val-��ת���ֽ�
* Output       : ת����BCD��ʽ��ֵ
* Others       : ��
************************************************************************************************/
uint8 BinToBcd(uint8 Val)
{
    uint8 bcd = Val % 10;

    bcd = bcd | Val / 10 << 4;
    return bcd;
}

/************************************************************************************************
* Function Name: BcdToBin
* Decription   : BCD��ʽת����Bin
* Input        : Val-��ת���ֽ�
* Output       : ת�����ֵ
* Others       : ��
************************************************************************************************/
uint8 BcdToBin(uint8 Val)
{
    uint8 hex;

    hex = (Val >> 4 & 0x0F) * 10 + (Val & 0x0F);
    return hex;
}

/************************************************************************************************
* Function Name: BcdToAscii
* Decription   : BCD��ʽת�����ֽ�
* Input        : SrcPtr-��ת�����ݵ�ָ��,DstPtr-����ת���������ָ��,SrcLength-���ݳ���
* Output       : ת����ĳ���,Ϊת��֮ǰ���ȵ� LenMul ��
* Others       : ��
************************************************************************************************/
uint16 BcdToAscii(uint8 *SrcPtr, uint8 *DstPtr, uint8 SrcLength, uint8 LenMul)
{
    uint8 i, msb, lsb;

    for (i = 0; i < SrcLength; i++) {
        msb = *SrcPtr >> 4 & 0x0F;
        lsb = *SrcPtr & 0x0F;
        *DstPtr++ = (msb <= 9) ? (msb + '0') : (msb + 'A' - 10);
        *DstPtr++ = (lsb <= 9) ? (lsb + '0') : (lsb + 'A' - 10);
		if(LenMul == 3){
			*DstPtr++ = ' ';
		}
        SrcPtr++;
    }
    return (SrcLength * LenMul);
}

/************************************************************************************************
* Function Name: CalCrc8
* Decription   : Crc8У����㺯��
* Input        : DataBuf-ָ�����ݵ�ָ��,DataLen-ҪУ������ݳ���
* Output       : ����У��ֵ
* Others       : ��
************************************************************************************************/
uint8 CalCrc8(const uint8 *DataBuf, uint16 DataLen)
{
    uint8 i, crc8=0;

    while (DataLen--) {
        crc8 ^= *DataBuf;
        for (i = 0; i < 8; i++) {
            if (crc8 & 0x01) {
                crc8 >>= 1;
                crc8 ^= 0x8C;
            } else {
                crc8 >>= 1;
            }
        }
        DataBuf++;
    }
    return crc8;
}

/************************************************************************************************
* Function Name: CalCrc16
* Decription   : CalCrc16У����㺯��
* Input        : DataBuf-ָ�����ݵ�ָ��,DataLen-ҪУ������ݳ���
* Output       : ����У��ֵ
* Others       : ��
************************************************************************************************/
uint16 CalCrc16(const uint8 *DataBuf, uint32 DataLen)
{
    #define CRC_POLY 0x8408
    uint16 crc16 = 0xFFFF;
    uint8 i;

    while (DataLen--) {
        crc16 ^= *DataBuf++;
        for (i = 0; i < 8; i++) {
            if (crc16 & 0x0001) {
                crc16 >>= 1;
                crc16 ^= CRC_POLY;
            } else {
                crc16 >>= 1;
            }
        }
    }
    crc16 ^= 0xFFFF;

    return crc16;
}

extern void Gprs_OutputDebugMsg(bool NeedTime, uint8 *StrPtr);
uint8 gprs_print(uint8 *DataFrmPtr, uint8 len)
{
	uint8 *AsciiBuf;

    if ((void *)0 == (AsciiBuf = OSMemGetOpt(LargeMemoryPtr, 20, TIME_DELAY_MS(50)))) {
        return -1;
    }
	memset(AsciiBuf, 0, MEM_LARGE_BLOCK_LEN);
	BcdToAscii(DataFrmPtr, AsciiBuf, len, 3);
	Gprs_OutputDebugMsg(FALSE, AsciiBuf);
    OSMemPut(LargeMemoryPtr, AsciiBuf);
	return 0;
}

/************************************************************************************************
* Function Name: CalCrc16 - 1021
* Decription   : CalCrc16У����㺯��
* Input        : DataBuf-ָ�����ݵ�ָ��,DataLen-ҪУ������ݳ���
* Output       : ����У��ֵ
* Others       : ��
************************************************************************************************/
uint16	phyCalCRC16(uint8*Ptr, uint8 Len)
{
	uint8 i;
	uint16  u16crc = 0;//�����ֽ�
	while(Len--!=0)
	{
		for(i=0x80; i!=0; i/=2)
		{
			if((u16crc  &  0x8000)!=0)
			{
				u16crc*=2;
				u16crc^=0x1021; //crc*=2  crc=crc*2�� crc  xor	0x1021
			} 		//��ʽu16crc����2����u16crc
			else
				u16crc*=2;	  //crc=crc*2
			if((*Ptr & i)!=0)
				u16crc^=0x1021;  //crc=crc xor 0x1021	//�ټ��ϱ�λ��u16crc
		}
		Ptr++;
	}
	return(u16crc);
};

/***************************************End of file*********************************************/

