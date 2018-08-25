
/******************************************************************************
  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.
******************************************************************************
File Name     : drv_disp_hw.c
Version	      : Initial Draft
Author	      : Hisilicon multimedia software group
Created	      : 2012/12/30
Last Modified :
Description   :
Function List :
History	      :
******************************************************************************/

/* SPDX-License-Identifier: GPL-2.0 */

#include "drv_win_buffer.h"
#include "drv_disp_bufcore.h"
#include "drv_disp_buffer.h"
#include "drv_window.h"
#include "drv_win_priv.h"

#include "drv_win_hdr.h"
#include<linux/ion.h>

#define  MMZ_REF_COUNT_MAX  782
#define  TMP_MMZ_BUFFER_NUM 32

#if defined(HI_NVR_SUPPORT)
#define  OPEN_REFCNT 0
#else
#define  OPEN_REFCNT 1
#endif
typedef enum tagVDP_MEM_SOURCE_E
{
    VDP_MEM_SOURCE_SMMU = 0,
    VDP_MEM_SOURCE_ION,
    VDP_MEM_SOURCE_ILLEGAL,
    VDP_MEM_SOURCE_BUTT
}VDP_MEM_SOURCE_E;

typedef struct MemBufRef
{
    spinlock_t NodeSpinLock;
    DISP_MMZ_BUF_S  aMMZMemBufForReferenceOpt[MMZ_REF_COUNT_MAX];
    volatile HI_U32  u32MemBufWrPtr;
    volatile HI_U32  u32MemBufRdPtr;
} MemBufRef_S;

HI_VOID WinBuf_DebugAddIncreaseMemRefCnt(WB_DEBUG_INFO_S *pstInfo);
HI_VOID WinBuf_DebugAddDecreaseMemRefCnt(WB_DEBUG_INFO_S *pstInfo);

MemBufRef_S g_stMemBufRef =
{
    .NodeSpinLock = __SPIN_LOCK_UNLOCKED(g_stMemBufRef.NodeSpinLock),
    .aMMZMemBufForReferenceOpt = {{0, 0, 0, 0, 0}},
    .u32MemBufWrPtr = 0,
    .u32MemBufRdPtr = 0,
};

#if OPEN_REFCNT
static MemBufRef_S *GetMemBufRef(HI_VOID)
{
    return &g_stMemBufRef;
}
#endif

HI_VOID ConvertRefSourceType(HI_S32 s32Source, VDP_MEM_SOURCE_E * penMemSource)
{
    if (0 == s32Source)
    {
        *penMemSource = VDP_MEM_SOURCE_SMMU;
    }
    else if (1 == s32Source)
    {
        *penMemSource = VDP_MEM_SOURCE_ION;
    }
    else
    {
        *penMemSource = VDP_MEM_SOURCE_ILLEGAL;
    }
    return;
}
static HI_S32 IncreaseFrameMemRefcnt(HI_U32  u32StartPhyAddr,
                                     HI_U32  u32Size,
                                     HI_BOOL bSecure)
{
#if OPEN_REFCNT
    HI_S32 s32Ret = HI_FAILURE;

#ifdef  HI_SMMU_SUPPORT
    SMMU_BUFFER_S stSMMUBuf = {0};
    VDP_MEM_SOURCE_E enMemSource = VDP_MEM_SOURCE_BUTT;
    HI_S32 s32Source = VDP_MEM_SOURCE_BUTT;
#else
    MMZ_BUFFER_S stMMZBuf = {0};
#endif

    if (HI_TRUE == bSecure)
    {
#ifdef  HI_SMMU_SUPPORT
        stSMMUBuf.pu8StartVirAddr  =  0;
        stSMMUBuf.u32StartSmmuAddr =  u32StartPhyAddr;
        stSMMUBuf.u32Size          =  u32Size;

        if (HI_SUCCESS != HI_DRV_SEC_SMMU_Query_Buffer_Source(u32StartPhyAddr, &s32Source))
        {
            WIN_FATAL("query  buf source failed,addr:%x, size:%d\n", u32StartPhyAddr, u32Size);
            return HI_FAILURE;
        }


        ConvertRefSourceType(s32Source, &enMemSource);

        if (VDP_MEM_SOURCE_SMMU == enMemSource)
        {
            s32Ret = HI_DRV_SECSMMU_Buffer_Get(&stSMMUBuf);
            if (HI_SUCCESS != s32Ret)
            {
                 WIN_FATAL("HI_DRV_SECSMMU_Buffer_Get failed ,secure addr=0x%x,s32Ret = %d\n", u32StartPhyAddr, s32Ret);
            }
        }
        else if (VDP_MEM_SOURCE_ION == enMemSource)
        {
            s32Ret = ion_get(u32StartPhyAddr);
            if (HI_SUCCESS != s32Ret)
            {
                 WIN_FATAL("ion_get failed , secure addr=0x%x, s32Ret = %d\n", u32StartPhyAddr,s32Ret);
            }
        }
        else
        {
            WIN_FATAL("query  buf source Type %d,can't support ref cnt\n", enMemSource);
            return HI_FAILURE;
        }
#else
        stMMZBuf.pu8StartVirAddr = 0;
        stMMZBuf.u32StartPhyAddr = u32StartPhyAddr;
        stMMZBuf.u32Size         = u32Size;

        s32Ret = HI_DRV_SECMMZ_Buffer_Get(&stMMZBuf);
#endif
    }
    else
    {
#ifdef  HI_SMMU_SUPPORT
        stSMMUBuf.pu8StartVirAddr  =  0;
        stSMMUBuf.u32StartSmmuAddr =  u32StartPhyAddr;
        stSMMUBuf.u32Size          =  u32Size;
        if (HI_SUCCESS != HI_DRV_SMMU_Query_Buffer_Source(&stSMMUBuf, &s32Source))
        {
            WIN_FATAL("query  buf source failed,addr:%x, size:%d\n", u32StartPhyAddr, u32Size);
            return HI_FAILURE;
        }
        ConvertRefSourceType(s32Source, &enMemSource);
        if (VDP_MEM_SOURCE_SMMU == enMemSource)
        {
        s32Ret = HI_DRV_SMMU_Buffer_Get(&stSMMUBuf);
        }
        else if (VDP_MEM_SOURCE_ION == enMemSource)
        {
            s32Ret = ion_get(u32StartPhyAddr);
        }
        else
        {
            WIN_FATAL("query  buf source Type %d,can't support ref cnt\n", enMemSource);
            return HI_FAILURE;
        }
#else
        stMMZBuf.pu8StartVirAddr = 0;
        stMMZBuf.u32StartPhyAddr = u32StartPhyAddr;
        stMMZBuf.u32Size         = u32Size;

        s32Ret = HI_DRV_MMZ_Buffer_Get(&stMMZBuf);
#endif
    }

    return s32Ret;
#else
    return HI_SUCCESS;
#endif
}

