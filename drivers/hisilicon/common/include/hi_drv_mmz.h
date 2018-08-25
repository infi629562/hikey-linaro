/*
 * Copyright (C) 2017, Hisilicon Tech. Co., Ltd.
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __HI_DRV_MMZ_H__
#define __HI_DRV_MMZ_H__

/* add include here */
#include <linux/version.h>

#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/seq_file.h>
#include <linux/list.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include "hi_type.h"

#include <linux/scatterlist.h>

/** @addtogroup H_MMZ */
/** @{ */

/***************************** Macro Definition ******************************/
#define MMZ_OTHERS	NULL
#define MMZ_SMMU	"iommu"
/*************************** Structure Definition ****************************/

typedef struct
{
	HI_U8* pu8StartVirAddr;
	HI_U32 u32StartPhyAddr;
	HI_U32 u32Size;
} MMZ_BUFFER_S;

typedef struct
{
	HI_U8* pu8StartVirAddr;
	HI_U32 u32StartSmmuAddr;
	HI_U32 u32Size;
} SMMU_BUFFER_S;

/********************** Global Variable declaration **************************/

/******************************* API declaration *****************************/
/*alloc mmz memory, get physic address and map kernel-state address*/
/*CNcomment:����mmz�ڴ棬�õ�������ַ�������ں�̬��ַ��ӳ��*/
HI_S32	HI_DRV_MMZ_AllocAndMap(const char *name, char *mmzzonename, HI_U32 size, int align, MMZ_BUFFER_S *psMBuf);

/*unmap kernel-state address, release mmz memory*/
/*CNcomment:����ں�̬��ַ��ӳ�䣬���ͷ�mmz�ڴ�*/
HI_VOID HI_DRV_MMZ_UnmapAndRelease(MMZ_BUFFER_S *psMBuf);

/*Only alloc mmz memory, return physic address, but not map kernel-state address*/
/*CNcomment:ֻ����mmz�ڴ棬����������ַ�������ں�̬��ַ��ӳ��*/
HI_S32	HI_DRV_MMZ_Alloc(const char *bufname, char *zone_name, HI_U32 size, int align, MMZ_BUFFER_S *psMBuf);

/*map kernel-state address after alloc mmz memory for cache, and flushing cache with HI_DRV_MMZ_Flush*/
/*CNcomment:����mmz��Cache�ڴ�����������ں�̬��ַ��ӳ��, ��ʹ��HI_DRV_MMZ_Flush����cacheͬ�� */
HI_S32 HI_DRV_MMZ_MapCache(MMZ_BUFFER_S *psMBuf);

/*flush cache data to memory, needed to call when map memory with HI_DRV_MMZ_MapCache*/
/*CNcomment:ʹ��HI_DRV_MMZ_MapCacheʱ������������HI_DRV_MMZ_Flush����cache����ͬ�� */
HI_S32 HI_DRV_MMZ_Flush(MMZ_BUFFER_S *psMBuf);

/*alloc mmz memory, and map kernel-state address*/
/*CNcomment:����mmz�ڴ�����������ں�̬��ַ��ӳ��*/
HI_S32	HI_DRV_MMZ_Map(MMZ_BUFFER_S *psMBuf);

/*unmap kernel-state address*/
/*CNcomment:����ں�̬��ַ��ӳ��*/
HI_VOID HI_DRV_MMZ_Unmap(MMZ_BUFFER_S *psMBuf);

/*release unmapped mmz memory */
/*CNcomment:���ӳ���������û�н����ں�̬ӳ���mmz�ڴ�����ͷ�*/
HI_VOID HI_DRV_MMZ_Release(MMZ_BUFFER_S *psMBuf);

/* get phy addr addr by kvirt	*/
HI_S32 HI_DRV_MMZ_GetPhyByVirt(MMZ_BUFFER_S *psMBuf);

// SMMU API
HI_S32	HI_DRV_SMMU_AllocAndMap(const HI_CHAR *bufname, HI_U32 size, HI_U32 align, SMMU_BUFFER_S *pSmmuBuf);

HI_VOID HI_DRV_SMMU_UnmapAndRelease(SMMU_BUFFER_S *pSmmuBuf);

HI_S32	HI_DRV_SMMU_Alloc(const HI_CHAR *bufname, HI_U32 size, int align, SMMU_BUFFER_S *pSmmuBuf);

HI_S32	HI_DRV_SMMU_MapCache(SMMU_BUFFER_S *pSmmuBuf);

HI_S32	HI_DRV_SMMU_Flush(SMMU_BUFFER_S *pSmmuBuf);

HI_S32	HI_DRV_SMMU_Map(SMMU_BUFFER_S *pSmmuBuf);

HI_VOID HI_DRV_SMMU_Unmap(SMMU_BUFFER_S *pSmmuBuf);

HI_VOID HI_DRV_SMMU_Release(SMMU_BUFFER_S *pSmmuBuf);

/* map cma mem to smmu	*/
HI_U32 HI_DRV_MMZ_MapToSmmu(MMZ_BUFFER_S *psMBuf);

/* unmap cma mem to smmu   */
HI_S32 HI_DRV_MMZ_UnmapFromSmmu(SMMU_BUFFER_S *psMBuf);

/* get smmu addr addr by kvirt	 */
HI_S32 HI_DRV_SMMU_GetSmmuAddrByVirt(SMMU_BUFFER_S *psMBuf);

