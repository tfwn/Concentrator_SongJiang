/************************************************************************************************
*                                   SRWF-6009-BOOT
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Flash.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        10/08/2015      Zhangxp         SRWF-6009-BOOT  Original Version
************************************************************************************************/
#ifndef _FLASH_H_
#define _FLASH_H_


/************************************************************************************************
*                               Macro Define Section
************************************************************************************************/
#define FLASH_PAGE_SIZE                 ((uint16)0x800)     // FLASHÿһҳ�Ĵ�С2K

#define FLASH_APPCODE_START_ADDR        ((uint32)0x08004000)// APP�������еĿ�ʼλ��,96K

#define FLASH_PARAM_START_ADDRESS       ((uint32)0x0801C000)// FLASH���湤������������,2K
#define FLASH_PARAM_PAGE_SIZE           1

#define FLASH_NODE_START_ADDRESS        (FLASH_PARAM_START_ADDRESS + FLASH_PAGE_SIZE * FLASH_PARAM_PAGE_SIZE)    // FLASH���浵������������,16ҳ
#define FLASH_NODE_PAGE_SIZE            16                  // FLASH���浵������������,16ҳ,32K

#define FLASH_UPGRADECODE_START_ADDR	((uint32)0x08027000)// �����ļ���ŵĿ�ʼλ��
#define FLASH_UPGRADECODE_SIZE          48                  // �����ļ�,���Ϊ96K,ÿһҳΪ2K

#define FLASH_UPGRADE_INFO_START        ((uint32)0x0807F800)// �����ļ���������λ��,510K��ʼ,ռ��2K�ռ�
#define FLASH_UPGRADE_INFO_SIZE         1                   // �����ļ���������λ�ô�С


/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/

/************************************************************************************************
*                                   Struct Define Section
************************************************************************************************/

/************************************************************************************************
*                                   Function Declare Section
************************************************************************************************/
extern void Flash_Write(uint8 * Tp, uint16 Num, uint32 Flash_Addr);
extern void Flash_Erase(uint32 StartAddr, uint8 PageCount);


#endif
/**************************************End of file**********************************************/


