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

// if the library is being compiled with "DYNAMIC_LIB" option
// it should do dclspec(dllexport)
#if AMD_DEPTHOFFIELDFX_COMPILE_DYNAMIC_LIB
#define AMD_DLL_EXPORT
#endif

#include <d3d11.h>

#include "AMD_DepthOfFieldFX.h"
#include "AMD_DepthOfFieldFX_Opaque.h"

#pragma warning(disable : 4100)  // disable unreference formal parameter warnings for /W4 builds

namespace AMD {
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_DESC::DEPTHOFFIELDFX_DESC() : m_pDevice(nullptr), m_pDeviceContext(nullptr), m_pCircleOfConfusionSRV(nullptr)
{
    static DEPTHOFFIELDFX_OPAQUE_DESC opaque(*this);
    m_pOpaque = &opaque;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_GetVersion(uint* major, uint* minor, uint* patch)
{
    *major = AMD_DEPTHOFFIELDFX_VERSION_MAJOR;
    *minor = AMD_DEPTHOFFIELDFX_VERSION_MINOR;
    *patch = AMD_DEPTHOFFIELDFX_VERSION_PATCH;
    return DEPTHOFFIELDFX_RETURN_CODE_SUCCESS;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Initialize(const DEPTHOFFIELDFX_DESC& desc)
{
    if (nullptr == desc.m_pDevice)
    {
        return DEPTHOFFIELDFX_RETURN_CODE_INVALID_DEVICE;
    }
    if (nullptr == desc.m_pDeviceContext)
    {
        return DEPTHOFFIELDFX_RETURN_CODE_INVALID_DEVICE_CONTEXT;
    }

    desc.m_pOpaque->initalize(desc);

    return DEPTHOFFIELDFX_RETURN_CODE_SUCCESS;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Resize(const DEPTHOFFIELDFX_DESC& desc)
{
    DEPTHOFFIELDFX_RETURN_CODE result = DEPTHOFFIELDFX_RETURN_CODE_SUCCESS;
    if ((desc.m_screenSize.x > 16384)
        || (desc.m_screenSize.y > 16384)
        || (desc.m_maxBlurRadius > 64))
    {
        result = DEPTHOFFIELDFX_RETURN_CODE_INVALID_PARAMS;
    }
    else
    {
        result = desc.m_pOpaque->resize(desc);
    }
    return result;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Render(const DEPTHOFFIELDFX_DESC& desc)
{
    DEPTHOFFIELDFX_RETURN_CODE result = desc.m_pOpaque->render(desc);
    return result;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_RenderQuarterRes(const DEPTHOFFIELDFX_DESC& desc)
{
    DEPTHOFFIELDFX_RETURN_CODE result = desc.m_pOpaque->render_quarter_res(desc);
    return result;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_RenderBox(const DEPTHOFFIELDFX_DESC& desc)
{
    DEPTHOFFIELDFX_RETURN_CODE result = desc.m_pOpaque->render_box(desc);
    return result;
}

AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Release(const DEPTHOFFIELDFX_DESC& desc)
{
    DEPTHOFFIELDFX_RETURN_CODE result = desc.m_pOpaque->release();
    return result;
}
}