/* get page table base address and the write or read address when smmu occurs error*/
/*
pu32ptaddr: the page table base address
		  moudules need to configure it to smmu common register SMMU_SCB_TTBR and SMMU_CB_TTBR

pu32err_rdaddr: the smmu hardware will give the address if the smmu hardward run error,and
			the module can	read data which are always 0 from this address
			moudules need to configure it smmu common register SMMU_ERR_RDADDR

pu32err_wraddr: the smmu hardware will give the address if the smmu hardward run error,and
			the module can	write data to this address so can avoid to write other module's address
			moudules need to configure it to their's smmu common register SMMU_ERR_WRADDR
*/
HI_VOID HI_DRV_SMMU_GetPageTableAddr(HI_U32 *pu32ptaddr, HI_U32 *pu32err_rdaddr, HI_U32 *pu32err_wraddr);

/* increment refcount for nosec smmu buffer */
HI_S32 HI_DRV_SMMU_Buffer_Get(SMMU_BUFFER_S *pSmmuBuf);

/* decrement refcount for nosec smmu buffer  */
HI_S32 HI_DRV_SMMU_Buffer_Put(SMMU_BUFFER_S *pSmmuBuf);

/* alloc sec cma mem */
HI_S32	HI_DRV_SECMMZ_Alloc(const HI_CHAR *bufname, HI_U32 size, HI_U32 align, MMZ_BUFFER_S *pSecMBuf);

/* free sec cma mem */
HI_S32 HI_DRV_SECMMZ_Release(const MMZ_BUFFER_S *pSecMBuf);

/* alloc sec sys mem  */
HI_S32	HI_DRV_SECSMMU_Alloc(const HI_CHAR *bufname, HI_U32 size, HI_U32 align, SMMU_BUFFER_S *pSecSmmuBuf);

/* free sec sys mem*/
HI_S32 HI_DRV_SECSMMU_Release(const SMMU_BUFFER_S *pSecSmmuBuf);

/*normal smmu map to sec smmu */
HI_S32 HI_DRV_SMMU_MapToSecSmmu(HI_U32 nonSecsmmu, SMMU_BUFFER_S *pSecSmmuBuf);

/*normal smmu unmap from sec smmu  */
HI_S32 HI_DRV_SMMU_UnmapFromSecSmmu(const SMMU_BUFFER_S *pSecSmmuBuf);

/* normal cma mem map to sec smmu*/
HI_S32 HI_DRV_MMZ_MapToSecSmmu(const MMZ_BUFFER_S *psMBuf, SMMU_BUFFER_S *pSecSmmuBuf);

/* normal cma mem unmap from sec smmu*/
HI_S32 HI_DRV_MMZ_UnmapFromSecSmmu(const SMMU_BUFFER_S *pSecSmmuBuf);

/* increment refcount for nosec mmz buffer */
HI_S32 HI_DRV_MMZ_Buffer_Get(MMZ_BUFFER_S *psMBuf);

/* decrement refcount for nosec mmz buffer  */
HI_S32 HI_DRV_MMZ_Buffer_Put(MMZ_BUFFER_S *psMBuf);

/* increment refcount for sec mmz buffer */
HI_S32 HI_DRV_SECMMZ_Buffer_Get(MMZ_BUFFER_S *pSecMBuf);

/* decrement refcount for sec mmz buffer  */
HI_S32 HI_DRV_SECMMZ_Buffer_Put(MMZ_BUFFER_S *pSecMBuf);

/* increment refcount for sec smmu buffer */
HI_S32 HI_DRV_SECSMMU_Buffer_Get(SMMU_BUFFER_S *pSecSmmuBuf);

/* decrement refcount for sec smmu buffer  */
HI_S32 HI_DRV_SECSMMU_Buffer_Put(SMMU_BUFFER_S *pSecSmmuBuf);


/* query where the iommu addr come from
 * pSmmuBuf: input, smmu address should be input
 * source: output, 0: iommu addr from mmz driver
 *		   1: iommu addr from other driver such as  ion/gpu ..
 *		   -1: iommu addr is illegal
 * return val:
 *		   0: exec successfully
 *		   !0: exec failed
 *
 * when return val is !0,the source should be ignored.
 * */
HI_S32 HI_DRV_SMMU_Query_Buffer_Source(SMMU_BUFFER_S *pSmmuBuf, HI_S32 *source);
/** @} */



/* query where the sec_iommu addr come from
 * pSmmuBuf: input, smmu address should be input
 * source: output, 0: sec_iommu addr from mmz driver
 *		  -1: sec_iommu addr from other driver such as  ion/gpu ..
 * return val:
 *		   0: exec successfully
 *		   !0: exec failed
 *
 * when return val is !0,the source should be ignored.
 * */
HI_S32 HI_DRV_SEC_SMMU_Query_Buffer_Source(unsigned int	 sec_smmu, HI_S32 *source);
/** @} */

int HI_DRV_MMZ_Init(void);
void HI_DRV_MMZ_Exit(void);

HI_S32 DRV_MMZ_ModInit(HI_VOID);
HI_VOID DRV_MMZ_ModExit(HI_VOID);

#ifdef __cplusplus
 #if __cplusplus
}
 #endif
#endif /* __cplusplus */

#endif