static HI_S32 DecreaseFrameMemRefcnt(HI_U32  u32StartPhyAddr,
                                     HI_U32  u32Size,
                                     HI_BOOL bSecure)
{
#if  OPEN_REFCNT
    HI_SIZE_T irqflag = 0;
    HI_U32  u32Wptr = 0, u32Rdtr = 0;
    HI_S32 s32Ret = HI_FAILURE;
    MemBufRef_S *pMemBufRef = GetMemBufRef();

    spin_lock_irqsave(&pMemBufRef->NodeSpinLock, irqflag);

    u32Wptr = pMemBufRef->u32MemBufWrPtr;
    u32Rdtr = pMemBufRef->u32MemBufRdPtr;

    if (((u32Wptr  + 1) % MMZ_REF_COUNT_MAX) == u32Rdtr)
    {
        WIN_FATAL("The mem ref buf is full,wr:%d, rd:%d\n", u32Wptr, u32Rdtr);
        s32Ret = HI_ERR_VO_BUFQUE_FULL;
        goto __DECREASE_CNT_EXIT;
    }

    pMemBufRef->aMMZMemBufForReferenceOpt[u32Wptr].pu8StartVirAddr = 0;
    pMemBufRef->aMMZMemBufForReferenceOpt[u32Wptr].u32StartPhyAddr = u32StartPhyAddr;
    pMemBufRef->aMMZMemBufForReferenceOpt[u32Wptr].u32Size = u32Size;
    pMemBufRef->aMMZMemBufForReferenceOpt[u32Wptr].bSecure = bSecure;

#ifdef  HI_SMMU_SUPPORT
    pMemBufRef->aMMZMemBufForReferenceOpt[u32Wptr].bSmmu   = HI_TRUE;
#else
    pMemBufRef->aMMZMemBufForReferenceOpt[u32Wptr].bSmmu   = HI_FALSE;
#endif

    pMemBufRef->u32MemBufWrPtr = (u32Wptr  + 1) % MMZ_REF_COUNT_MAX;
    s32Ret = HI_SUCCESS;

__DECREASE_CNT_EXIT:

    spin_unlock_irqrestore(&pMemBufRef->NodeSpinLock, irqflag);
    return s32Ret;
#else
    return HI_SUCCESS;
#endif

}

HI_S32  IncreaseFrameCntAccordingToFrame(HI_DRV_VIDEO_FRAME_S *pstFrame,
        WB_DEBUG_INFO_S *pstInfo)
{
    HI_U32 u32PhyAddr_Y               = 0;
    HI_U32 u32PhyAddr_Y_RightEye      = 0;
    HI_S32 s32Ret = HI_FAILURE;

    if (pstFrame->ePixFormat >= HI_DRV_PIX_BUTT)
    {
        WIN_ERROR("parameter out of range.\n");
        return HI_ERR_VO_FRAME_INFO_ERROR;
    }

    if (((pstFrame->ePixFormat >= HI_DRV_PIX_FMT_NV08_CMP)
         && (pstFrame->ePixFormat <= HI_DRV_PIX_FMT_NV42_CMP))
        || (pstFrame->ePixFormat == HI_DRV_PIX_FMT_NV12_TILE_CMP)
        || (pstFrame->ePixFormat == HI_DRV_PIX_FMT_NV21_TILE_CMP))
    {
        u32PhyAddr_Y            = pstFrame->stBufAddr[0].u32PhyAddr_YHead;
        u32PhyAddr_Y_RightEye   = pstFrame->stBufAddr[1].u32PhyAddr_YHead;
    }
    else
    {
        u32PhyAddr_Y            = pstFrame->stBufAddr[0].u32PhyAddr_Y;
        u32PhyAddr_Y_RightEye   = pstFrame->stBufAddr[1].u32PhyAddr_Y;
    }

    s32Ret = IncreaseFrameMemRefcnt(u32PhyAddr_Y, 0, pstFrame->bSecure);
    if (HI_SUCCESS == s32Ret)
    {
        WinBuf_DebugAddIncreaseMemRefCnt(pstInfo);
    }
    else
    {
        WIN_ERROR("IncreaseFrameMemRefcnt fai,maybe addr is invalib.\n");
        return HI_ERR_VO_FRAME_INFO_ERROR;
    }

    if (HI_DRV_FT_NOT_STEREO != pstFrame->eFrmType)
    {
        s32Ret = IncreaseFrameMemRefcnt(u32PhyAddr_Y_RightEye, 0, pstFrame->bSecure);
        if (HI_SUCCESS == s32Ret)
        {
            WinBuf_DebugAddIncreaseMemRefCnt(pstInfo);
        }
        else
        {
            WIN_ERROR("IncreaseFrameMemRefcnt fai,maybe addr is invalib.\n");
            return HI_ERR_VO_FRAME_INFO_ERROR;
    }
    }


    return HI_SUCCESS;
}

static HI_VOID  GetFrameAddr(HI_DRV_VIDEO_FRAME_S *pstFrame,
                             HI_U32 *pu32PhyAddr_Y,
                             HI_U32 *pu32PhyAddr_Y_RightEye)
{

    if (((pstFrame->ePixFormat >= HI_DRV_PIX_FMT_NV08_CMP)
         && (pstFrame->ePixFormat <= HI_DRV_PIX_FMT_NV42_CMP))
        || (pstFrame->ePixFormat == HI_DRV_PIX_FMT_NV12_TILE_CMP)
        || (pstFrame->ePixFormat == HI_DRV_PIX_FMT_NV21_TILE_CMP))
    {
        *pu32PhyAddr_Y            = pstFrame->stBufAddr[0].u32PhyAddr_YHead;
        *pu32PhyAddr_Y_RightEye   = pstFrame->stBufAddr[1].u32PhyAddr_YHead;
    }
    else
    {
        *pu32PhyAddr_Y            = pstFrame->stBufAddr[0].u32PhyAddr_Y;
        *pu32PhyAddr_Y_RightEye   = pstFrame->stBufAddr[1].u32PhyAddr_Y;
    }

    return;
}

static HI_VOID DecreaseFrameRefcnt(HI_DRV_VIDEO_FRAME_S *pstFrame,
                                   WB_DEBUG_INFO_S *pstInfo)
{
    HI_U32 u32PhyAddr_Y               = 0;
    HI_U32 u32PhyAddr_Y_RightEye      = 0;
    HI_S32 s32Ret = HI_FAILURE;
    HI_DRV_VIDEO_PRIVATE_S *pstPriv = HI_NULL;
    DISP_BUF_NODE_S *pstDispBufNode = HI_NULL;
    HI_DRV_VIDEO_FRAME_S  *pstELFrm = HI_NULL;

    pstPriv = (HI_DRV_VIDEO_PRIVATE_S *) & (pstFrame->u32Priv[0]);
    if (/*(HI_TRUE == pstPriv->bForFenceUse)
        || */(HI_DRV_FRAME_VDP_ALLOCATE_STILL == pstFrame->u32StillFrame))
    {
        return;
    }

    (HI_VOID)GetFrameAddr(pstFrame, &u32PhyAddr_Y, &u32PhyAddr_Y_RightEye);

    s32Ret = DecreaseFrameMemRefcnt(u32PhyAddr_Y, 0, pstFrame->bSecure);
    if (HI_SUCCESS == s32Ret)
    {
        /*first  add the release  statistics value*/
        WinBuf_DebugAddDecreaseMemRefCnt(pstInfo);
    }

    /*second,   release  3d right eye or el frame.*/
    if (HI_DRV_FT_NOT_STEREO != pstFrame->eFrmType)
    {
        s32Ret = DecreaseFrameMemRefcnt(u32PhyAddr_Y_RightEye, 0, pstFrame->bSecure);
        if (HI_SUCCESS == s32Ret)
        {
            WinBuf_DebugAddDecreaseMemRefCnt(pstInfo);
        }
    }
    else
    {
        /*judge whether el frame exist, if exist then release.*/
        pstDispBufNode = container_of((HI_U32 *)pstFrame, DISP_BUF_NODE_S, u32Data[0]);
        if (pstDispBufNode->bValidData2)
        {
            pstELFrm = (HI_DRV_VIDEO_FRAME_S *)pstDispBufNode->u32Data2;

            GetFrameAddr(pstELFrm, &u32PhyAddr_Y, &u32PhyAddr_Y_RightEye);
            s32Ret = DecreaseFrameMemRefcnt(u32PhyAddr_Y, 0, pstFrame->bSecure);
            if (HI_SUCCESS == s32Ret)
            {
                WinBuf_DebugAddDecreaseMemRefCnt(pstInfo);
            }
        }
    }



    return;
}


