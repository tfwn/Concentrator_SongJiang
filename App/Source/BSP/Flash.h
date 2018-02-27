/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Flash.h
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.0        08/05/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#ifndef  FLASH_H
#define  FLASH_H

#ifdef   FLASH_GLOBALS
#define  FLASH_EXT
#else
#define  FLASH_EXT  extern
#endif

/************************************************************************************************
*                               Pubilc Macro Define Section
************************************************************************************************/
#define FLASH_PAGE_SIZE                         ((uint16)0x800)         // FLASHÿһҳ�Ĵ�С2K

#define FLASH_BOOT_SIZE                         ((uint16)0x4000)        // BOOT���볤��16K,��0K��ʼ,���ռ��16K�ռ�

#define FLASH_APPCODE_START_ADDR                ((uint32)0x08004000)    // APP�������еĿ�ʼλ��,16K��ʼ  0x08004000

#define FLASH_CONCENTRATOR_INFO_ADDRESS         ((uint32)0x0803E800)    // ����������������,��250K��ʼ,ռ�ݿռ�2K
#define FLASH_BACKUP_CONCENTRATOR_INFO_ADDRESS  ((uint32)0x0803F000)    // ������������������,��252K��ʼ��ռ�ݿռ�2K

#define FLASH_NODE_INFO_ADDRESS                 ((uint32)0x08040000)    // ��ڵ��������,��256K��ʼ,ռ��16K�ռ�
#define FLASH_NODE_INFO_PAGE_SIZE               8                       // FLASH����ڵ�·������,8ҳ,16K

#define FLASH_CUSTOM_ROUTE_ADDRESS1             ((uint32)0x0804B000)    // �Զ���·���洢��1,��300K��ʼ,ռ��22K�ռ�
#define FLASH_CUSTOM_ROUTE_ADDRESS2             ((uint32)0x08050800)    // �Զ���·���洢��2,��322K��ʼ,ռ��22K�ռ�
#define FLASH_CUSTOM_ROUTE_PAGE_SIZE            11                      // �Զ���·���洢��ռ�ݿռ�,22K

#define FLASH_UPGRADECODE_START_ADDR            ((uint32)0x08060000)    // �����ļ���ŵĿ�ʼλ��,384K��ʼ,���ռ��100K�ռ�
#define FLASH_UPGRADECODE_SIZE                  50                      // �����ļ�,���Ϊ100K,ÿһҳΪ2K

#define FLASH_UPGRADE_INFO_START                ((uint32)0x0807F800)    // �����ļ���������λ��,510K��ʼ,ռ��2K�ռ�
#define FLASH_UPGRADE_INFO_SIZE                 1                       // �����ļ���������λ�ô�С


#define FLASH_START_MEMORYID                    0x0218

/************************************************************************************************
*                                  Struct Define Section
************************************************************************************************/

/************************************************************************************************
*                                   Enum Define Section
************************************************************************************************/

/************************************************************************************************
*                            Variable Declare Section
************************************************************************************************/

/************************************************************************************************
*                            Function Declare Section
************************************************************************************************/
FLASH_EXT void Flash_LoadConcentratorInfo(void);
FLASH_EXT void Flash_SaveConcentratorInfo(void);
FLASH_EXT void Flash_LoadSubNodesInfo(void);
FLASH_EXT void Flash_SaveSubNodesInfo(void);

FLASH_EXT void Flash_Erase(uint32 StartAddr, uint8 PageCount);
FLASH_EXT void Flash_Read(uint8 *BufPtr, uint16 Num, uint32 Flash_Addr);
FLASH_EXT void Flash_Write(uint8 *BufPtr, uint16 Num, uint32 FlashAddr);


#endif
/***************************************End of file*********************************************/

