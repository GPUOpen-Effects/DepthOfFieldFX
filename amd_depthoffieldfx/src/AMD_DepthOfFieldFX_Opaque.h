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

#ifndef AMD_DEPTHOFFIELDFX_OPAQUE_H
#define AMD_DEPTHOFFIELDFX_OPAQUE_H

#include "AMD_DepthOfFieldFX.h"

#pragma warning(disable : 4127)  // disable conditional expression is constant warnings

namespace AMD {
struct DEPTHOFFIELDFX_OPAQUE_DESC
{
public:
#pragma warning(push)
#pragma warning(disable : 4201)  // suppress nameless struct/union level 4 warnings
    AMD_DECLARE_BASIC_VECTOR_TYPE;
#pragma warning(pop)
    DEPTHOFFIELDFX_OPAQUE_DESC(const DEPTHOFFIELDFX_DESC& desc);

    DEPTHOFFIELDFX_RETURN_CODE initalize(const DEPTHOFFIELDFX_DESC& desc);
    DEPTHOFFIELDFX_RETURN_CODE resize(const DEPTHOFFIELDFX_DESC& desc);
    DEPTHOFFIELDFX_RETURN_CODE render(const DEPTHOFFIELDFX_DESC& desc);
    DEPTHOFFIELDFX_RETURN_CODE render_quarter_res(const DEPTHOFFIELDFX_DESC& desc);
    DEPTHOFFIELDFX_RETURN_CODE render_box(const DEPTHOFFIELDFX_DESC& desc);
    DEPTHOFFIELDFX_RETURN_CODE release();

    DEPTHOFFIELDFX_RETURN_CODE create_shaders(const DEPTHOFFIELDFX_DESC& desc);
    static DEPTHOFFIELDFX_RETURN_CODE convert_result(HRESULT hResult);

    BOOL update_constant_buffer(const DEPTHOFFIELDFX_DESC& desc, uint padWidth, uint padHeight);
    void Bind_UAVs(const DEPTHOFFIELDFX_DESC& desc, ID3D11UnorderedAccessView* pUAV0, ID3D11UnorderedAccessView* pUAV1, ID3D11UnorderedAccessView* pUAV2);


    uint m_padding;
    uint m_bufferWidth;
    uint m_bufferHeight;

    ID3D11Buffer*              m_pIntermediateBuffer;
    ID3D11Buffer*              m_pIntermediateBufferTransposed;
    ID3D11UnorderedAccessView* m_pIntermediateUAV;
    ID3D11UnorderedAccessView* m_pIntermediateTransposedUAV;
    ID3D11Buffer*              m_pDofParamsCB;
    ID3D11SamplerState*        m_pPointSampler;

    ID3D11ComputeShader* m_pFastFilterSetupCS;
    ID3D11ComputeShader* m_pFastFilterSetupQuarterResCS;
    ID3D11ComputeShader* m_pBoxFastFilterSetupCS;
    ID3D11ComputeShader* m_pReadFinalResultCS;
    ID3D11ComputeShader* m_pVerticalIntegrateCS;
    ID3D11ComputeShader* m_pDoubleVerticalIntegrateCS;
};
};

#endif  // ndef AMD_DEPTHOFFIELDFX_OPAQUE_H