#if OPEN_REFCNT
static HI_VOID PutMemBuf(DISP_MMZ_BUF_S *pstTmpBufNode, HI_U32 u32NodeNum)
{
    HI_U32 i = 0;
#ifdef  HI_SMMU_SUPPORT
    SMMU_BUFFER_S stSMMU_MMZBuf = {0};
    VDP_MEM_SOURCE_E enMemSource = VDP_MEM_SOURCE_BUTT;
    HI_S32 s32Source = VDP_MEM_SOURCE_BUTT;
#else
    MMZ_BUFFER_S  stSMMU_MMZBuf = {0};
#endif

    for (i = 0; i < u32NodeNum; i ++)
    {
        if (pstTmpBufNode[i].bSecure)
        {
#ifdef  HI_SMMU_SUPPORT
            stSMMU_MMZBuf.pu8StartVirAddr  = pstTmpBufNode[i].pu8StartVirAddr;
            stSMMU_MMZBuf.u32StartSmmuAddr = pstTmpBufNode[i].u32StartPhyAddr;
            stSMMU_MMZBuf.u32Size          = pstTmpBufNode[i].u32Size;

            if (HI_SUCCESS != HI_DRV_SEC_SMMU_Query_Buffer_Source(stSMMU_MMZBuf.u32StartSmmuAddr, &s32Source))
            {
                WIN_FATAL("query  buf source failed,addr:%x, size:%d\n", stSMMU_MMZBuf.u32StartSmmuAddr, stSMMU_MMZBuf.u32Size);
                return ;
            }

            ConvertRefSourceType(s32Source, &enMemSource);
            if (VDP_MEM_SOURCE_SMMU == enMemSource)
            {
                if (HI_SUCCESS != HI_DRV_SECSMMU_Buffer_Put(&stSMMU_MMZBuf))
                {
                    WIN_FATAL("HI_DRV_SECSMMU_Buffer_Put failed %x, size %d\n", stSMMU_MMZBuf.u32StartSmmuAddr, stSMMU_MMZBuf.u32Size);
                }
            }
            else if (VDP_MEM_SOURCE_ION == enMemSource)
            {
                if (HI_SUCCESS != ion_put(stSMMU_MMZBuf.u32StartSmmuAddr))
                {
                    WIN_FATAL("ion_put failed addr %x, size %d\n", stSMMU_MMZBuf.u32StartSmmuAddr, stSMMU_MMZBuf.u32Size);
                    return;
                }
            }
            else
            {
                WIN_FATAL("query  buf source Type %d,can't support ref cnt\n", enMemSource);
                return;
            }
#else
            stSMMU_MMZBuf.pu8StartVirAddr  = pstTmpBufNode[i].pu8StartVirAddr;
            stSMMU_MMZBuf.u32StartPhyAddr  = pstTmpBufNode[i].u32StartPhyAddr;
            stSMMU_MMZBuf.u32Size          = pstTmpBufNode[i].u32Size;
            HI_DRV_SECMMZ_Buffer_Put(&stSMMU_MMZBuf);
#endif
        }
        else
        {
#ifdef  HI_SMMU_SUPPORT
            stSMMU_MMZBuf.pu8StartVirAddr  = pstTmpBufNode[i].pu8StartVirAddr;
            stSMMU_MMZBuf.u32StartSmmuAddr = pstTmpBufNode[i].u32StartPhyAddr;
            stSMMU_MMZBuf.u32Size          = pstTmpBufNode[i].u32Size;
            if (HI_SUCCESS != HI_DRV_SMMU_Query_Buffer_Source(&stSMMU_MMZBuf, &s32Source))
            {
                WIN_FATAL("query  buf source failed,addr:%x, size:%d\n", stSMMU_MMZBuf.u32StartSmmuAddr, stSMMU_MMZBuf.u32Size);
                return ;
            }
            ConvertRefSourceType(s32Source, &enMemSource);
            if (VDP_MEM_SOURCE_SMMU == enMemSource)
            {
            HI_DRV_SMMU_Buffer_Put(&stSMMU_MMZBuf);
            }
            else if (VDP_MEM_SOURCE_ION == enMemSource)
            {
                if (HI_SUCCESS != ion_put(stSMMU_MMZBuf.u32StartSmmuAddr))
                {
                    WIN_FATAL("ion_put failed addr %x, size %d\n", stSMMU_MMZBuf.u32StartSmmuAddr, stSMMU_MMZBuf.u32Size);
                    return;
                }
            }
            else
            {
                WIN_FATAL("query  buf source Type %d,can't support ref cnt\n", enMemSource);
                return;
            }
#else
            stSMMU_MMZBuf.pu8StartVirAddr  = pstTmpBufNode[i].pu8StartVirAddr;
            stSMMU_MMZBuf.u32StartPhyAddr  = pstTmpBufNode[i].u32StartPhyAddr;
            stSMMU_MMZBuf.u32Size          = pstTmpBufNode[i].u32Size;
            HI_DRV_MMZ_Buffer_Put(&stSMMU_MMZBuf);
#endif
        }
    }


    return;
}

#endif

HI_VOID WinBuf_CheckMemRefCntReset(HI_U32 u32WinIndex, WB_POOL_S *pstWinBP)
{
    if (pstWinBP->pstDebugInfo->u32IncreaseMemRefCnt !=
        pstWinBP->pstDebugInfo->u32DecreaseMemRefCnt)
    {
        WIN_FATAL("Memleak, Windex:%x, GetRef:%d, PutRef:%d!\n", u32WinIndex,
                  pstWinBP->pstDebugInfo->u32IncreaseMemRefCnt,
                  pstWinBP->pstDebugInfo->u32DecreaseMemRefCnt);
    }

    return;
}

HI_VOID WinBuf_RetAllMemRefCnts(HI_VOID)
{
#if  OPEN_REFCNT
    HI_SIZE_T irqflag = 0;
    HI_U32  u32MemBufWrPtr = 0, u32MemBufRdPtr = 0, i = 0;
    MemBufRef_S *pMemBufRef = GetMemBufRef();
    DISP_MMZ_BUF_S stTmpBufNode[TMP_MMZ_BUFFER_NUM] = {{0, 0, 0, 0, 0}};

    spin_lock_irqsave(&pMemBufRef->NodeSpinLock, irqflag);

    for (i = 0; i < TMP_MMZ_BUFFER_NUM; i ++ )
    {
        u32MemBufWrPtr = pMemBufRef->u32MemBufWrPtr;
        u32MemBufRdPtr = pMemBufRef->u32MemBufRdPtr;

        if (u32MemBufRdPtr == u32MemBufWrPtr)
        {
            break;
        }

        stTmpBufNode[i].u32StartPhyAddr = pMemBufRef->aMMZMemBufForReferenceOpt[u32MemBufRdPtr].u32StartPhyAddr;
        stTmpBufNode[i].pu8StartVirAddr   = pMemBufRef->aMMZMemBufForReferenceOpt[u32MemBufRdPtr].pu8StartVirAddr;
        stTmpBufNode[i].u32Size       = pMemBufRef->aMMZMemBufForReferenceOpt[u32MemBufRdPtr].u32Size;
        stTmpBufNode[i].bSecure       = pMemBufRef->aMMZMemBufForReferenceOpt[u32MemBufRdPtr].bSecure;

        pMemBufRef->u32MemBufRdPtr = (pMemBufRef->u32MemBufRdPtr + 1) % MMZ_REF_COUNT_MAX;
    }

    spin_unlock_irqrestore(&pMemBufRef->NodeSpinLock, irqflag);

    (HI_VOID) PutMemBuf(stTmpBufNode, i);
#endif

    return;
}

WB_DEBUG_INFO_S * WinBuf_DebugCreate(HI_U32 recordnum)
{
    WB_DEBUG_INFO_S *pstInfo;

    if (0 == recordnum)
    {
	return HI_NULL;
    }

    pstInfo = (WB_DEBUG_INFO_S *)DISP_MALLOC(sizeof(WB_DEBUG_INFO_S));
    if (!pstInfo)
    {
	return HI_NULL;
    }

    DISP_MEMSET(pstInfo, 0 , sizeof(WB_DEBUG_INFO_S));

    pstInfo->u32RecordNumber = recordnum;
    return pstInfo;
}


