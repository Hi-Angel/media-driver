/*
* Copyright (c) 2022, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
//!
//! \file     media_blt_copy_xe_lpm_plus_base.cpp
//! \brief    Common interface used in Blitter Engine
//! \details  Common interface used in Blitter Engine which are platform independent
//!

#include "media_blt_copy_xe_lpm_plus_base.h"
#include "mhw_cp_interface.h"

//!
//! \brief    BltStateXe_Xpm_Plus constructor
//! \details  Initialize the BltStateXe_Xpm_Plus members.
//! \param    osInterface
//!           [in] Pointer to MOS_INTERFACE.
//!
BltStateXe_Lpm_Plus_Base::BltStateXe_Lpm_Plus_Base(PMOS_INTERFACE    osInterface) :
    BltStateNext(osInterface),
    initialized(false),
    allocated(false),
    tempSurface(nullptr),
    tempAuxSurface(nullptr),
    surfaceSize(0),
    auxSize(0),
    pMainSurface(nullptr),
    pAuxSurface(nullptr)
{
}

//!
//! \brief    BltStateXe_Xpm_Plus constructor
//! \details  Initialize the BltStateXe_Xpm_Plus members.
//! \param    osInterface
//!           [in] Pointer to MOS_INTERFACE.
//!
BltStateXe_Lpm_Plus_Base::BltStateXe_Lpm_Plus_Base(PMOS_INTERFACE    osInterface, MhwInterfacesNext *mhwInterfaces) :
    BltStateNext(osInterface, mhwInterfaces),
    initialized(false),
    allocated(false),
    tempSurface(nullptr),
    tempAuxSurface(nullptr),
    surfaceSize(0),
    auxSize(0),
    pMainSurface(nullptr),
    pAuxSurface(nullptr)
{

}


BltStateXe_Lpm_Plus_Base::~BltStateXe_Lpm_Plus_Base()
{
    FreeResource();
    if (pMainSurface)
    {
        MOS_FreeMemAndSetNull(pMainSurface);
    }
    if (pAuxSurface)
    {
        MOS_FreeMemAndSetNull(pAuxSurface);
    }
}

//!
//! \brief    BltStateXe_Xpm_Plus initialize
//! \details  Initialize the BltStateXe_Xpm_Plus, create BLT context.
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::Initialize()
{
    BltStateNext::Initialize();
    initialized = true;
    m_blokCopyon = true;
    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Get control surface
//! \details  BLT engine will copy aux data of source surface to destination
//! \param    src
//!           [in] Pointer to source surface
//! \param    dst
//!           [in] Pointer to destination buffer is created for aux data
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::GetCCS(
    PMOS_SURFACE src,
    PMOS_SURFACE dst)
{
    BLT_STATE_PARAM bltStateParam;

    BLT_CHK_NULL_RETURN(src);
    BLT_CHK_NULL_RETURN(dst);
    BLT_CHK_NULL_RETURN(&src->OsResource);
    BLT_CHK_NULL_RETURN(&dst->OsResource);

    MOS_ZeroMemory(&bltStateParam, sizeof(BLT_STATE_PARAM));
    bltStateParam.bCopyCCS = true;
    bltStateParam.ccsFlag  = CCS_READ;
    bltStateParam.pSrcCCS  = src;
    bltStateParam.pDstCCS  = dst;

    BLT_CHK_STATUS_RETURN(SubmitCMD(&bltStateParam));

    // sync
    MOS_LOCK_PARAMS flag;
    flag.Value     = 0;
    flag.WriteOnly = 1;
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnLockSyncRequest(m_osInterface, &dst->OsResource, &flag));

    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Put control surface
//! \details  BLT engine will copy aux data of source surface to destination
//! \param    src
//!           [in] Pointer to source surface
//! \param    dst
//!           [in] Pointer to destination buffer is created for aux data
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::PutCCS(
    PMOS_SURFACE src,
    PMOS_SURFACE dst)
{
    BLT_STATE_PARAM bltStateParam;

    BLT_CHK_NULL_RETURN(src);
    BLT_CHK_NULL_RETURN(dst);
    BLT_CHK_NULL_RETURN(&src->OsResource);
    BLT_CHK_NULL_RETURN(&dst->OsResource);

    MOS_ZeroMemory(&bltStateParam, sizeof(BLT_STATE_PARAM));
    bltStateParam.bCopyCCS = true;
    bltStateParam.ccsFlag  = CCS_WRITE;
    bltStateParam.pSrcCCS  = src;
    bltStateParam.pDstCCS  = dst;

    BLT_CHK_STATUS_RETURN(SubmitCMD(&bltStateParam));

    // sync
    MOS_LOCK_PARAMS flag;
    flag.Value     = 0;
    flag.WriteOnly = 1;
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnLockSyncRequest(m_osInterface, &dst->OsResource, &flag));

    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Lock surface
//! \details  Lock surface to get main surface and aux data
//! \param    pSrcSurface
//!           [in] Pointer to source surface
//! \return   MOS_STATUS
//!           MOS_STATUS_SUCCESS if success, otherwise error code
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::LockSurface(
    PMOS_SURFACE pSurface)
{
    MOS_STATUS eStatus    = MOS_STATUS_SUCCESS;
    void*      pTemp      = nullptr;
    do
    {
        if (pSurface==nullptr)
        {
            BLT_ASSERTMESSAGE("BLT: pSurface check nullptr fail in LockSurface.")
            break;
        }

        // Initialize for the first time
        if (!initialized)
        {
            if (Initialize() != MOS_STATUS_SUCCESS)
            {
                break;
            }
        }

        // Allocate internel resource
        if (AllocateResource (pSurface) != MOS_STATUS_SUCCESS)
        {
            break;
        }

        // Get main surface and CCS
        // Currentlt main surface copy will cause page fault, which cause crash.
        // BLT_CHK_STATUS(CopyMainSurface(pSurface, tempSurface));
        if (GetCCS(pSurface, tempAuxSurface) != MOS_STATUS_SUCCESS)
        {
            break;
        }

        MOS_LOCK_PARAMS LockFlags;
        LockFlags.Value        = 0;
        LockFlags.ReadOnly     = 1;
        LockFlags.TiledAsTiled = 1;
        LockFlags.NoDecompress = 1;

        // Lock main surface data
        pTemp = (uint8_t *)m_osInterface->pfnLockResource(
            m_osInterface,
            &pSurface->OsResource,
            &LockFlags);
        if (pTemp == nullptr)
        {
            break;
        }

        MOS_SecureMemcpy(
            pMainSurface,
            surfaceSize,
            pTemp,
            surfaceSize);
        if (m_osInterface->pfnUnlockResource(m_osInterface, &pSurface->OsResource) != MOS_STATUS_SUCCESS)
        {
            break;
        }

        // Lock CCS data
        pTemp = (uint8_t *)m_osInterface->pfnLockResource(
            m_osInterface,
            &tempAuxSurface->OsResource,
            &LockFlags);
        if (pTemp == nullptr)
        {
            break;
        }

        MOS_SecureMemcpy(
            pAuxSurface,
            auxSize,
            pTemp,
            auxSize);
        if (m_osInterface->pfnUnlockResource(m_osInterface, &tempAuxSurface->OsResource))
        {
            break;
        }

        return eStatus;
    } while (false);

    BLT_ASSERTMESSAGE("BLT: Lock surface failed.");
    FreeResource();
    return eStatus;
}

//!
//! \brief    Unlock surface
//! \details  Free resource created by lockSurface, must be called once call LockSurface
//! \return   MOS_STATUS
//!           MOS_STATUS_SUCCESS if success, otherwise error code
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::UnLockSurface()
{
    FreeResource();
    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Write compressed surface
//! \details  Write compressed surface data from system memory to GPU memory
//! \param    pSysMemory
//!           [in] Pointer to system memory
//! \param    dataSize
//!           [in] data size, including main surface data and aux data
//! \param    pSurface
//!           [in] Pointer to the destination surface
//! \return   MOS_STATUS
//!           MOS_STATUS_SUCCESS if success, otherwise error code
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::WriteCompressedSurface(
    void*        pSysMemory,
    uint32_t     dataSize,
    PMOS_SURFACE pSurface)
{
    MOS_STATUS eStatus  = MOS_STATUS_SUCCESS;
    void*      pTemp    = nullptr;
    uint32_t   sizeAux  = 0;
    do
    {
        if (pSurface == nullptr)
        {
            BLT_ASSERTMESSAGE("BLT: pSurface check nullptr fail in WriteCompressedSurface.")
            break;
        }

        // Initialize for the first time
        if (!initialized)
        {
            if (Initialize() != MOS_STATUS_SUCCESS)
            {
                break;
            }
        }

        // Allocate internel resource
        if (AllocateResource(pSurface) != MOS_STATUS_SUCCESS)
        {
            break;
        }

        sizeAux = dataSize / 257;

        MOS_LOCK_PARAMS LockFlags;
        LockFlags.Value        = 0;
        LockFlags.WriteOnly    = 1;
        LockFlags.TiledAsTiled = 1;
        LockFlags.NoDecompress = 1;

        // Lock temp main surface
        pTemp = (uint32_t *)m_osInterface->pfnLockResource(
            m_osInterface,
            &pSurface->OsResource,
            &LockFlags);
        // copy surface data to temp surface
        MOS_SecureMemcpy(
            pTemp,
            sizeAux * 256,
            pSysMemory,
            sizeAux * 256);
        if (m_osInterface->pfnUnlockResource(m_osInterface, &pSurface->OsResource) != MOS_STATUS_SUCCESS)
        {
            break;
        }

        // Lock temp aux surface
        pTemp = (uint8_t *)m_osInterface->pfnLockResource(
            m_osInterface,
            &tempAuxSurface->OsResource,
            &LockFlags);
        // copy aux data to temp aux surface
        MOS_SecureMemcpy(
            pTemp,
            sizeAux,
            (uint8_t *)pSysMemory + sizeAux * 256,
            sizeAux);
        if (m_osInterface->pfnUnlockResource(m_osInterface, &tempAuxSurface->OsResource) != MOS_STATUS_SUCCESS)
        {
            break;
        }
        BLT_CHK_STATUS_RETURN(PutCCS(tempAuxSurface, pSurface));

        FreeResource();
        return eStatus;
    } while (false);

    BLT_ASSERTMESSAGE("BLT: Write compressed surface failed.");
    FreeResource();
    return eStatus;
}

//!
//! \brief    Allocate resource
//! \details  Allocate internel resource
//! \param    pSrcSurface
//!           [in] Pointer to source surface
//! \return   MOS_STATUS
//!           MOS_STATUS_SUCCESS if success, otherwise error code
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::AllocateResource(
    PMOS_SURFACE pSurface)
{
    MOS_ALLOC_GFXRES_PARAMS AllocParams;

    tempSurface = (PMOS_SURFACE)MOS_AllocAndZeroMemory(sizeof(MOS_SURFACE));
    tempAuxSurface     = (PMOS_SURFACE)MOS_AllocAndZeroMemory(sizeof(MOS_SURFACE));
    BLT_CHK_NULL_RETURN(tempSurface);
    BLT_CHK_NULL_RETURN(tempAuxSurface);

    // Always allocate the temp surface as compressible surface to make sure the size is correct.
    MOS_ZeroMemory(&AllocParams, sizeof(MOS_ALLOC_GFXRES_PARAMS));
    AllocParams.TileType        = pSurface->TileType;
    AllocParams.Type            = MOS_GFXRES_2D;
    AllocParams.dwWidth         = pSurface->dwWidth;
    AllocParams.dwHeight        = pSurface->dwHeight;
    AllocParams.Format          = pSurface->Format;
    AllocParams.bIsCompressible = true;
    AllocParams.CompressionMode = pSurface->CompressionMode;
    AllocParams.pBufName        = "TempOutSurface";
    AllocParams.dwArraySize     = 1;

    BLT_CHK_STATUS_RETURN(m_osInterface->pfnAllocateResource(
        m_osInterface,
        &AllocParams,
        &tempSurface->OsResource));

    tempSurface->dwPitch = pSurface->dwPitch;
    tempSurface->dwWidth = pSurface->dwWidth;
    tempSurface->dwHeight = pSurface->dwHeight;
    tempSurface->Format   = pSurface->Format;
    tempSurface->TileType = pSurface->TileType;

    MOS_ZeroMemory(&AllocParams, sizeof(MOS_ALLOC_GFXRES_PARAMS));
    AllocParams.TileType        = MOS_TILE_LINEAR;
    AllocParams.Type            = MOS_GFXRES_BUFFER;
    AllocParams.dwWidth         = (uint32_t)tempSurface->OsResource.pGmmResInfo->GetSizeMainSurface() / 256;
    AllocParams.dwHeight        = 1;
    AllocParams.Format          = Format_Buffer;
    AllocParams.bIsCompressible = false;
    AllocParams.CompressionMode = MOS_MMC_DISABLED;
    AllocParams.pBufName        = "TempCCS";
    AllocParams.dwArraySize     = 1;

    BLT_CHK_STATUS_RETURN(m_osInterface->pfnAllocateResource(
        m_osInterface,
        &AllocParams,
        &tempAuxSurface->OsResource));

    surfaceSize  = (uint32_t)tempSurface->OsResource.pGmmResInfo->GetSizeMainSurface();
    auxSize      = surfaceSize / 256;
    pMainSurface = MOS_AllocAndZeroMemory(surfaceSize);
    pAuxSurface  = MOS_AllocAndZeroMemory(auxSize);
    BLT_CHK_NULL_RETURN(pMainSurface);
    BLT_CHK_NULL_RETURN(pAuxSurface);

    allocated    = true;

    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Free resource
//! \details  Free internel resource, must be called once call AllocateResource
//! \return   MOS_STATUS
//!           MOS_STATUS_SUCCESS if success, otherwise error code
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::FreeResource()
{
    if (allocated)
    {
        m_osInterface->pfnFreeResource(m_osInterface, &tempSurface->OsResource);
        m_osInterface->pfnFreeResource(m_osInterface, &tempAuxSurface->OsResource);
        allocated = false;
    }
    if (tempSurface)
    {
        MOS_FreeMemAndSetNull(tempSurface);
    }
    if (tempAuxSurface)
    {
        MOS_FreeMemAndSetNull(tempAuxSurface);
    }

    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Setup control surface copy parameters
//! \details  Setup control surface copy parameters for BLT Engine
//! \param    mhwParams
//!           [in/out] Pointer to MHW_CTRL_SURF_COPY_BLT_PARAM
//! \param    inputSurface
//!           [in] Pointer to input surface
//! \param    outputSurface
//!           [in] Pointer to output surface
//! \param    flag
//!           [in] Flag for read/write CCS
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::SetupCtrlSurfCopyBltParam(
    PMHW_CTRL_SURF_COPY_BLT_PARAM pMhwBltParams,
    PMOS_SURFACE                  inputSurface,
    PMOS_SURFACE                  outputSurface,
    uint32_t                      flag)
{
    BLT_CHK_NULL_RETURN(pMhwBltParams);
    BLT_CHK_NULL_RETURN(inputSurface);
    BLT_CHK_NULL_RETURN(outputSurface);

    if (flag == CCS_READ)
    {
        pMhwBltParams->dwSrcMemoryType = 0;
        pMhwBltParams->dwDstMemoryType = 1;
        pMhwBltParams->dwSizeofControlSurface = (uint32_t)inputSurface->OsResource.pGmmResInfo->GetSizeMainSurface() / 65536;
    }
    else
    {
        pMhwBltParams->dwSrcMemoryType = 1;
        pMhwBltParams->dwDstMemoryType = 0;
        pMhwBltParams->dwSizeofControlSurface = (uint32_t)outputSurface->OsResource.pGmmResInfo->GetSizeMainSurface() / 65536;
    }

    pMhwBltParams->pSrcOsResource  = &inputSurface->OsResource;
    pMhwBltParams->pDstOsResource  = &outputSurface->OsResource;

    return MOS_STATUS_SUCCESS;
}

//!
//! \brief    Submit command
//! \details  Submit BLT command
//! \param    pBltStateParam
//!           [in] Pointer to BLT_STATE_PARAM
//! \return   MOS_STATUS
//!           Return MOS_STATUS_SUCCESS if successful, otherwise failed
//!
MOS_STATUS BltStateXe_Lpm_Plus_Base::SubmitCMD(
    PBLT_STATE_PARAM pBltStateParam)
{
    MOS_COMMAND_BUFFER           cmdBuffer;
    MHW_FAST_COPY_BLT_PARAM      fastCopyBltParam;
    MHW_CTRL_SURF_COPY_BLT_PARAM ctrlSurfCopyBltParam;
    MOS_GPUCTX_CREATOPTIONS_ENHANCED createOption = {};
    int                          planeNum = 1;

    BLT_CHK_NULL_RETURN(m_miItf);
    BLT_CHK_NULL_RETURN(m_bltItf);
    // no gpucontext will be created if the gpu context has been created before.
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnCreateGpuContext(
        m_osInterface,
        MOS_GPU_CONTEXT_BLT,
        MOS_GPU_NODE_BLT,
        &createOption));
    // Set GPU context
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnSetGpuContext(m_osInterface, MOS_GPU_CONTEXT_BLT));

    // Initialize the command buffer struct
    MOS_ZeroMemory(&cmdBuffer, sizeof(MOS_COMMAND_BUFFER));
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnGetCommandBuffer(m_osInterface, &cmdBuffer, 0));

    MOS_SURFACE       srcResDetails;
    MOS_SURFACE       dstResDetails;
    MOS_ZeroMemory(&srcResDetails, sizeof(MOS_SURFACE));
    MOS_ZeroMemory(&dstResDetails, sizeof(MOS_SURFACE));
    srcResDetails.Format = Format_Invalid;
    dstResDetails.Format = Format_Invalid;
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnGetResourceInfo(m_osInterface, pBltStateParam->pSrcSurface, &srcResDetails));
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnGetResourceInfo(m_osInterface, pBltStateParam->pDstSurface, &dstResDetails));

    if (srcResDetails.Format != dstResDetails.Format)
    {
        MCPY_ASSERTMESSAGE("BLT copy can't support CSC copy. input format = %d, output format = %d", srcResDetails.Format, dstResDetails.Format);
        return MOS_STATUS_INVALID_PARAMETER;
    }
    planeNum = GetPlaneNum(dstResDetails.Format);

    if (pBltStateParam->bCopyMainSurface)
    {
        BLT_CHK_STATUS_RETURN(SetupBltCopyParam(
            &fastCopyBltParam,
            pBltStateParam->pSrcSurface,
            pBltStateParam->pDstSurface,
            0));

        auto& Register = m_miItf->MHW_GETPAR_F(MI_LOAD_REGISTER_IMM)();
        Register = {};
        Register.dwRegister = mhw_blt_state::BCS_SWCTRL_XE::REGISTER_OFFSET;
        mhw_blt_state::BCS_SWCTRL_XE swctrl;
        if (pBltStateParam->pSrcSurface->TileType != MOS_TILE_LINEAR)
        {
            swctrl.DW0.Tile4Source = 1;
        }
        if (pBltStateParam->pDstSurface->TileType != MOS_TILE_LINEAR)
        {//output tiled
            swctrl.DW0.Tile4Destination = 1;
        }
        Register.dwData = swctrl.DW0.Value;
        BLT_CHK_STATUS_RETURN(m_miItf->MHW_ADDCMD_F(MI_LOAD_REGISTER_IMM)(&cmdBuffer));

        if (m_blokCopyon)
        {
            BLT_CHK_STATUS_RETURN(m_bltItf->AddBlockCopyBlt(
                &cmdBuffer,
                &fastCopyBltParam,
                srcResDetails.YPlaneOffset.iSurfaceOffset,
                dstResDetails.YPlaneOffset.iSurfaceOffset));
        }
        else
        {
            BLT_CHK_STATUS_RETURN(m_bltItf->AddFastCopyBlt(
                &cmdBuffer,
                &fastCopyBltParam,
                srcResDetails.YPlaneOffset.iSurfaceOffset,
                dstResDetails.YPlaneOffset.iSurfaceOffset));
        }
        if (planeNum >= 2)
        {
            BLT_CHK_STATUS_RETURN(SetupBltCopyParam(
             &fastCopyBltParam,
             pBltStateParam->pSrcSurface,
             pBltStateParam->pDstSurface,
             1));
            if (m_blokCopyon)
            {
                BLT_CHK_STATUS_RETURN(m_bltItf->AddBlockCopyBlt(
                    &cmdBuffer,
                    &fastCopyBltParam,
                    srcResDetails.UPlaneOffset.iSurfaceOffset,
                    dstResDetails.UPlaneOffset.iSurfaceOffset));
            }
            else
            {
                BLT_CHK_STATUS_RETURN(m_bltItf->AddFastCopyBlt(
                    &cmdBuffer,
                    &fastCopyBltParam,
                    srcResDetails.UPlaneOffset.iSurfaceOffset,
                    dstResDetails.UPlaneOffset.iSurfaceOffset));
            }
            if (planeNum == 3)
            {
                BLT_CHK_STATUS_RETURN(SetupBltCopyParam(
                    &fastCopyBltParam,
                    pBltStateParam->pSrcSurface,
                    pBltStateParam->pDstSurface,
                    2));
                if (m_blokCopyon)
                {
                    BLT_CHK_STATUS_RETURN(m_bltItf->AddBlockCopyBlt(
                        &cmdBuffer,
                        &fastCopyBltParam,
                        srcResDetails.VPlaneOffset.iSurfaceOffset,
                        dstResDetails.VPlaneOffset.iSurfaceOffset));
                }
                else
                {
                    BLT_CHK_STATUS_RETURN(m_bltItf->AddFastCopyBlt(
                        &cmdBuffer,
                        &fastCopyBltParam,
                        srcResDetails.VPlaneOffset.iSurfaceOffset,
                        dstResDetails.VPlaneOffset.iSurfaceOffset));
                }
            }
            else if(planeNum > 3)
            {
                MCPY_ASSERTMESSAGE("illegal usage");
                return MOS_STATUS_INVALID_PARAMETER;
            }
         }
    }

    if (pBltStateParam->bCopyCCS)
    {
        BLT_CHK_STATUS_RETURN(SetupCtrlSurfCopyBltParam(
            &ctrlSurfCopyBltParam,
            pBltStateParam->pSrcCCS,
            pBltStateParam->pDstCCS,
            pBltStateParam->ccsFlag));
        //BLT_CHK_STATUS_RETURN(pbltInterfacePvc->AddCtrlSurfCopyBlt(
        //    &cmdBuffer,
        //    &ctrlSurfCopyBltParam));
    }

    // Add flush DW
    auto& flushDwParams = m_miItf->MHW_GETPAR_F(MI_FLUSH_DW)();
    flushDwParams = {};
    BLT_CHK_STATUS_RETURN(m_miItf->MHW_ADDCMD_F(MI_FLUSH_DW)(&cmdBuffer));

    // Add Batch Buffer end
    BLT_CHK_STATUS_RETURN(m_miItf->AddMiBatchBufferEnd(&cmdBuffer, nullptr));

    // Return unused command buffer space to OS
    m_osInterface->pfnReturnCommandBuffer(m_osInterface, &cmdBuffer, 0);

    // Flush the command buffer
    BLT_CHK_STATUS_RETURN(m_osInterface->pfnSubmitCommandBuffer(m_osInterface, &cmdBuffer, false));

    return MOS_STATUS_SUCCESS;
}

MOS_STATUS BltStateXe_Lpm_Plus_Base::CopyMainSurface(
    PMOS_RESOURCE src,
    PMOS_RESOURCE dst)
{
    BLT_STATE_PARAM bltStateParam;

    BLT_CHK_NULL_RETURN(src);
    BLT_CHK_NULL_RETURN(dst);
    MOS_TraceEventExt(EVENT_MEDIA_COPY, EVENT_TYPE_START, nullptr, 0, nullptr, 0);

    MOS_ZeroMemory(&bltStateParam, sizeof(BLT_STATE_PARAM));
    bltStateParam.bCopyMainSurface = true;
    bltStateParam.pSrcSurface      = src;
    bltStateParam.pDstSurface      = dst;

    BLT_CHK_STATUS_RETURN(SubmitCMD(&bltStateParam));

    MOS_TraceEventExt(EVENT_MEDIA_COPY, EVENT_TYPE_END, nullptr, 0, nullptr, 0);
    return MOS_STATUS_SUCCESS;
}

uint32_t BltStateXe_Lpm_Plus_Base::GetColorDepth(
    GMM_RESOURCE_FORMAT dstFormat,
    uint32_t            BytesPerTexel)
{
    uint32_t bitsPerTexel = BytesPerTexel * BLT_BITS_PER_BYTE;

    switch (bitsPerTexel)
    {
     case 8:
         return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_8BITCOLOR;
         break;
     case 16:
         switch (dstFormat)
         {
             case GMM_FORMAT_B5G5R5A1_UNORM:
                 return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_32BITCOLOR;
                 break;
             default:
                 return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_16BITCOLOR;;
                 break;
         }
         break;
     case 64:
         return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_64BITCOLOR;
         break;
     case 96:
         return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_96BITCOLOR_ONLYLINEARCASEISSUPPORTED;
         break;
     case 128:
         return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_128BITCOLOR;
         break;
     default:
         return mhw::blt::xe_lpm_plus_next::Cmd::XY_BLOCK_COPY_BLT_CMD::COLOR_DEPTH_32BITCOLOR;
         break;
    }

 }
