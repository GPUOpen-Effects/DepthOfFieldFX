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


Texture2D<float>    tDepth : register(t0);
Texture2D<float>    tCoc : register(t0);
RWTexture2D<float>  uCoC : register(u0);
RWTexture2D<float4> uDebugVisCoc : register(u0);

cbuffer CalcDOFParams
{
    uint2 ScreenParams;
    float zNear;
    float zFar;
    float focusDistance;
    float fStop;
    float focalLength;
    float maxRadius;
    float forceCoC;
};


float CocFromDepth(float sceneDepth, float focusDistance, float fStop, float focalLength)
{
    const float cocScale             = (focalLength * focalLength) / fStop;  // move to constant buffer
    const float distanceToLense      = sceneDepth - focalLength;
    const float distanceToFocusPlane = distanceToLense - focusDistance;
    float       coc                  = (distanceToLense > 0.0) ? (cocScale * (distanceToFocusPlane / distanceToLense)) : 0.0;

    coc = clamp(coc * float(ScreenParams.x) * 0.5, -maxRadius, maxRadius);

    return coc;
}

///////////////////////////////////////
// compute camera-space depth for current pixel
float CameraDepth(float depth, float zNear, float zFar)
{
    float invRange = 1.0 / (zFar - zNear);
    return (-zFar * zNear) * invRange / (depth - zFar * invRange);
}


[numthreads(8, 8, 1)]
void CalcDOF(uint3 threadID : SV_DispatchThreadID)
{
    if ((threadID.x < ScreenParams.x) && (threadID.y < ScreenParams.y))
    {
        const float depth    = tDepth.Load(int3(threadID.xy, 0), 0);
        const float camDepth = CameraDepth(depth, zNear, zFar);
        float       CoC      = clamp(CocFromDepth(camDepth, focusDistance, fStop, focalLength), -maxRadius, maxRadius);
        if (abs(forceCoC) > 0.25)
        {
            CoC = -forceCoC;
        }
        uCoC[int2(threadID.xy)] = CoC;
    }
}


[numthreads(8, 8, 1)]
void DebugVisDOF(uint3 Tid : SV_DispatchThreadID)
{
    float4 colorMap[] = {
        { 0.0, 0.0, 0.2, 1.0 }, { 0.0, 0.4, 0.2, 1.0 }, { 0.4, 0.4, 0.0, 1.0 }, { 1.0, 0.2, 0.0, 1.0 }, { 1.0, 0.0, 0.0, 1.0 },
    };

    const uint2 texCoord = Tid.xy;
    const float Coc      = tCoc.Load(int3(texCoord, 0));
    float       value    = min((abs(min(Coc, float(maxRadius))) - 1) / (float(maxRadius - 1) / 4.0), 3.99);
    int         offset   = int(floor(value));
    float       t        = frac(value);
    float4      result   = lerp(colorMap[offset], colorMap[offset + 1], t);
    if (abs(Coc) < 1.0)
    {
        result = float4(0.0, 0.0, 0.1, 1.0);
    }

    uDebugVisCoc[texCoord] = result;
}