HI_VOID WinBuf_DebugDestroy(WB_DEBUG_INFO_S *pstInfo)
{
    if (pstInfo)
    {
	DISP_FREE(pstInfo);
    }
}

HI_VOID WinBuf_DebugAddInput(WB_DEBUG_INFO_S *pstInfo, HI_U32 u32FrameId)
{
    pstInfo->u32InputFrameID[pstInfo->u32InputPos] = u32FrameId;

    pstInfo->u32InputPos = (pstInfo->u32InputPos + 1)%pstInfo->u32RecordNumber;
    pstInfo->u32Input++;
}

HI_VOID WinBuf_DebugAddCfg(WB_DEBUG_INFO_S *pstInfo, HI_U32 u32FrameId)
{
    pstInfo->u32CfgFrameID[pstInfo->u32CfgPos] = u32FrameId;

    pstInfo->u32CfgPos = (pstInfo->u32CfgPos + 1)%pstInfo->u32RecordNumber;
    pstInfo->u32Config++;
}

HI_VOID WinBuf_DebugAddRls(WB_DEBUG_INFO_S *pstInfo, HI_U32 u32FrameId)
{
    pstInfo->u32RlsFrameID[pstInfo->u32RlsPos] = u32FrameId;

    pstInfo->u32RlsPos = (pstInfo->u32RlsPos + 1)%pstInfo->u32RecordNumber;
    pstInfo->u32Release++;
}

HI_VOID WinBuf_DebugAddTryQF(WB_DEBUG_INFO_S *pstInfo)
{
    pstInfo->u32TryQueueFrame++;
}

HI_VOID WinBuf_DebugAddQFOK(WB_DEBUG_INFO_S *pstInfo)
{
    pstInfo->u32QueueFrame++;
}

HI_VOID WinBuf_DebugAddUnderload(WB_DEBUG_INFO_S *pstInfo)
{
    pstInfo->u32Underload++;
}

HI_VOID WinBuf_DebugAddDisacard(WB_DEBUG_INFO_S *pstInfo)
{
    pstInfo->u32Disacard++;
}

HI_VOID WinBuf_DebugAddIncreaseMemRefCnt(WB_DEBUG_INFO_S *pstInfo)
{
    pstInfo->u32IncreaseMemRefCnt++;
}

HI_VOID WinBuf_DebugAddDecreaseMemRefCnt(WB_DEBUG_INFO_S *pstInfo)
{
    pstInfo->u32DecreaseMemRefCnt++;
}

static HI_S32 WinBuf_AddEmptyNode(WB_POOL_S *pstWinBP,
                                  HI_DRV_VIDEO_FRAME_S *pstDispFrame,
                                  DISP_BUF_NODE_S *pstNodeEmpty)
{
    (HI_VOID)DecreaseFrameRefcnt(pstDispFrame, pstWinBP->pstDebugInfo);
    return DispBuf_AddEmptyNode(&pstWinBP->stBuffer, pstNodeEmpty);
}

HI_S32 WinBuf_Create(HI_U32 u32BufNum, HI_U32 u32MemType, WIN_BUF_ALLOC_PARA_S *pstAlloc, WB_POOL_S *pstWinBP)
{
    HI_S32 nRet = HI_SUCCESS;

    WIN_CHECK_NULL_RETURN(pstWinBP);

    if(u32MemType != WIN_BUF_MEM_SRC_SUPPLY)
    {
	WIN_FATAL("Win buffer NOT SURPPORT ALLOC MEMORY NOW!\n");
	return HI_FAILURE;
    }

    if( (u32BufNum == 0) || (u32BufNum > DISP_BUF_NODE_MAX_NUMBER))
    {
	WIN_FATAL("Win buffer number is invalid = %d\n", u32BufNum);
	return HI_FAILURE;
    }

    DISP_MEMSET(pstWinBP, 0, sizeof(WB_POOL_S));
    DISP_ASSERT(sizeof(HI_DRV_VIDEO_FRAME_S) <= (sizeof(((pstWinBP->stBuffer).pstBufArray[0])->u32Data)));

    pstWinBP->u32BufNumber = u32BufNum;
    pstWinBP->u32MemType   = u32MemType;

    nRet = DispBuf_Create(&pstWinBP->stBuffer, u32BufNum);
    if(nRet != HI_SUCCESS)
    {
	WIN_FATAL("Win create buffer failed\n");
	return nRet;
    }

#ifdef _WIN_BUF_MEM_FB_SUPPLY_
    if(u32MemType == WIN_BUF_MEM_FB_SUPPLY)
    {
	DISP_BUF_NODE_S *pstNode;
	HI_U32 u;

	for(u=0; u<pstWinBP->u32BufNumber; u++)
	{
	    (HI_VOID)DispBuf_GetNodeContent(&pstWinBP->stBuffer, u, &pstNode);
	}

	pstWinBP->u32MemType = WIN_BUF_MEM_FB_SUPPLY;
    }
#endif

    pstWinBP->pstDisplay = HI_NULL;
    pstWinBP->pstConfig	 = HI_NULL;

    pstWinBP->pstDebugInfo = WinBuf_DebugCreate(DISP_BUF_NODE_MAX_NUMBER);
    if (!pstWinBP->pstDebugInfo)
    {
	goto __ERR_EXIT_;
    }

    return HI_SUCCESS;

__ERR_EXIT_:
    if(pstWinBP->u32MemType == WIN_BUF_MEM_FB_SUPPLY)
    {
	DISP_BUF_NODE_S *pstNode;
	HI_U32 u;

	for(u=0; u<pstWinBP->u32BufNumber; u++)
	{
	    (HI_VOID)DispBuf_GetNodeContent(&pstWinBP->stBuffer, u, &pstNode);
	}
    }

    nRet = DispBuf_Destoy(&pstWinBP->stBuffer);

    return HI_FAILURE;
}

HI_S32 WinBuf_Destroy(WB_POOL_S *pstWinBP)
{
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN(pstWinBP);

    WinBuf_DebugDestroy(pstWinBP->pstDebugInfo);

    if(pstWinBP->u32MemType == WIN_BUF_MEM_FB_SUPPLY)
    {
	DISP_BUF_NODE_S *pstNode;
	HI_U32 u;

	for(u=0; u<pstWinBP->u32BufNumber; u++)
	{
	    nRet = DispBuf_GetNodeContent(&pstWinBP->stBuffer, u, &pstNode);

	    // todo : release memory
	}
    }


    nRet = DispBuf_Destoy(&pstWinBP->stBuffer);

    return HI_SUCCESS;
}

HI_S32 WinBuf_Reset(WB_POOL_S *pstWinBP)
{
    WIN_CHECK_NULL_RETURN(pstWinBP);

    pstWinBP->pstDisplay = HI_NULL;
    pstWinBP->pstConfig	 = HI_NULL;
    return HI_SUCCESS;
}

HI_S32 WinBuf_SetSource(WB_POOL_S *pstWinBP, WB_SOURCE_INFO_S *pstSrc)
{
    WIN_CHECK_NULL_RETURN(pstWinBP);
    WIN_CHECK_NULL_RETURN(pstSrc);

    pstWinBP->stSrcInfo.hSrc = pstSrc->hSrc;
    pstWinBP->stSrcInfo.hSecondSrc = pstSrc->hSecondSrc;

    if (HI_NULL != pstSrc->pfAcqFrame)
    {
        pstWinBP->stSrcInfo.pfAcqFrame = pstSrc->pfAcqFrame;
    }

    if (HI_NULL != pstSrc->pfRlsFrame)
    {
        pstWinBP->stSrcInfo.pfRlsFrame = pstSrc->pfRlsFrame;
    }

    return HI_SUCCESS;
}


