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

#ifndef DOUBLE_INTEGRATE
#define DOUBLE_INTEGRATE 1
#endif

#ifndef CONVERT_TO_SRGB
#define CONVERT_TO_SRGB 1
#endif

#define RWTexToUse RWStructuredBuffer<int4>

Texture2D<float4> tColor : register(t0);
Texture2D<float>  tCoc : register(t1);

sampler pointSampler : register(s0);

RWTexToUse intermediate : register(u0);
RWTexToUse intermediate_transposed : register(u1);

RWTexture2D<float4> resultColor : register(u2);

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
cbuffer Params : register(b0)
{
    int2   sourceResolution;
    float2 invSourceResolution;
    int2   bufferResolution;
    float  scale_factor;
    int    padding;
    int4   bartlettData[9];
    int4   boxBartlettData[4];
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// Calc offset into buffer from (x,y)
///////////////////////////////////////////////////////////////////////////////////////////////////
uint GetOffset(int2 addr2d) { return ((addr2d.y * bufferResolution.x) + addr2d.x); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Calc offset into transposed buffer from (x,y)
///////////////////////////////////////////////////////////////////////////////////////////////////
uint GetOffsetTransposed(int2 addr2d) { return ((addr2d.x * bufferResolution.y) + addr2d.y); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Atomic add color to buffer
///////////////////////////////////////////////////////////////////////////////////////////////////
void InterlockedAddToBuffer(RWTexToUse buffer, int2 addr2d, int4 color)
{
    const int offset = GetOffset(addr2d);
    InterlockedAdd(buffer[offset].r, color.r);
    InterlockedAdd(buffer[offset].g, color.g);
    InterlockedAdd(buffer[offset].b, color.b);
    InterlockedAdd(buffer[offset].a, color.a);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Read result from buffer
///////////////////////////////////////////////////////////////////////////////////////////////////
int4 ReadFromBuffer(RWTexToUse buffer, int2 addr2d)
{
    const int offset = GetOffset(addr2d);
    int4      result = buffer[offset];
    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// write color to buffer
///////////////////////////////////////////////////////////////////////////////////////////////////
void WriteToBuffer(RWTexToUse buffer, int2 addr2d, int4 color)
{
    const int offset = GetOffset(addr2d);
    buffer[offset]   = color;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// write color to transposed buffer
///////////////////////////////////////////////////////////////////////////////////////////////////
void WriteToBufferTransposed(RWTexToUse buffer, int2 addr2d, int4 color)
{
    const int offset = GetOffsetTransposed(addr2d);
    buffer[offset]   = color;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// convert Circle of Confusion to blur radius in pixels
///////////////////////////////////////////////////////////////////////////////////////////////////
int CocToBlurRadius(const in float fCoc, const in int BlurRadius) { return clamp(abs(int(fCoc)), 0, BlurRadius); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// convert color from float to int and divide by kernel weight
///////////////////////////////////////////////////////////////////////////////////////////////////
int4 normalizeBlurColor(const in float4 color, const in int blur_radius)
{
    const float half_width = blur_radius + 1;

    // the weight for the bartlett is half_width^4
    const float weight = (half_width * half_width * half_width * half_width);

    float normalization = scale_factor / weight;

    return int4(round(color * normalization));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// write the bartlett data to the buffer
///////////////////////////////////////////////////////////////////////////////////////////////////
void WriteDeltaBartlett(RWTexToUse deltaBuffer, float3 vColor, int blur_radius, int2 loc)
{
    int4 intColor = normalizeBlurColor(float4(vColor, 1.0), blur_radius);
    [loop] for (int i = 0; i < 9; ++i)
    {
        const int2 delta       = bartlettData[i].xy * (blur_radius + 1);
        const int  delta_value = bartlettData[i].z;

        // Offset the location by location of the delta and padding
        // Need to offset by (1,1) because the kernel is not centered
        int2 bufLoc = loc.xy + delta + padding + uint2(1, 1);

        // Write the delta
        // Use interlocked add to prevent the threads from stepping on each other
        InterlockedAddToBuffer(deltaBuffer, bufLoc, intColor * delta_value);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// write the bartlett data to the buffer for box filter
///////////////////////////////////////////////////////////////////////////////////////////////////
void WriteBoxDeltaBartlett(RWTexToUse deltaBuffer, float3 vColor, int blur_radius, int2 loc)
{
    float normalization = scale_factor / float(blur_radius * 2 + 1);
    int4  intColor      = int4(round(float4(vColor, 1.0) * normalization));
    for (int i = 0; i < 4; ++i)
    {
        const int2 delta       = boxBartlettData[i].xy * blur_radius;
        const int2 offset      = boxBartlettData[i].xy > int2(0, 0);
        const int  delta_value = boxBartlettData[i].z;

        // Offset the location by location of the delta and padding
        int2 bufLoc = loc.xy + delta + padding + offset;

        // Write the delta
        // Use interlocked add to prevent the threads from stepping on each other's toes
        InterlockedAddToBuffer(deltaBuffer, bufLoc, intColor * delta_value);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(8, 8, 1)]
void FastFilterSetup(uint3 ThreadID : SV_DispatchThreadID)
{
    if ((int(ThreadID.x) < sourceResolution.x) && (int(ThreadID.y) < sourceResolution.y))
    {
        // Read the coc from the coc\depth buffer
        const float fcoc        = tCoc.Load(int3(ThreadID.xy, 0));
        const int   blur_radius = CocToBlurRadius(fcoc, padding);
        float3      vColor      = tColor.Load(int3(ThreadID.xy, 0)).rgb;

        WriteDeltaBartlett(intermediate, vColor, blur_radius, ThreadID.xy);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(8, 8, 1)]
void QuarterResFastFilterSetup(uint3 ThreadID : SV_DispatchThreadID)
{
    uint2 loc = ThreadID.xy * 2;
    if ((int(loc.x) < sourceResolution.x) && (int(loc.y) < sourceResolution.y))
    {
        float2 texCoord = (loc + 1.0) * invSourceResolution;

        const int    maxRadius = padding;
        const float4 fCoc4     = tCoc.Gather(pointSampler, texCoord);
        const float4 focusMask = 2.0 < abs(fCoc4);
        const float  weight    = dot(focusMask, focusMask);

        const float4 red4   = tColor.GatherRed(pointSampler, texCoord);
        const float4 green4 = tColor.GatherGreen(pointSampler, texCoord);
        const float4 blue4  = tColor.GatherBlue(pointSampler, texCoord);

        if (weight >= 3.9)
        {
            float4      absCoc4     = abs(fCoc4);
            const float fcoc        = max(max(absCoc4.x, absCoc4.y), max(absCoc4.z, absCoc4.w));
            const int   blur_radius = CocToBlurRadius(fcoc, padding);
            float3      vColor;
            vColor.r = dot(red4, focusMask) / weight;
            vColor.g = dot(green4, focusMask) / weight;
            vColor.b = dot(blue4, focusMask) / weight;
            WriteDeltaBartlett(intermediate, vColor, blur_radius, loc);
        }
        else
        {
            WriteDeltaBartlett(intermediate, float3(red4.x, green4.x, blue4.x), CocToBlurRadius(fCoc4.x, padding), loc + int2(0, 1));
            WriteDeltaBartlett(intermediate, float3(red4.y, green4.y, blue4.y), CocToBlurRadius(fCoc4.y, padding), loc + int2(1, 1));
            WriteDeltaBartlett(intermediate, float3(red4.z, green4.z, blue4.z), CocToBlurRadius(fCoc4.z, padding), loc + int2(1, 0));
            WriteDeltaBartlett(intermediate, float3(red4.w, green4.w, blue4.w), CocToBlurRadius(fCoc4.w, padding), loc + int2(0, 0));
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(8, 8, 1)]
void BoxFastFilterSetup(uint3 ThreadID : SV_DispatchThreadID)
{
    if ((int(ThreadID.x) < sourceResolution.x) && (int(ThreadID.y) < sourceResolution.y))
    {
        // Read the coc from the coc\depth buffer
        const float fcoc        = tCoc.Load(int3(ThreadID.xy, 0));
        const int   blur_radius = CocToBlurRadius(fcoc, padding);
        float3      vColor      = tColor.Load(int3(ThreadID.xy, 0)).rgb;

        WriteBoxDeltaBartlett(intermediate, vColor, blur_radius, ThreadID.xy);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Read and normalize the results
///////////////////////////////////////////////////////////////////////////////////////////////////
float4 ReadResult(RWTexToUse buffer, int2 loc)
{
    float4 tex_read = ReadFromBuffer(buffer, loc + padding);

    // normalize the result
    tex_read.rgba /= tex_read.a;

    return tex_read;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Integrate a single domain - write the results out transposed
///////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(64, 1, 1)]
void VerticalIntegrate(uint3 Tid : SV_DispatchThreadID, uint3 Gid : SV_GroupID)
{
    // To perform double integration in a single step, we must keep two counters delta and color

    if (int(Tid.x) < bufferResolution.x)
    {
        int2 addr = int2(Tid.x, 0);

        // Initialization/////////////////////////////////////
        // We want delta and color to be the same
        // their value is just the first element of the domain
        int4 delta = ReadFromBuffer(intermediate, addr);
        int4 color = delta;
        /////////////////////////////////////////////////////

        uint chunkEnd = bufferResolution.y;

        // Actually do the integration////////////////////////////////
        [loop] for (uint i = 1; i < chunkEnd; ++i)
        {
            addr = int2(Tid.x, i);
            // Read from the current location
            delta += ReadFromBuffer(intermediate, addr);

#if DOUBLE_INTEGRATE
            // Accumulate to the delta
            color += delta;
#else
            color = delta;
#endif
            // Write the delta integrated value to the output
            WriteToBufferTransposed(intermediate_transposed, addr, color);
        }
    }
}


float3 LinearToSRGB(float3 linColor) { return pow(abs(linColor), 1.0 / 2.2); }

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(8, 8, 1)] void ReadFinalResult(uint3 Tid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
    uint2 texCoord = Tid.xy;
    float Coc      = tCoc[texCoord];

    float4 result = ReadResult(intermediate, texCoord);

#if CONVERT_TO_SRGB
    result.rgb = LinearToSRGB(result.rgb);
#endif

    resultColor[texCoord] = float4(result.rgb, 1.0);
}
