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

#ifndef AMD_DEPTHOFFIELDFX_H
#define AMD_DEPTHOFFIELDFX_H

#define AMD_DEPTHOFFIELDFX_VERSION_MAJOR 1
#define AMD_DEPTHOFFIELDFX_VERSION_MINOR 0
#define AMD_DEPTHOFFIELDFX_VERSION_PATCH 0

// default to static lib
#ifndef AMD_DEPTHOFFIELDFX_COMPILE_DYNAMIC_LIB
#define AMD_DEPTHOFFIELDFX_COMPILE_DYNAMIC_LIB 0
#endif

#if AMD_DEPTHOFFIELDFX_COMPILE_DYNAMIC_LIB
#ifdef AMD_DLL_EXPORT
#define AMD_DEPTHOFFIELDFX_DLL_API __declspec(dllexport)
#else
#define AMD_DEPTHOFFIELDFX_DLL_API __declspec(dllimport)
#endif
#else  // AMD_DEPTHOFFIELD_COMPILE_DYNAMIC_LIB
#define AMD_DEPTHOFFIELDFX_DLL_API
#endif  // AMD_DEPTHOFFIELD_COMPILE_DYNAMIC_LIB

#include "AMD_Types.h"

namespace AMD {
enum DEPTHOFFIELDFX_RETURN_CODE
{
    DEPTHOFFIELDFX_RETURN_CODE_SUCCESS,
    DEPTHOFFIELDFX_RETURN_CODE_FAIL,
    DEPTHOFFIELDFX_RETURN_CODE_INVALID_PARAMS,
    DEPTHOFFIELDFX_RETURN_CODE_INVALID_DEVICE,
    DEPTHOFFIELDFX_RETURN_CODE_INVALID_DEVICE_CONTEXT,
    DEPTHOFFIELDFX_RETURN_CODE_INVALID_SURFACE,
};

struct DEPTHOFFIELDFX_OPAQUE_DESC;

struct DEPTHOFFIELDFX_DESC
{
/**
FX Modules share a variety of trivial types such as vectors and
camera structures. These types are declared inside an FX descriptor
in order to avoid any collisions between different modules or app types.
*/
#pragma warning(push)
#pragma warning(disable : 4201)  // suppress nameless struct/union level 4 warnings
    AMD_DECLARE_BASIC_VECTOR_TYPE;
#pragma warning(pop)
    AMD_DECLARE_CAMERA_TYPE;

    AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_DESC();

    uint2 m_screenSize;
    uint  m_scaleFactor;
    uint  m_maxBlurRadius;

    ID3D11Device*        m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    ID3D11ShaderResourceView*  m_pCircleOfConfusionSRV;
    ID3D11ShaderResourceView*  m_pColorSRV;
    ID3D11UnorderedAccessView* m_pResultUAV;

    DEPTHOFFIELDFX_OPAQUE_DESC* m_pOpaque;

private:
    AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_DESC(const DEPTHOFFIELDFX_DESC&);
    AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_DESC& operator=(const DEPTHOFFIELDFX_DESC&);
};


/**
Get DepthOfFieldFX library version number
This can be called before DEPTHOFFIELDFX_Initialize
*/
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_GetVersion(uint* major, uint* minor, uint* patch);
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Initialize(const DEPTHOFFIELDFX_DESC& desc);
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Resize(const DEPTHOFFIELDFX_DESC& desc);
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Render(const DEPTHOFFIELDFX_DESC& desc);
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_RenderQuarterRes(const DEPTHOFFIELDFX_DESC& desc);
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_RenderBox(const DEPTHOFFIELDFX_DESC& desc);
AMD_DEPTHOFFIELDFX_DLL_API DEPTHOFFIELDFX_RETURN_CODE DepthOfFieldFX_Release(const DEPTHOFFIELDFX_DESC& desc);
}

#endif  // AMD_DEPTHOFFIELD_H