HI_S32 WinBuf_PutNewFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstFrame, HI_U32 u32PlayCnt)
{
    HI_S32 nRet;
    DISP_BUF_NODE_S *pstNode;
    HI_DRV_VIDEO_PRIVATE_S *pstPriv = HI_NULL;

    WIN_CHECK_NULL_RETURN(pstWinBP);
    WIN_CHECK_NULL_RETURN(pstFrame);

    WinBuf_DebugAddTryQF(pstWinBP->pstDebugInfo);
    nRet = DispBuf_GetEmptyNode(&pstWinBP->stBuffer, &pstNode);
    if (nRet != HI_SUCCESS)
    {
        return nRet;
    }

    /*set a barrier bettween DispBuf_GetEmptyNode and DispBuf_DelEmptyNode,
    * del oper will change the state of node in empty list, but get oper
    * will check it. so a barrier is neccessary.
    */
    barrier();

    nRet = DispBuf_DelEmptyNode(&pstWinBP->stBuffer, pstNode);
    if (nRet != HI_SUCCESS)
    {
        WIN_ERROR("DispBuf_DelEmptyNode failed, ID=0x%x\n", pstNode->u32ID);
        return nRet;
    }

    memcpy(pstNode->u32Data, pstFrame, sizeof(HI_DRV_VIDEO_FRAME_S));
    pstNode->u32PlayCnt = u32PlayCnt;

    pstPriv = (HI_DRV_VIDEO_PRIVATE_S *) & (pstFrame->u32Priv[0]);

    if (/*(HI_TRUE != pstPriv->bForFenceUse)
        && */(HI_DRV_FRAME_VDP_ALLOCATE_STILL != pstFrame->u32StillFrame))
    {
        nRet = IncreaseFrameCntAccordingToFrame(pstFrame, pstWinBP->pstDebugInfo);
        if (nRet)
        {
            return nRet;
        }
    }

    nRet = DispBuf_AddFullNode(&pstWinBP->stBuffer, pstNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    WinBuf_DebugAddInput(pstWinBP->pstDebugInfo, pstFrame->u32FrameIndex);
    WinBuf_DebugAddQFOK(pstWinBP->pstDebugInfo);

    return HI_SUCCESS;
}


HI_DRV_VIDEO_FRAME_S *WinBuf_GetConfigFrame(WB_POOL_S *pstWinBP)
{
    DISP_BUF_NODE_S *pstNode;
    HI_DRV_VIDEO_FRAME_S *pstFrm;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    if (pstWinBP->pstDisplay)
    {
	if (pstWinBP->pstDisplay->u32PlayCnt)
	{
	    HI_DRV_VIDEO_FRAME_S *pstTmpFrm;

	    pstWinBP->pstDisplay->u32PlayCnt--;
	    pstWinBP->pstConfig = pstWinBP->pstDisplay;

	    pstTmpFrm = (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstConfig->u32Data;
	    #if 0
	    WIN_ERROR("idx %d %d %#x\n",
		pstTmpFrm->u32FrameIndex,
		pstWinBP->pstDisplay->u32PlayCnt,
		pstTmpFrm->stBufAddr[0].u32PhyAddr_Y);
	    #endif
	    return (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstConfig->u32Data;
	}
    }

    nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstNode);
    if (nRet != HI_SUCCESS)
    {
	WinBuf_DebugAddUnderload(pstWinBP->pstDebugInfo);
	return HI_NULL;
    }

    nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    pstWinBP->pstConfig = pstNode;
    pstFrm = (HI_DRV_VIDEO_FRAME_S *)pstNode->u32Data;

    pstNode->u32PlayCnt--;

    WinBuf_DebugAddCfg(pstWinBP->pstDebugInfo, pstFrm->u32FrameIndex);

    return  pstFrm;
}


HI_S32 WinBuf_SetCaptureFrame(WB_POOL_S *pstWinBP, HI_U32 u32InvalidFlag)
{
    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);
    if ((pstWinBP->pstCapture) || (u32InvalidFlag == 1))
    {
	WIN_FATAL("do not support continous capturing.\n");
	return HI_ERR_VO_WIN_UNSUPPORT;
    }
    pstWinBP->pstCapture =  pstWinBP->pstDisplay;
    if (!pstWinBP->pstCapture)
	WIN_WARN("-capture should not be null ptr.\n");

    return HI_SUCCESS;
}

HI_S32 WinBuf_ReleaseCaptureFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstFrame, HI_BOOL bForceFlag)
{
    HI_DRV_VIDEO_FRAME_S *pstCaptureFrame = HI_NULL;
    WIN_CHECK_NULL_RETURN(pstWinBP);
    WIN_CHECK_NULL_RETURN(pstFrame);

    /*we may capture normal frame or black frame, so there exists a branch. */
    if (pstWinBP->pstCapture)
	pstCaptureFrame = (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstCapture->u32Data;
    else
	pstCaptureFrame =   BP_GetBlackFrameInfo();

    /* when release, if capture not equal to capture frame nor equal to black
       frame,return error.
    */
    WIN_CHECK_NULL_RETURN(pstCaptureFrame);

    if (pstFrame->stBufAddr[0].u32PhyAddr_Y != pstCaptureFrame->stBufAddr[0].u32PhyAddr_Y)
    {
	WIN_FATAL("-you release wrong capture frame.\n");
	return HI_ERR_VO_INVALID_OPT;
    }

    pstWinBP->pstCapture = NULL;
    return HI_SUCCESS;
}

HI_DRV_VIDEO_FRAME_S *WinBuf_GetCapturedFrame(WB_POOL_S *pstWinBP)
{
    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    if (pstWinBP->pstCapture != HI_NULL)
    {
	return (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstCapture->u32Data;
    }

    return HI_NULL;
}

HI_S32 WinBuf_RlsFrameWithHandle(WB_POOL_S * pstWinBP,HI_DRV_VIDEO_FRAME_S *pstFrm)
{
    HI_S32 Ret = HI_SUCCESS;

    if((HI_NULL == pstWinBP) || (HI_NULL == pstFrm))
    {
	return HI_ERR_VO_NULL_PTR;
    }

    if (HI_DRV_VIDEO_FRAME_TYPE_DOLBY_BL == pstFrm->enSrcFrameType)
    {
	Ret = WinBuf_RlsDolbyFrame(pstWinBP, pstFrm);
    }
    else
    {
	if (pstWinBP->stSrcInfo.hSrc != HI_INVALID_HANDLE)
	{
	    pstWinBP->stSrcInfo.pfRlsFrame(pstWinBP->stSrcInfo.hSrc, pstFrm);
	}
	else
	{
	    HI_ERR_WIN("Cannot release frame.hSrc is invalid.\n");
	    return HI_ERR_VO_FRAME_RELEASE_FAILED;
	}
    }

    return Ret;
}


HI_BOOL CheckFrameCanRealRelease(HI_DRV_VIDEO_FRAME_S *pWantReleaseFrame,
                                 WB_POOL_S *pstWinBP)
{
    if ((pWantReleaseFrame->stBufAddr[0].u32PhyAddr_Y != pstWinBP->u32QuickRlsFrameAddr)
        || (pWantReleaseFrame->u32FrameIndex != pstWinBP->u32QuickRlsFrameIndex))
    {
        return HI_TRUE;
    }
    else
    {
        return HI_FALSE;
    }
}

//release frame that has been displayed and set configed frame as displayed frame.
HI_S32 WinBuf_RlsAndUpdateUsingFrame(WB_POOL_S *pstWinBP)
{
    HI_DRV_VIDEO_FRAME_S *pstDispFrame, *pstCfgFrame;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN(pstWinBP);

    if (pstWinBP->pstDisplay != HI_NULL)
    {
        pstDispFrame = (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstDisplay->u32Data;

        /*1) display not null, and config == display,this
            means repeate-playing  controlled by vdp itself.
              2) not the same with repeate-playing controlled by avplay
                : pstDisplay != pstconfig, but the ptr points the same frame.
              */
        if (pstWinBP->pstDisplay == pstWinBP->pstConfig)
        {
            pstWinBP->pstConfig  = HI_NULL;
        }
        else if (pstWinBP->pstConfig != HI_NULL)
        {
            /*this branch means normal playing:
                          we get new config node, and if not repeated by avplay ,we
                         will release the display node and  move forward.*/
            pstCfgFrame = (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstConfig->u32Data;

            /*if display and config not points the same frame, we will release display node.*/
            if ( (pstDispFrame->stBufAddr[0].u32PhyAddr_Y
                  != pstCfgFrame->stBufAddr[0].u32PhyAddr_Y))
            {
                if (HI_TRUE == CheckFrameCanRealRelease(pstDispFrame, pstWinBP))
                {
                    (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstDispFrame);
                    pstWinBP->u32QuickRlsFrameIndex = pstDispFrame->u32FrameIndex;
                    pstWinBP->u32QuickRlsFrameAddr = pstDispFrame->stBufAddr[0].u32PhyAddr_Y;
                }
            }

            /* release disp buffer*/
            nRet = WinBuf_AddEmptyNode(pstWinBP, pstDispFrame, pstWinBP->pstDisplay);
            DISP_ASSERT(nRet == HI_SUCCESS);

            /*move forward.*/
            pstWinBP->pstDisplay = pstWinBP->pstConfig;
            pstWinBP->pstConfig  = HI_NULL;
        }
        else
        {
            /*this branch means: in last intr,we undergo a reset, and in black frame mode. so we did
                         not receive new frame from avplay, in this intr, we should release it.
                      */
            if (HI_TRUE == CheckFrameCanRealRelease(pstDispFrame, pstWinBP))
            {
                pstWinBP->u32QuickRlsFrameIndex = pstDispFrame->u32FrameIndex;
                pstWinBP->u32QuickRlsFrameAddr = pstDispFrame->stBufAddr[0].u32PhyAddr_Y;
                (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstDispFrame);
            }

            nRet = WinBuf_AddEmptyNode(pstWinBP, pstDispFrame, pstWinBP->pstDisplay);
            DISP_ASSERT(nRet == HI_SUCCESS);
            pstWinBP->pstDisplay = HI_NULL;
        }

    }
    else if (pstWinBP->pstConfig != HI_NULL)
    {
        pstWinBP->pstDisplay = pstWinBP->pstConfig;

        pstWinBP->pstConfig = HI_NULL;
    }

    return HI_SUCCESS;
}

HI_S32 WinBuf_RepeatDisplayedFrame(WB_POOL_S *pstWinBP)
{
    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    DISP_ASSERT(HI_NULL == pstWinBP->pstConfig);

    pstWinBP->pstConfig = pstWinBP->pstDisplay;

    return HI_SUCCESS;
}

HI_S32 WinBuf_DiscardDisplayedFrame(WB_POOL_S *pstWinBP)
{
    HI_BOOL bInFullList = HI_FALSE;
    HI_DRV_VIDEO_FRAME_S *pstDispFrame = HI_NULL;
    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    if (pstWinBP->pstConfig != HI_NULL)
    {
        WIN_ERROR("DiscardDisplayedFrame Error.\n");
        return HI_FAILURE;
    }

    if (pstWinBP->pstDisplay != HI_NULL)
    {
        pstDispFrame = (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstDisplay->u32Data;

        bInFullList = DispBuf_FullListHasSameNode(&(pstWinBP->stBuffer),
                      pstWinBP->pstDisplay);
        if (!bInFullList)
        {
            (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstDispFrame);
        }

        (HI_VOID)WinBuf_AddEmptyNode(pstWinBP, pstDispFrame, pstWinBP->pstDisplay);
    }

    pstWinBP->pstConfig = HI_NULL;
    pstWinBP->pstDisplay = HI_NULL;

    return HI_SUCCESS;
}
HI_DRV_VIDEO_FRAME_S *WinBuf_GetDisplayedFrame(WB_POOL_S *pstWinBP)
{
    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    if (pstWinBP->pstDisplay != HI_NULL)
    {
	return (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstDisplay->u32Data;
    }

    return HI_NULL;
}

HI_DRV_VIDEO_FRAME_S *WinBuf_GetConfigedFrame(WB_POOL_S *pstWinBP)
{
    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    if (pstWinBP->pstConfig != HI_NULL)
    {
	return (HI_DRV_VIDEO_FRAME_S *)pstWinBP->pstConfig->u32Data;
    }

    return HI_NULL;
}

HI_S32 WinBuf_ForceReleaseFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstFrame)
{
    WIN_CHECK_NULL_RETURN(pstWinBP);

    if (HI_NULL == pstFrame)
    {
        return HI_SUCCESS;
    }

    (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstFrame);
    if (HI_NULL != pstWinBP->pstDisplay)
    {
        (HI_VOID)WinBuf_AddEmptyNode(pstWinBP, pstFrame, pstWinBP->pstDisplay);
    }

    return HI_SUCCESS;
}

HI_S32 WinBuf_ReleaseOneFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstPreFrame)
{
    DISP_BUF_NODE_S *pstWBNode, *pstWBNextNode;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame, *pstNextFrame;
    HI_U32 u32UsingAddrY;
    HI_U32 u32UsingIdx;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN(pstWinBP);

    if (pstPreFrame)
    {
        u32UsingAddrY = pstPreFrame->stBufAddr[0].u32PhyAddr_Y;
        u32UsingIdx = pstPreFrame->u32FrameIndex;
    }
    else
    {
        u32UsingAddrY = 0;
        u32UsingIdx = 0xffffffff;
    }

    nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstWBNode);
    if (nRet != HI_SUCCESS)
    {
        return HI_FAILURE;
    }

    nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
    if (nRet != HI_SUCCESS)
    {
        return HI_FAILURE;
    }

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
    pstNextFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNextNode->u32Data;

    if ((pstCurrFrame->stBufAddr[0].u32PhyAddr_Y != u32UsingAddrY)
        && (pstCurrFrame->stBufAddr[0].u32PhyAddr_Y !=
            pstNextFrame->stBufAddr[0].u32PhyAddr_Y))
    {
        (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstCurrFrame);
    }
    /*
       why add Idx != UsingIdx?
       1.vo may release the same addr frame twice
       2.if 1,vpss will check the error ,then don't release the addr
       3.if 2,vo may receive two same addr frame with diff Idx.
       4.if 3,vpss is blocked
       to avoid this situation,add add Idx != UsingIdx here.
     */
    else if ((pstCurrFrame->stBufAddr[0].u32PhyAddr_Y != u32UsingAddrY
              || pstCurrFrame->u32FrameIndex != u32UsingIdx)
             && (pstCurrFrame->stBufAddr[0].u32PhyAddr_Y !=
                 pstNextFrame->stBufAddr[0].u32PhyAddr_Y)
            )
    {
        // if the current frame is different with next frame and not using,
        // release it.
        (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstCurrFrame);

    }

    // release node of current frame and add to empty array.
    nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    nRet = WinBuf_AddEmptyNode(pstWinBP, pstCurrFrame, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    WinBuf_DebugAddDisacard(pstWinBP->pstDebugInfo);

    return HI_SUCCESS;
}

HI_S32 WinBuf_FlushWaitingFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstPreFrame)
{
    DISP_BUF_NODE_S *pstWBNode, *pstWBNextNode;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame;
    HI_U32 u32UsingAddrY;
    HI_U32 u32UsingIdx;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN(pstWinBP);

    if (pstPreFrame)
    {
        u32UsingAddrY = pstPreFrame->stBufAddr[0].u32PhyAddr_Y;
        u32UsingIdx = pstPreFrame->u32FrameIndex;
    }
    else
    {
        u32UsingAddrY = 0;
        u32UsingIdx = 0xffffffff;
    }

    nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstWBNode);
    if (nRet != HI_SUCCESS)
    {
        return HI_SUCCESS;
    }

    nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);

    while (nRet == HI_SUCCESS)
    {
        WinBuf_ReleaseOneFrame(pstWinBP, pstPreFrame);

        // update position, 'pstWBNextNode' becomes current node, and get new next node
        pstWBNode = pstWBNextNode;

        nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
    }

    // all node is clean exept the last one, now release it.

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;

    if (pstCurrFrame->stBufAddr[0].u32PhyAddr_Y != u32UsingAddrY)
    {
        (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstCurrFrame);
    }
    /*
       why add Idx != UsingIdx?
       1.vo may release the same addr frame twice
       2.if 1,vpss will check the error ,then don't release the addr
       3.if 2,vo may receive two same addr frame with diff Idx.
       4.if 3,vpss is blocked
       to avoid this situation,add add Idx != UsingIdx here.
     */
    else if (pstCurrFrame->stBufAddr[0].u32PhyAddr_Y != u32UsingAddrY
             || pstCurrFrame->u32FrameIndex != u32UsingIdx)
    {
        (HI_VOID)WinBuf_RlsFrameWithHandle(pstWinBP, pstCurrFrame);
    }

    nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    nRet = WinBuf_AddEmptyNode(pstWinBP, pstCurrFrame, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    return HI_SUCCESS;
}

HI_BOOL WinBuf_FindDstFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstDstFrame)
{
    DISP_BUF_NODE_S *pstWBNode;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame;
    HI_DRV_VIDEO_PRIVATE_S *pstDstPriv, *pstPriv;
    HI_S32 nRet;
    HI_U32 u;

    if (!pstDstFrame)
    {
	return HI_FALSE;
    }

    // get dst frame private information.
    pstDstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstDstFrame->u32Priv[0]);

    for (u=0; u<pstWinBP->u32BufNumber; u++)
    {
	// get next new frame node
	nRet = DispBuf_GetFullNodeByIndex(&pstWinBP->stBuffer, u, &pstWBNode);
	if (nRet != HI_SUCCESS)
	{
	    return HI_FALSE;
	}

	// get frame info of new node.
	pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
	pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);

	// check whether frame information is matchable
	if (  (pstCurrFrame->u32FrameIndex == pstDstFrame->u32FrameIndex)
	    &&(pstCurrFrame->u32Pts	   == pstDstFrame->u32Pts)
	    &&(pstPriv->eOriginField	   == pstDstPriv->eOriginField)
	    )
	{
	    return HI_TRUE;
	}
    }

    return HI_FALSE;
}

