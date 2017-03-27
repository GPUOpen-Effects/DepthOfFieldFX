//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <assert.h>
#include <d3d11_1.h>
#include <fstream>
#include <string>

#if AMD_DEPTHOFFEILDFX_COMPILE_DYNAMIC_LIB
#define AMD_DLL_EXPORTS
#endif

#include "AMD_DepthOfFieldFX_OPAQUE.h"
#include "AMD_DepthOfFieldFX_Precompiled.h"

#pragma warning(disable : 4100)  // disable unreference formal parameter warnings for /W4 builds

namespace AMD {
template <typename type> void SAFE_RELEASE(type** a)
{
    if (*a != nullptr)
    {
        (*a)->Release();
        (*a) = nullptr;
    }
}

#define ELEMENTS_OF(x) (sizeof(x) / sizeof(x[0]))

struct float4
{
    float x;
    float y;
    float z;
    float w;
};

struct float2
{
    float2(float x_, float y_) : x(x_), y(y_) {}
    float x;
    float y;
};

struct int2
{
    int x;
    int y;
};

struct int4
{
    int x;
    int y;
    int z;
    int w;
};

struct dofParams
{
    int2   sourceResolution;
    float2 invSourceResolution;
    int2   bufferResolution;
    float  scale_factor;
    int    padding;
    int4   bartlettData[9];
    int4   boxBartlettData[4];
};

static const int4 s_bartlettData[9] = {
    { -1, -1, 1, 0 }, { 0, -1, -2, 0 }, { 1, -1, 1, 0 }, { -1, 0, -2, 0 }, { 0, 0, 4, 0 }, { 1, 0, -2, 0 }, { -1, 1, 1, 0 }, { 0, 1, -2, 0 }, { 1, 1, 1, 0 },
};

static const int4 s_boxBartlettData[4] = {
    { -1, -1, 1, 0 }, { 1, -1, -1, 0 }, { -1, 1, -1, 0 }, { 1, 1, 1, 0 },
};

DEPTHOFFIELDFX_OPAQUE_DESC::DEPTHOFFIELDFX_OPAQUE_DESC(const DEPTHOFFIELDFX_DESC& desc)
    : m_pIntermediateBuffer(nullptr)
    , m_pIntermediateBufferTransposed(nullptr)
    , m_pIntermediateUAV(nullptr)
    , m_pIntermediateTransposedUAV(nullptr)
    , m_pDofParamsCB(nullptr)
    , m_pPointSampler(nullptr)
    , m_pFastFilterSetupCS(nullptr)
    , m_pFastFilterSetupQuarterResCS(nullptr)
    , m_pBoxFastFilterSetupCS(nullptr)
    , m_pReadFinalResultCS(nullptr)
    , m_pVerticalIntegrateCS(nullptr)
    , m_pDoubleVerticalIntegrateCS(nullptr)

{
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::initalize(const DEPTHOFFIELDFX_DESC& desc)
{
    DEPTHOFFIELDFX_RETURN_CODE result = create_shaders(desc);
    ID3D11Device*              pDev   = desc.m_pDevice;

    if (result == DEPTHOFFIELDFX_RETURN_CODE_SUCCESS)
    {
        // initalize
        D3D11_BUFFER_DESC bdesc = { 0 };
        bdesc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        bdesc.Usage             = D3D11_USAGE_DYNAMIC;
        bdesc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
        bdesc.ByteWidth         = sizeof(dofParams);
        _ASSERT(sizeof(dofParams) % 16 == 0);
        HRESULT hr = pDev->CreateBuffer(&bdesc, nullptr, &m_pDofParamsCB);
        result     = convert_result(hr);
    }

    if (result == DEPTHOFFIELDFX_RETURN_CODE_SUCCESS)
    {
        D3D11_SAMPLER_DESC sdesc = {};
        sdesc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
        sdesc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
        sdesc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
        sdesc.ComparisonFunc     = D3D11_COMPARISON_ALWAYS;
        sdesc.Filter             = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        HRESULT hr               = pDev->CreateSamplerState(&sdesc, &m_pPointSampler);
        result                   = convert_result(hr);
    }

    return result;
}


DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::resize(const DEPTHOFFIELDFX_DESC& desc)
{
    HRESULT result = S_OK;

    ID3D11Device* pDev = desc.m_pDevice;

    int width      = desc.m_screenSize.x;
    int height     = desc.m_screenSize.y;
    m_padding      = desc.m_maxBlurRadius + 2;
    m_bufferWidth  = width + 2 * m_padding;
    m_bufferHeight = height + 2 * m_padding;

    SAFE_RELEASE(&m_pIntermediateUAV);
    SAFE_RELEASE(&m_pIntermediateTransposedUAV);
    SAFE_RELEASE(&m_pIntermediateBuffer);
    SAFE_RELEASE(&m_pIntermediateBufferTransposed);

    if (result == S_OK)
    {
        const uint elementCount = m_bufferWidth * m_bufferHeight;

        D3D11_BUFFER_DESC bdesc   = { 0 };
        bdesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        bdesc.Usage               = D3D11_USAGE_DEFAULT;
        bdesc.ByteWidth           = elementCount * sizeof(uint4);
        bdesc.StructureByteStride = sizeof(uint4);
        bdesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        memset(&uavDesc, 0, sizeof(uavDesc));
        uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Format              = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements  = elementCount;
        // uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

        result = pDev->CreateBuffer(&bdesc, nullptr, &m_pIntermediateBuffer);
        pDev->CreateUnorderedAccessView(m_pIntermediateBuffer, &uavDesc, &m_pIntermediateUAV);
        result = pDev->CreateBuffer(&bdesc, nullptr, &m_pIntermediateBufferTransposed);
        pDev->CreateUnorderedAccessView(m_pIntermediateBufferTransposed, &uavDesc, &m_pIntermediateTransposedUAV);
    }

    return convert_result(result);
}

void DEPTHOFFIELDFX_OPAQUE_DESC::Bind_UAVs(const DEPTHOFFIELDFX_DESC& desc, ID3D11UnorderedAccessView* pUAV0, ID3D11UnorderedAccessView* pUAV1, ID3D11UnorderedAccessView* pUAV2)
{
    ID3D11UnorderedAccessView* pUAVs[] = { pUAV0, pUAV1, pUAV2 };
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, 3, pUAVs, nullptr);
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::render(const DEPTHOFFIELDFX_DESC& desc)
{
    HRESULT result = S_OK;


    if (((desc.m_screenSize.x + desc.m_maxBlurRadius * 2) * (desc.m_screenSize.y + desc.m_maxBlurRadius * 2)) > (m_bufferWidth * m_bufferHeight))
    {
        return DEPTHOFFIELDFX_RETURN_CODE_INVALID_SURFACE;
    }

    ID3D11DeviceContext* pCtx = desc.m_pDeviceContext;

    ID3D11UnorderedAccessView* pUAVs[] = { m_pIntermediateUAV, nullptr, desc.m_pResultUAV };
    ID3D11ShaderResourceView*  pSRVs[] = { desc.m_pColorSRV, desc.m_pCircleOfConfusionSRV };
    ID3D11Buffer*              pCBs[]  = { m_pDofParamsCB };

    pCtx->CSSetSamplers(0, 1, &m_pPointSampler);

    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
    pCtx->CSSetConstantBuffers(0, ELEMENTS_OF(pCBs), pCBs);
    pCtx->CSSetShaderResources(0, ELEMENTS_OF(pSRVs), pSRVs);

    // clear intermediate buffer
    UINT clearValues[4] = { 0 };
    pCtx->ClearUnorderedAccessViewUint(m_pIntermediateUAV, clearValues);

    // Fast Filter Setup
    pCtx->CSSetShader(m_pFastFilterSetupCS, nullptr, 0);

    int tgX = (desc.m_screenSize.x + 7) / 8;
    int tgY = (desc.m_screenSize.y + 7) / 8;

    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
    Bind_UAVs(desc, m_pIntermediateUAV, nullptr, nullptr);
    pCtx->Dispatch(tgX, tgY, 1);

    // do Vertical integration
    {
        update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
        pCtx->CSSetShader(m_pDoubleVerticalIntegrateCS, nullptr, 0);
        Bind_UAVs(desc, m_pIntermediateUAV, m_pIntermediateTransposedUAV, nullptr);
        pCtx->Dispatch((m_bufferWidth + 63) / 64, 1, 1);
    }

    // do vertical integration by transposing the image and doing horizontal integration again
    {
        update_constant_buffer(desc, m_bufferHeight, m_bufferWidth);
        Bind_UAVs(desc, m_pIntermediateTransposedUAV, m_pIntermediateUAV, nullptr);
        pCtx->Dispatch((m_bufferHeight + 63) / 64, 1, 1);
    }

    // debug: Copy from intermediate results
    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);

    pCtx->CSSetShader(m_pReadFinalResultCS, nullptr, 0);
    Bind_UAVs(desc, m_pIntermediateUAV, nullptr, desc.m_pResultUAV);
    pCtx->Dispatch((desc.m_screenSize.x + 7) / 8, (desc.m_screenSize.y + 7) / 8, 1);

    memset(pUAVs, 0, sizeof(pUAVs));
    memset(pSRVs, 0, sizeof(pSRVs));
    memset(pCBs, 0, sizeof(pCBs));
    pCtx->CSSetUnorderedAccessViews(0, ELEMENTS_OF(pUAVs), pUAVs, nullptr);
    pCtx->CSSetShaderResources(0, ELEMENTS_OF(pSRVs), pSRVs);
    pCtx->CSSetConstantBuffers(0, ELEMENTS_OF(pCBs), pCBs);

    return convert_result(result);
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::render_quarter_res(const DEPTHOFFIELDFX_DESC& desc)
{
    HRESULT result = S_OK;

    ID3D11DeviceContext* pCtx = desc.m_pDeviceContext;

    ID3D11UnorderedAccessView* pUAVs[] = { m_pIntermediateUAV, nullptr, desc.m_pResultUAV };
    ID3D11ShaderResourceView*  pSRVs[] = { desc.m_pColorSRV, desc.m_pCircleOfConfusionSRV };
    ID3D11Buffer*              pCBs[]  = { m_pDofParamsCB };

    pCtx->CSSetSamplers(0, 1, &m_pPointSampler);

    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
    pCtx->CSSetConstantBuffers(0, ELEMENTS_OF(pCBs), pCBs);
    pCtx->CSSetShaderResources(0, ELEMENTS_OF(pSRVs), pSRVs);

    // clear intermediate buffer
    UINT clearValues[4] = { 0 };
    pCtx->ClearUnorderedAccessViewUint(m_pIntermediateUAV, clearValues);

    // Fast Filter Setup
    pCtx->CSSetShader(m_pFastFilterSetupQuarterResCS, nullptr, 0);

    int tgX = ((desc.m_screenSize.x / 2) + 7) / 8;
    int tgY = ((desc.m_screenSize.y / 2) + 7) / 8;

    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
    Bind_UAVs(desc, m_pIntermediateUAV, nullptr, nullptr);
    pCtx->Dispatch(tgX, tgY, 1);

    // do Vertical integration
    {
        update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
        pCtx->CSSetShader(m_pDoubleVerticalIntegrateCS, nullptr, 0);
        Bind_UAVs(desc, m_pIntermediateUAV, m_pIntermediateTransposedUAV, nullptr);
        pCtx->Dispatch((m_bufferWidth + 63) / 64, 1, 1);
    }

    // do vertical integration by transposing the image and doing horizontal integration again
    {
        update_constant_buffer(desc, m_bufferHeight, m_bufferWidth);
        Bind_UAVs(desc, m_pIntermediateTransposedUAV, m_pIntermediateUAV, nullptr);
        pCtx->Dispatch((m_bufferHeight + 63) / 64, 1, 1);
    }

    // debug: Copy from intermediate results
    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);

    pCtx->CSSetShader(m_pReadFinalResultCS, nullptr, 0);
    Bind_UAVs(desc, m_pIntermediateUAV, nullptr, desc.m_pResultUAV);
    pCtx->Dispatch((desc.m_screenSize.x + 7) / 8, (desc.m_screenSize.y + 7) / 8, 1);

    memset(pUAVs, 0, sizeof(pUAVs));
    memset(pSRVs, 0, sizeof(pSRVs));
    memset(pCBs, 0, sizeof(pCBs));
    pCtx->CSSetUnorderedAccessViews(0, ELEMENTS_OF(pUAVs), pUAVs, nullptr);
    pCtx->CSSetShaderResources(0, ELEMENTS_OF(pSRVs), pSRVs);
    pCtx->CSSetConstantBuffers(0, ELEMENTS_OF(pCBs), pCBs);

    return convert_result(result);
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::render_box(const DEPTHOFFIELDFX_DESC& desc)
{
    HRESULT result = S_OK;

    ID3D11DeviceContext* pCtx = desc.m_pDeviceContext;

    ID3D11UnorderedAccessView* pUAVs[] = { m_pIntermediateUAV, nullptr, desc.m_pResultUAV };
    ID3D11ShaderResourceView*  pSRVs[] = { desc.m_pColorSRV, desc.m_pCircleOfConfusionSRV };
    ID3D11Buffer*              pCBs[]  = { m_pDofParamsCB };

    pCtx->CSSetSamplers(0, 1, &m_pPointSampler);

    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
    pCtx->CSSetConstantBuffers(0, ELEMENTS_OF(pCBs), pCBs);
    pCtx->CSSetShaderResources(0, ELEMENTS_OF(pSRVs), pSRVs);

    // clear intermediate buffer
    UINT clearValues[4] = { 0 };
    pCtx->ClearUnorderedAccessViewUint(m_pIntermediateUAV, clearValues);

    // Fast Filter Setup
    pCtx->CSSetShader(m_pBoxFastFilterSetupCS, nullptr, 0);

    int tgX = (desc.m_screenSize.x + 7) / 8;
    int tgY = (desc.m_screenSize.y + 7) / 8;

    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
    Bind_UAVs(desc, m_pIntermediateUAV, nullptr, nullptr);
    pCtx->Dispatch(tgX, tgY, 1);

    // do Vertical integration
    {
        update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);
        pCtx->CSSetShader(m_pVerticalIntegrateCS, nullptr, 0);
        Bind_UAVs(desc, m_pIntermediateUAV, m_pIntermediateTransposedUAV, nullptr);
        pCtx->Dispatch((m_bufferWidth + 63) / 64, 1, 1);
    }

    // do vertical integration by transposing the image and doing horizontal integration again
    {
        update_constant_buffer(desc, m_bufferHeight, m_bufferWidth);
        Bind_UAVs(desc, m_pIntermediateTransposedUAV, m_pIntermediateUAV, nullptr);
        pCtx->Dispatch((m_bufferHeight + 63) / 64, 1, 1);
    }

    // debug: Copy from intermediate results
    update_constant_buffer(desc, m_bufferWidth, m_bufferHeight);

    pCtx->CSSetShader(m_pReadFinalResultCS, nullptr, 0);
    Bind_UAVs(desc, m_pIntermediateUAV, nullptr, desc.m_pResultUAV);
    pCtx->Dispatch((desc.m_screenSize.x + 7) / 8, (desc.m_screenSize.y + 7) / 8, 1);

    memset(pUAVs, 0, sizeof(pUAVs));
    memset(pSRVs, 0, sizeof(pSRVs));
    memset(pCBs, 0, sizeof(pCBs));
    pCtx->CSSetUnorderedAccessViews(0, ELEMENTS_OF(pUAVs), pUAVs, nullptr);
    pCtx->CSSetShaderResources(0, ELEMENTS_OF(pSRVs), pSRVs);
    pCtx->CSSetConstantBuffers(0, ELEMENTS_OF(pCBs), pCBs);

    return convert_result(result);
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::release()
{
    DEPTHOFFIELDFX_RETURN_CODE result = DEPTHOFFIELDFX_RETURN_CODE_SUCCESS;
    SAFE_RELEASE(&m_pIntermediateBuffer);
    SAFE_RELEASE(&m_pIntermediateBufferTransposed);
    SAFE_RELEASE(&m_pIntermediateUAV);
    SAFE_RELEASE(&m_pIntermediateTransposedUAV);
    SAFE_RELEASE(&m_pDofParamsCB);
    SAFE_RELEASE(&m_pPointSampler);
    SAFE_RELEASE(&m_pFastFilterSetupCS);
    SAFE_RELEASE(&m_pFastFilterSetupQuarterResCS);
    SAFE_RELEASE(&m_pBoxFastFilterSetupCS);
    SAFE_RELEASE(&m_pReadFinalResultCS);
    SAFE_RELEASE(&m_pVerticalIntegrateCS);
    SAFE_RELEASE(&m_pDoubleVerticalIntegrateCS);
    return result;
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::create_shaders(const DEPTHOFFIELDFX_DESC& desc)
{
    // ID3D11Device* pDevice = desc.m_pDevice;
    HRESULT result = S_OK;

    ID3D11Device* pDev = desc.m_pDevice;

    if (result == S_OK)
    {
        result = pDev->CreateComputeShader(g_csFastFilterSetup, sizeof(g_csFastFilterSetup), nullptr, &m_pFastFilterSetupCS);
    }
    if (result == S_OK)
    {
        result = pDev->CreateComputeShader(g_csFastFilterSetupQuarterRes, sizeof(g_csFastFilterSetupQuarterRes), nullptr, &m_pFastFilterSetupQuarterResCS);
    }
    if (result == S_OK)
    {
        result = pDev->CreateComputeShader(g_csVerticalIntegrate, sizeof(g_csVerticalIntegrate), nullptr, &m_pVerticalIntegrateCS);
    }
    if (result == S_OK)
    {
        result = pDev->CreateComputeShader(g_csReadFinalResult, sizeof(g_csReadFinalResult), nullptr, &m_pReadFinalResultCS);
    }
    if (result == S_OK)
    {
        result = pDev->CreateComputeShader(g_csBoxFastFilterSetup, sizeof(g_csBoxFastFilterSetup), nullptr, &m_pBoxFastFilterSetupCS);
    }
    if (result == S_OK)
    {
        result = pDev->CreateComputeShader(g_csDoubleVerticalIntegrate, sizeof(g_csDoubleVerticalIntegrate), nullptr, &m_pDoubleVerticalIntegrateCS);
    }


    return convert_result(result);
}

DEPTHOFFIELDFX_RETURN_CODE DEPTHOFFIELDFX_OPAQUE_DESC::convert_result(HRESULT hResult)
{
    DEPTHOFFIELDFX_RETURN_CODE result = DEPTHOFFIELDFX_RETURN_CODE_SUCCESS;

    if (hResult != S_OK)
    {
        result = DEPTHOFFIELDFX_RETURN_CODE_FAIL;
    }
    return result;
}

BOOL DEPTHOFFIELDFX_OPAQUE_DESC::update_constant_buffer(const DEPTHOFFIELDFX_DESC& desc, uint padWidth, uint padHeight)
{
    ID3D11DeviceContext*     pCtx = desc.m_pDeviceContext;
    D3D11_MAPPED_SUBRESOURCE data;
    pCtx->Map(m_pDofParamsCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &data);

    if (data.pData != nullptr)
    {
        dofParams* pParams             = static_cast<dofParams*>(data.pData);
        pParams->bufferResolution.x    = padWidth;
        pParams->bufferResolution.y    = padHeight;
        pParams->sourceResolution.x    = desc.m_screenSize.x;
        pParams->sourceResolution.y    = desc.m_screenSize.y;
        pParams->invSourceResolution.x = 1.0f / static_cast<float>(desc.m_screenSize.x);
        pParams->invSourceResolution.y = 1.0f / static_cast<float>(desc.m_screenSize.y);
        pParams->padding               = desc.m_pOpaque->m_padding;
        pParams->scale_factor          = float(1 << desc.m_scaleFactor);
        memcpy(pParams->bartlettData, s_bartlettData, sizeof(dofParams::bartlettData));
        memcpy(pParams->boxBartlettData, s_boxBartlettData, sizeof(dofParams::boxBartlettData));

        pCtx->Unmap(m_pDofParamsCB, 0);
    }

    return TRUE;
}
}