//#define WIN_BUF_GetFrameByMaxID_PRINT
HI_DRV_VIDEO_FRAME_S * WinBuf_GetFrameByDstFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstDstFrame, HI_DRV_VIDEO_FRAME_S *pstRefFrame)
{
    DISP_BUF_NODE_S *pstWBNode = HI_NULL;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame;
    HI_DRV_VIDEO_PRIVATE_S *pstDstPriv, *pstPriv;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    // 1 search destination frame
    if (WinBuf_FindDstFrame(pstWinBP, pstDstFrame) != HI_TRUE)
    {
	return HI_NULL;
    }

    // 2 get private information of dst frame
    pstDstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstDstFrame->u32Priv[0]);

    // 3 search dst frame
    do{
	// update position, 'pstWBNextNode' becomes current node, and get new next node
	nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstWBNode);
	DISP_ASSERT(nRet == HI_SUCCESS);

	pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
	pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);

	if (  (pstCurrFrame->u32FrameIndex == pstDstFrame->u32FrameIndex)
	    &&(pstCurrFrame->u32Pts	   == pstDstFrame->u32Pts)
	    &&(pstPriv->eOriginField	   == pstDstPriv->eOriginField)
	    )
	{
	    break;
	}

	// release current frame
	nRet = WinBuf_ReleaseOneFrame(pstWinBP, pstRefFrame);
	DISP_ASSERT(nRet == HI_SUCCESS);
    }while(nRet == HI_SUCCESS);

    // get node
    nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    // all node is clean exept the last one, now release it.
    pstWinBP->pstConfig = pstWBNode;

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
    WinBuf_DebugAddCfg(pstWinBP->pstDebugInfo, pstCurrFrame->u32FrameIndex);

#ifdef WIN_BUF_GetFrameByMaxID_PRINT
    pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);

    HI_PRINT("02 Dest=%d,tb=%d,ID=%d, tb=%d\n",
				  pstDstFrame->u32FrameIndex,
				  pstDstPriv->eOriginField,
				  pstCurrFrame->u32FrameIndex,
				  pstPriv->eOriginField);
#endif
    return pstCurrFrame;
}



//#define WIN_BUF_GetFrameByMaxID_PRINT
HI_DRV_VIDEO_FRAME_S * WinBuf_GetFrameByMaxID(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstRefFrame,HI_U32 u32RefID, HI_DRV_FIELD_MODE_E enDstField)
{
    DISP_BUF_NODE_S *pstWBNode, *pstWBNextNode;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame, *pstNextFrame;
    HI_DRV_VIDEO_PRIVATE_S *pstPriv;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

#ifdef WIN_BUF_GetFrameByMaxID_PRINT
    //printk("Dest ID=%d, tb=%d,", u32RefID, enDstField);
#endif

    nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstWBNode);
    if (nRet != HI_SUCCESS)
    {
#ifdef WIN_BUF_GetFrameByMaxID_PRINT
	//printk(".x.");
#endif
	return HI_NULL;
    }

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
    pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);

    if(u32RefID == 0)
    {
	// release node of current frame and add to empty array.
	nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
	DISP_ASSERT(nRet == HI_SUCCESS);

	pstWinBP->pstConfig = pstWBNode;

	// add to debug record array
	WinBuf_DebugAddCfg(pstWinBP->pstDebugInfo, pstCurrFrame->u32FrameIndex);

	return pstCurrFrame;
    }

    if (   (pstCurrFrame->u32FrameIndex == u32RefID)
	 &&(pstPriv->eOriginField == enDstField)
	)
    {
	// release node of current frame and add to empty array.
	nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
	DISP_ASSERT(nRet == HI_SUCCESS);

	pstWinBP->pstConfig = pstWBNode;

	// add to debug record array
	WinBuf_DebugAddCfg(pstWinBP->pstDebugInfo, pstCurrFrame->u32FrameIndex);

#ifdef WIN_BUF_GetFrameByMaxID_PRINT
	//printk("Find 01 ID=%d, tb=%d\n", pstCurrFrame->u32FrameIndex, pstPriv->eOriginField);
#endif
	return pstCurrFrame;
    }

    // if current frame ID is bigger than refID, reserve and not use now.
    if (pstCurrFrame->u32FrameIndex > u32RefID)
    {
#ifdef WIN_BUF_GetFrameByMaxID_PRINT
	printk("02 Dest=%d,tb=%d,ID=%d, tb=%d\n",
				  u32RefID,
				  enDstField,
				  pstCurrFrame->u32FrameIndex,
				  pstPriv->eOriginField);
#endif
	return HI_NULL;
    }

    nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
    while(nRet == HI_SUCCESS)
    {
	pstNextFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNextNode->u32Data;
	if (pstNextFrame->u32FrameIndex > u32RefID)
	{
#ifdef WIN_BUF_GetFrameByMaxID_PRINT
	    printk("N.");
#endif
	    break;
	}

	// release current frame
	WinBuf_ReleaseOneFrame(pstWinBP, pstRefFrame);

	pstWBNode = pstWBNextNode;

	pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
	pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);
	if (  (pstCurrFrame->u32FrameIndex == u32RefID)
	    &&(pstPriv->eOriginField == enDstField)
	    )
	{
#ifdef WIN_BUF_GetFrameByMaxID_PRINT
	    printk("Y.");
#endif
	    break;
	}

	// update position, 'pstWBNextNode' becomes current node, and get new next node
	nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
    }

    // get node
    nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    // all node is clean exept the last one, now release it.
    pstWinBP->pstConfig = pstWBNode;

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
    pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);

    WinBuf_DebugAddCfg(pstWinBP->pstDebugInfo, pstCurrFrame->u32FrameIndex);

#ifdef WIN_BUF_GetFrameByMaxID_PRINT
    printk("02 Dest=%d,tb=%d,ID=%d, tb=%d\n",
				  u32RefID,
				  enDstField,
				  pstCurrFrame->u32FrameIndex,
				  pstPriv->eOriginField);
#endif
    return pstCurrFrame;
}

//#define WIN_BUF_GetFrameByDisplayInfo_PRINT
HI_DRV_VIDEO_FRAME_S *WinBuf_GetFrameByDisplayInfo(WB_POOL_S *pstWinBP,
																HI_DRV_VIDEO_FRAME_S *pstRefFrame,
																HI_U32 u32RefRate,
																HI_DRV_FIELD_MODE_E enDstField)
{
    DISP_BUF_NODE_S *pstWBNode, *pstWBNextNode;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame;
    HI_DRV_VIDEO_PRIVATE_S *pstPriv;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
    printk("[df=%d]", enDstField);
#endif

    do
    {
	// get new frame
	nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstWBNode);
	if (nRet != HI_SUCCESS)
	{
#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
	    printk("U.");
#endif
	    return HI_NULL;
	}

	pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;
	pstPriv = (HI_DRV_VIDEO_PRIVATE_S *)&(pstCurrFrame->u32Priv[0]);

	// compare field and frame rate
	if ( (pstPriv->eOriginField == HI_DRV_FIELD_ALL)
	    ||(pstPriv->eOriginField == enDstField)
	    ||(abs((HI_S32)(pstCurrFrame->u32OriFrameRate/10) - (HI_S32)u32RefRate) >= 100)
	    )
	{
	    return WinBuf_GetConfigFrame(pstWinBP);
	}
	// process current frame according to sync info
	switch(pstCurrFrame->enTBAdjust)
	{
	    case HI_DRV_VIDEO_TB_REPEAT:
		// this frame may delay than audio
		pstCurrFrame->enTBAdjust = HI_DRV_VIDEO_TB_PLAY;
#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
		printk("R.");
#endif
		return HI_NULL;
	    case HI_DRV_VIDEO_TB_DISCARD:
		// this frame may advacne than audio, so get next
		nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
		printk("D.");
#endif
		if (nRet == HI_SUCCESS)
		{
#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
		    printk("DR.");
#endif
		    WinBuf_ReleaseOneFrame(pstWinBP, pstRefFrame);
		    break;
		}
		else
		{
#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
		    printk("DC.");
#endif
		    return WinBuf_GetConfigFrame(pstWinBP);
		}
	    case HI_DRV_VIDEO_TB_PLAY:
	    default:
		// this frame should not adjust
#ifdef WIN_BUF_GetFrameByDisplayInfo_PRINT
		printk("P.");
#endif
		return WinBuf_GetConfigFrame(pstWinBP);
	}

    }while(1);

    return HI_NULL;
}




HI_DRV_VIDEO_FRAME_S * WinBuf_GetNewestFrame(WB_POOL_S *pstWinBP, HI_DRV_VIDEO_FRAME_S *pstRefFrame)
{
    DISP_BUF_NODE_S *pstWBNode, *pstWBNextNode;
    HI_DRV_VIDEO_FRAME_S *pstCurrFrame, *pstNextFrame;
    HI_S32 nRet;

    WIN_CHECK_NULL_RETURN_NULL(pstWinBP);

    nRet = DispBuf_GetFullNode(&pstWinBP->stBuffer, &pstWBNode);
    if (nRet != HI_SUCCESS)
    {
	return HI_NULL;
    }

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;

    nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
    while(nRet == HI_SUCCESS)
    {
	pstNextFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNextNode->u32Data;

	WinBuf_ReleaseOneFrame(pstWinBP, pstRefFrame);

	// update position, 'pstWBNextNode' becomes current node, and get new next node
	pstWBNode = pstWBNextNode;

	nRet = DispBuf_GetNextFullNode(&pstWinBP->stBuffer, &pstWBNextNode);
    }

    // get node
    nRet = DispBuf_DelFullNode(&pstWinBP->stBuffer, pstWBNode);
    DISP_ASSERT(nRet == HI_SUCCESS);

    // all node is clean exept the last one, now release it.
    pstWinBP->pstConfig = pstWBNode;

    pstCurrFrame = (HI_DRV_VIDEO_FRAME_S *)pstWBNode->u32Data;

    WinBuf_DebugAddCfg(pstWinBP->pstDebugInfo, pstCurrFrame->u32FrameIndex);

    return pstCurrFrame;
}



HI_S32 WinBuf_GetFullBufNum(WB_POOL_S *pstWinBP, HI_U32 *pu32BufNum)
{
    WIN_CHECK_NULL_RETURN(pstWinBP);
    WIN_CHECK_NULL_RETURN(pu32BufNum);

    return DispBuf_GetFullNodeNumber(&pstWinBP->stBuffer, pu32BufNum);
}

HI_S32 WinBuf_GetStateInfo(WB_POOL_S *pstWinBP, WB_STATE_S *pstWinBufState)
{
    DISP_BUF_NODE_S *pstNode;
    HI_DRV_VIDEO_FRAME_S *pstFrame;
    HI_S32 i, nRet;

    WIN_CHECK_NULL_RETURN(pstWinBP);
    WIN_CHECK_NULL_RETURN(pstWinBufState);

    pstWinBufState->u32Number = pstWinBP->u32BufNumber;

    pstWinBufState->u32EmptyRPtr = pstWinBP->stBuffer.u32EmptyReadPos;
    pstWinBufState->u32EmptyWPtr = pstWinBP->stBuffer.u32EmptyWritePos;
    pstWinBufState->u32FullRPtr	 = pstWinBP->stBuffer.u32FullReaddPos;
    pstWinBufState->u32FullWPtr	 = pstWinBP->stBuffer.u32FullWritePos;

    for (i=0; i<pstWinBP->u32BufNumber; i++)
    {
	nRet = DispBuf_GetNodeContent(&pstWinBP->stBuffer, i, &pstNode);
	if ( nRet== HI_SUCCESS)
	{
	    pstWinBufState->stNode[i].u32State = pstNode->u32State;
	    pstWinBufState->stNode[i].u32Empty = pstNode->u32EmptyCount;
	    pstWinBufState->stNode[i].u32Full  = pstNode->u32FullCount;

	    pstFrame = (HI_DRV_VIDEO_FRAME_S *)pstNode->u32Data;
	    pstWinBufState->stNode[i].u32FrameIndex = pstFrame->u32FrameIndex;
	}
	else
	{
	    pstWinBufState->stNode[i].u32State = 0;
	    pstWinBufState->stNode[i].u32Empty = 0;
	    pstWinBufState->stNode[i].u32Full  = 0;
	    pstWinBufState->stNode[i].u32FrameIndex = 0;
	}
    }

    memcpy(&pstWinBufState->stRecord, pstWinBP->pstDebugInfo, sizeof(WB_DEBUG_INFO_S));

    memcpy(&pstWinBufState->u32EmptyArray, pstWinBP->stBuffer.u32EmptyArray, sizeof(HI_U32)*DISP_BUF_NODE_MAX_NUMBER);
    memcpy(&pstWinBufState->u32FullArray, pstWinBP->stBuffer.u32FullArray, sizeof(HI_U32)*DISP_BUF_NODE_MAX_NUMBER);

    pstFrame = WinBuf_GetDisplayedFrame(pstWinBP);
    if (pstFrame)
    {
	memcpy(&pstWinBufState->stCurrentFrame, pstFrame, sizeof(HI_DRV_VIDEO_FRAME_S));
    }

    return HI_SUCCESS;
}
