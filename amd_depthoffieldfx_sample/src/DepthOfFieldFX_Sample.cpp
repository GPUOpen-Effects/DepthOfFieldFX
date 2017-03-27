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

// DXUT includes
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"

// AMD includes
#include "AMD_LIB.h"
#include "AMD_SDK.h"
#include "AMD_DepthOfFieldFX.h"


#include <string>

#pragma warning(disable : 4201)  // disable nameless struct/union warnings
#include "DepthOfFieldFX_Sample.h"

#pragma warning(disable : 4100)  // disable unreferenced formal parameter warnings
#pragma warning(disable : 4238)  // disable class rvalue used as lvalue warnings

using namespace DirectX;

//--------------------------------------------------------------------------------------
// UI Resources
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager g_DialogResourceManager;  // Manager for shared resources of dialogs
CDXUTTextHelper*           g_pTxtHelper = NULL;

CFirstPersonCamera  g_Viewer;
S_CAMERA_DESC       g_ViewerData;
CFirstPersonCamera* g_pCurrentCamera = &g_Viewer;
S_CAMERA_DESC*      g_pCurrentData   = &g_ViewerData;

// AMD helper classes defined here
static AMD::MagnifyTool g_MagnifyTool;
static AMD::HUD         g_HUD;

// Global boolean for HUD rendering
bool g_bRenderHUD              = true;
bool g_bShowDOFResult          = true;
bool g_bDebugCircleOfConfusion = false;
bool g_bSaveScreenShot         = false;


enum DepthOfFieldMode
{
    DOF_Disabled                   = 0,
    DOF_BoxFastFilterSpread        = 1,
    DOF_FastFilterSpread           = 2,
    DOF_QuarterResFastFilterSpread = 3,
};

DepthOfFieldMode g_depthOfFieldMode = DOF_FastFilterSpread;

//--------------------------------------------------------------------------------------
// Mesh
//--------------------------------------------------------------------------------------
AMD::Mesh           g_Model;
S_MODEL_DESC        g_ModelDesc;

AMD::Texture2D      g_appColorBuffer;
AMD::Texture2D      g_appDepthBuffer;
AMD::Texture2D      g_appDofSurface;
ID3D11VertexShader* g_d3dFullScreenVS = NULL;
ID3D11PixelShader*  g_d3dFullScreenPS = NULL;


//--------------------------------------------------------------------------------------
// D3D11 Model Rendering Interfaces
//--------------------------------------------------------------------------------------
ID3D11InputLayout*  g_d3dModelIL = NULL;
ID3D11VertexShader* g_d3dModelVS = NULL;
ID3D11PixelShader*  g_d3dModelPS = NULL;
ID3D11Buffer*       g_d3dModelCB = NULL;


//--------------------------------------------------------------------------------------
// D3D11 DOF Rendering Interfaces
//--------------------------------------------------------------------------------------
struct CalcDOFParams
{
    unsigned int ScreenParamsX;
    unsigned int ScreenParamsY;
    float        zNear;
    float        zFar;
    float        focusDistance;
    float        fStop;
    float        focalLength;
    float        maxRadius;
    float        forceCoc;
    float        pad[3];
};

struct CameraParameters
{
    float4 vecEye;
    float4 vecAt;
    float  focalLength;
    float  focalDistance;
    float  sensorWidth;
    float  fStop;
};


static const CameraParameters g_defaultCameraParameters[] = {
    { { 20.2270432, 4.19414091, 16.7282600 }, { 19.4321709, 4.09884357, 16.1290131 }, 400.0f, 21.67f, 100.0f, 1.4f, },
    { { -14.7709570f, 5.55706882f, -17.5470028 }, { -14.1790190f, 5.42186546f, -16.7524414 }, 218.0f, 23.3f, 100.0f, 1.6f, },
    { { 2.34538126f, -0.0807961449f, -12.6757965f }, { 2.23687410f, 0.0531809852f, -11.6907701f }, 190.0f, 14.61f, 100.0f, 1.8f, },
    { { 25.5143566, 5.54141998, -20.4762344 }, { 24.8163872, 5.42109346, -19.7702885 }, 133.0f, 34.95f, 50.0f, 1.6f, },
    { { 5.513732f, 0.803944f, -18.025604f }, { 5.315537f, 0.848312f, -17.046444f }, 205.0f, 39.47f, 85.4f, 2.6f },
    { { -15.698505f, 6.656400f, -21.832394f }, { -15.187683f, 6.442449f, -20.999754f }, 229.0f, 11.3f, 100.00f, 3.9f },
    { { 10.018296f, 0.288034f, -1.364868f }, { 9.142344f, 0.441804f, -0.907634f }, 157.0f, 10.9f, 100.00f, 2.2f },
    { { -3.399786f, 0.948747f, -15.984277f }, { -3.114154f, 1.013084f, -15.028101f }, 366.0f, 16.8f, 100.00f, 1.4f },
    { { -14.941996f, 4.904000f, -17.381784f }, { -14.348591f, 4.798616f, -16.583803f }, 155.0f, 24.9f, 42.70f, 1.4f },
};

static int g_defaultCameraParameterIndex = 0;

ID3D11ComputeShader* g_pCalcCoc  = NULL;
ID3D11ComputeShader* g_pDebugCoc = NULL;


ID3D11Buffer* g_d3dCalcDofCb = NULL;

AMD::Texture2D g_appCoCTexture;

#define MAX_DOF_RADIUS 64
float        g_FocalLength   = 190.0f;  // in mm
float        g_FocalDistance = 14.61f;  // in meters
float        g_sensorWidth   = 100.0f;  // in mm
float        g_fStop         = 1.8f;
float        g_forceCoc      = 0.0f;
unsigned int g_maxRadius     = 57;
unsigned int g_scale_factor  = 30;
unsigned int g_box_scale_factor = 24;

//--------------------------------------------------------------------------------------
// D3D11 Common Rendering Interfaces
//--------------------------------------------------------------------------------------
ID3D11Buffer* g_d3dViewerCB = NULL;

ID3D11SamplerState*      g_d3dLinearWrapSS           = NULL;
ID3D11BlendState*        g_d3dOpaqueBS               = NULL;
ID3D11RasterizerState*   g_d3dBackCullingSolidRS     = NULL;
ID3D11RasterizerState*   g_d3dNoCullingSolidRS       = NULL;
ID3D11DepthStencilState* g_d3dDepthLessEqualDSS      = NULL;

//--------------------------------------------------------------------------------------
// Timing data
//--------------------------------------------------------------------------------------
float g_SceneRenderingTime = 0.0f;
float g_DofRenderingTime  = 0.0f;

//--------------------------------------------------------------------------------------
// Miscellaneous global variables
//--------------------------------------------------------------------------------------
bool        g_bStartWindowed = true;
AMD::uint32 g_ScreenWidth    = 1920;
AMD::uint32 g_ScreenHeight   = 1080;

//--------------------------------------------------------------------------------------
// DepthOfFieldFX global variables
//--------------------------------------------------------------------------------------
AMD::DEPTHOFFIELDFX_DESC g_AMD_DofFX_Desc;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum DEPTHOFFIELDFX_SAMPLE_IDC
{
    // Common
    IDC_TOGGLE_FULLSCREEN = 1,

    // Sample UI
    IDC_CHECKBOX_SHOW_DOF_RESULT,
    IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION,

    ID_COMBOBOX_DOF_METHOD,

    IDC_STATIC_FOCAL_DISTANCE,
    IDC_SLIDER_FOCAL_DISTANCE,
    IDC_STATIC_FSTOP,
    IDC_SLIDER_FSTOP,
    IDC_STATIC_FOCAL_LENGTH,
    IDC_SLIDER_FOCAL_LENGTH,
    IDC_STATIC_SENSOR_WIDTH,
    IDC_SLIDER_SENSOR_WIDTH,
    IDC_SLIDER_MAX_RADIUS,
    IDC_STATIC_MAX_RADIUS,
    IDC_SLIDER_FORCE_COC,
    IDC_STATIC_FORCE_COC,

    IDC_BUTTON_SAVE_SCREEN_SHOT,

    // Total IDC Count
    IDC_NUM_CONTROL_IDS
};


void SetCameraParameters()
{
    const CameraParameters& params = g_defaultCameraParameters[g_defaultCameraParameterIndex];
    // Setup the camera's view parameters
    float4 vecEye(params.vecEye);
    float4 vecAt(params.vecAt);
    float4 vecDir = vecAt.v - vecEye.v;
    vecDir.f[1]   = 0.0f;
    vecDir.v      = XMVector3Normalize(vecDir.v);
    g_Viewer.SetViewParams(vecEye, vecAt);

    g_FocalLength   = params.focalLength;
    g_FocalDistance = params.focalDistance;
    g_sensorWidth   = params.sensorWidth;
    g_fStop         = params.fStop;


    g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_LENGTH)->SetValue((int)g_FocalLength);
    g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_DISTANCE)->SetValue((int)(g_FocalDistance * 100.0f));
    g_HUD.m_GUI.GetSlider(IDC_SLIDER_SENSOR_WIDTH)->SetValue((int)(g_sensorWidth * 10.0f));
    g_HUD.m_GUI.GetSlider(IDC_SLIDER_FSTOP)->SetValue(int(g_fStop * 10.0f));
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
// Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode(false);

    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackFrameMove(OnFrameMove);

    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

    DXUTSetMediaSearchPath(L"Media\\");


    unsigned int dofMajor;
    unsigned int dofMinor;
    unsigned int dofPatch;
    AMD::DepthOfFieldFX_GetVersion(&dofMajor, &dofMinor, &dofPatch);
    WCHAR windowTitle[64];
    swprintf(windowTitle, 64, L"AMD Depth Of Field FX v%d.%d.%d", dofMajor, dofMinor, dofPatch);


    InitApp();

    DXUTInit(true, true);  // Use this line instead to try to create a hardware device

    DXUTSetCursorSettings(true, true);  // Show the cursor and clip it when in full screen
    DXUTCreateWindow(windowTitle);

    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, g_bStartWindowed, g_ScreenWidth, g_ScreenHeight);
    DXUTMainLoop();  // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app
//--------------------------------------------------------------------------------------
void InitApp()
{
    D3DCOLOR DlgColor = 0x88888888;  // Semi-transparent background for the dialog

    g_HUD.m_GUI.Init(&g_DialogResourceManager);

    g_HUD.m_GUI.SetBackgroundColors(DlgColor);
    g_HUD.m_GUI.SetCallback(OnGUIEvent);

    int iY = AMD::HUD::iElementDelta;
    g_HUD.m_GUI.AddButton(IDC_TOGGLE_FULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddStatic(IDC_STATIC_FOCAL_LENGTH, L"Focal Length (mm)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddSlider(IDC_SLIDER_FOCAL_LENGTH, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 18, 400, (int)(g_FocalLength));
    g_HUD.m_GUI.AddStatic(IDC_STATIC_SENSOR_WIDTH, L"Sensor Width (mm)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddSlider(IDC_SLIDER_SENSOR_WIDTH, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 45, 1000, (int)(g_sensorWidth * 10.0f));
    g_HUD.m_GUI.AddStatic(IDC_STATIC_FOCAL_DISTANCE, L"Focal Distance (meters)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddSlider(IDC_SLIDER_FOCAL_DISTANCE, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 10, 10000,
                          (int)(g_FocalDistance * 100.0f));
    g_HUD.m_GUI.AddStatic(IDC_STATIC_FSTOP, L"fStop", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddSlider(IDC_SLIDER_FSTOP, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 14, 220, int(g_fStop * 10.0f));

    g_HUD.m_GUI.AddStatic(IDC_STATIC_MAX_RADIUS, L"MaxRadius", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddSlider(IDC_SLIDER_MAX_RADIUS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 5, MAX_DOF_RADIUS, g_maxRadius);
    g_HUD.m_GUI.AddStatic(IDC_STATIC_FORCE_COC, L"ForceCoC", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddSlider(IDC_SLIDER_FORCE_COC, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, MAX_DOF_RADIUS, (int)(g_forceCoc));

    g_HUD.m_GUI.AddButton(IDC_BUTTON_SAVE_SCREEN_SHOT, L"ScreenShot", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);

    CDXUTComboBox* pComboBox = nullptr;
    g_HUD.m_GUI.AddComboBox(ID_COMBOBOX_DOF_METHOD, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 140, 24, 0, false, &pComboBox);
    pComboBox->AddItem(L"Disabled", nullptr);
    pComboBox->AddItem(L"BoxFFS", nullptr);
    pComboBox->AddItem(L"FFS", nullptr);
    pComboBox->AddItem(L"QuarterResFFS", nullptr);

    pComboBox->SetSelectedByIndex(g_depthOfFieldMode);
    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION, L"Debug Circle Of Conf", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 140, 24, g_bDebugCircleOfConfusion);
    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_SHOW_DOF_RESULT, L"Show Depth Of Field", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 140, 24, g_bShowDOFResult);

    g_MagnifyTool.InitApp(&g_HUD.m_GUI, iY += AMD::HUD::iElementDelta);

    g_Viewer.SetRotateButtons(true, false, false);
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the
// application to modify the device settings. The supplied pDeviceSettings parameter
// contains the settings that the framework has selected for the new device, and the
// application can make any desired changes directly to this structure.  Note however that
// DXUT will not correct invalid device settings so care must be taken
// to return valid device settings, otherwise CreateDevice() will fail.
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
    assert(pDeviceSettings->MinimumFeatureLevel == D3D_FEATURE_LEVEL_11_0);
    pDeviceSettings->d3d11.SyncInterval = 0;

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if (s_bFirstTime)
    {
        s_bFirstTime = false;
        if (pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE)
        {
            DXUTDisplaySwitchingToREFWarning();
        }
    }
    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not
// intended to contain actual rendering calls, which should instead be placed in the
// OnFrameRender callback.
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
    // Update the camera's position based on user input
    g_pCurrentCamera->FrameMove(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render stats
//--------------------------------------------------------------------------------------
void RenderText(float sceneRenderingTime, float sceneRendering, float dofTime, float dof)
{
    g_pTxtHelper->Begin();

    g_pTxtHelper->SetInsertionPos(2, 0);
    g_pTxtHelper->SetForegroundColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    g_pTxtHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
    g_pTxtHelper->DrawTextLine(DXUTGetDeviceStats());

    WCHAR szTemp[256];
    swprintf_s(szTemp, L"Scene Rendering = %.3fms  (%.3fms)", sceneRenderingTime, sceneRendering);
    g_pTxtHelper->DrawTextLine(szTemp);
    swprintf_s(szTemp, L"Depth of Field  = %.3fms  (%.3fms)", dofTime, dof);
    g_pTxtHelper->DrawTextLine(szTemp);

    g_pTxtHelper->SetInsertionPos(10, g_ScreenHeight - 130);
    g_pTxtHelper->DrawTextLine(L"Camera Move        : W/S/A/D/Q/E\n"
                               L"Camera Look        : Left Mouse\n"
                               L"Toggle Quarter Res : TAB\n"
                               L"Change Camera      : C\n"
                               L"Toggle DOF         : G\n"
                               L"Toggle GUI         : F1\n");

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows
// messages to the application through this callback function. If the application sets
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
    {
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
    {
        return 0;
    }

    // Pass all windows messages to camera so it can respond to user input
    g_pCurrentCamera->HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    wchar_t buf[64];
    switch (nControlID)
    {
    case IDC_TOGGLE_FULLSCREEN:
        DXUTToggleFullScreen();
        break;
    case ID_COMBOBOX_DOF_METHOD:
        g_depthOfFieldMode = static_cast<DepthOfFieldMode>(g_HUD.m_GUI.GetComboBox(ID_COMBOBOX_DOF_METHOD)->GetSelectedIndex());
        break;
    case IDC_CHECKBOX_SHOW_DOF_RESULT:
        g_bShowDOFResult = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_SHOW_DOF_RESULT)->GetChecked();
        break;
    case IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION:
        g_bDebugCircleOfConfusion = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION)->GetChecked();
        break;
    case IDC_SLIDER_FOCAL_DISTANCE:
        g_FocalDistance = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_DISTANCE)->GetValue()) / 100.0f;
        swprintf(buf, 64, L"Focal Distance: %.2f", g_FocalDistance);
        g_HUD.m_GUI.GetStatic(IDC_STATIC_FOCAL_DISTANCE)->SetText(buf);
        break;
    case IDC_SLIDER_FSTOP:
        g_fStop = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_FSTOP)->GetValue()) / 10.0f;
        swprintf(buf, 64, L"fStop: %.1f", g_fStop);
        g_HUD.m_GUI.GetStatic(IDC_STATIC_FSTOP)->SetText(buf);
        break;
    case IDC_SLIDER_FOCAL_LENGTH:
        g_FocalLength = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_LENGTH)->GetValue());
        swprintf(buf, 64, L"Focal Length: %.0f", g_FocalLength);
        g_HUD.m_GUI.GetStatic(IDC_STATIC_FOCAL_LENGTH)->SetText(buf);
        break;
    case IDC_SLIDER_SENSOR_WIDTH:
        g_sensorWidth = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_SENSOR_WIDTH)->GetValue()) / 10.0f;
        swprintf(buf, 64, L"Sensor Width: %.1f", g_sensorWidth);
        g_HUD.m_GUI.GetStatic(IDC_STATIC_SENSOR_WIDTH)->SetText(buf);
        break;
    case IDC_SLIDER_MAX_RADIUS:
        g_maxRadius = g_HUD.m_GUI.GetSlider(IDC_SLIDER_MAX_RADIUS)->GetValue();
    case IDC_SLIDER_FORCE_COC:
        g_forceCoc = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_FORCE_COC)->GetValue());
        if (g_forceCoc > static_cast<float>(g_maxRadius))
        {
            g_forceCoc = static_cast<float>(g_maxRadius);
        }
        if (static_cast<float>(g_maxRadius) < g_forceCoc)
        {
            g_maxRadius = static_cast<int>(g_forceCoc);
        }

        swprintf(buf, 64, L"Max Radius: %d", g_maxRadius);
        g_HUD.m_GUI.GetStatic(IDC_STATIC_MAX_RADIUS)->SetText(buf);
        g_AMD_DofFX_Desc.m_maxBlurRadius = g_maxRadius;
        AMD::DepthOfFieldFX_Resize(g_AMD_DofFX_Desc);
        swprintf(buf, 64, L"Force CoC: %.1f", g_forceCoc);
        g_HUD.m_GUI.GetStatic(IDC_STATIC_FORCE_COC)->SetText(buf);
        break;

    case IDC_BUTTON_SAVE_SCREEN_SHOT:
        g_bSaveScreenShot = true;
        break;
    default:
        break;
    }
    // Call the MagnifyTool gui event handler
    g_MagnifyTool.OnGUIEvent(nEvent, nControlID, pControl, pUserContext);
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
#define VK_C (67)
#define VK_G (71)

    if (bKeyDown)
    {
        switch (nChar)
        {
        case VK_F1:
            g_bRenderHUD ^= true;
            break;
        case VK_TAB:
            g_depthOfFieldMode = static_cast<DepthOfFieldMode>(g_depthOfFieldMode ^ 1 | 2);
            g_HUD.m_GUI.GetComboBox(ID_COMBOBOX_DOF_METHOD)->SetSelectedByIndex(g_depthOfFieldMode);
            break;
        case VK_C:
            g_defaultCameraParameterIndex = (g_defaultCameraParameterIndex + 1) % (AMD_ARRAY_SIZE(g_defaultCameraParameters));
            SetCameraParameters();
            break;
        case VK_G:
            g_bShowDOFResult ^= true;
            g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_SHOW_DOF_RESULT)->SetChecked(g_bShowDOFResult);
            break;
        default:
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pSurfaceDesc, void* pUserContext)
{
    HRESULT        hr;
    CD3D11_DEFAULT defaultDesc;

    g_ScreenWidth  = pSurfaceDesc->Width;
    g_ScreenHeight = pSurfaceDesc->Height;

    ID3D11DeviceContext* pd3dContext = DXUTGetD3D11DeviceContext();
    V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dContext));
    g_pTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dContext, &g_DialogResourceManager, 15);

    // Hooks to various AMD helper classes
    V_RETURN(g_MagnifyTool.OnCreateDevice(pd3dDevice));
    V_RETURN(g_HUD.OnCreateDevice(pd3dDevice));

    V_RETURN(CompileShaders(pd3dDevice));

    V_RETURN(CreateMeshes(pd3dDevice));


    V_RETURN(SetupCamera(pd3dDevice));

    // Create common render states (mostly these match d3d11 default state settings)
    CD3D11_SAMPLER_DESC sampler_desc(defaultDesc);
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    hr                    = pd3dDevice->CreateSamplerState(&sampler_desc, &g_d3dLinearWrapSS);

    if (hr == S_OK)
    {
        CD3D11_BLEND_DESC blend_desc(defaultDesc);
        V_RETURN(pd3dDevice->CreateBlendState(&blend_desc, &g_d3dOpaqueBS));
    }

    if (hr == S_OK)
    {
        CD3D11_RASTERIZER_DESC raster_desc(defaultDesc);
        hr                                 = pd3dDevice->CreateRasterizerState(&raster_desc, &g_d3dBackCullingSolidRS);
        D3D11_RASTERIZER_DESC raster_desc2 = raster_desc;
        raster_desc2.CullMode              = D3D11_CULL_NONE;
        raster_desc2.DepthClipEnable       = FALSE;
        V_RETURN(pd3dDevice->CreateRasterizerState(&raster_desc2, &g_d3dNoCullingSolidRS));
    }

    if (hr == S_OK)
    {
        CD3D11_DEPTH_STENCIL_DESC dssDesc(defaultDesc);
        dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        V_RETURN(pd3dDevice->CreateDepthStencilState(&dssDesc, &g_d3dDepthLessEqualDSS));
    }

    TIMER_Init(pd3dDevice);

    ///
    if (hr == S_OK)
    {
        SetupDepthOfField(pd3dDevice);
        g_AMD_DofFX_Desc.m_pDevice        = pd3dDevice;
        g_AMD_DofFX_Desc.m_pDeviceContext = pd3dContext;
        g_AMD_DofFX_Desc.m_screenSize.x   = g_ScreenWidth;
        g_AMD_DofFX_Desc.m_screenSize.y   = g_ScreenHeight;
        AMD::DEPTHOFFIELDFX_RETURN_CODE amdResult = AMD::DepthOfFieldFX_Initialize(g_AMD_DofFX_Desc);
        if (amdResult != AMD::DEPTHOFFIELDFX_RETURN_CODE_SUCCESS)
        {
            return E_FAIL;
        }
    }

    return hr;
}


void SetCameraProjectionParameters()
{
    float fov = 2 * atan(0.5f * g_sensorWidth / g_FocalLength);
    // Setup the camera's projection parameters
    FLOAT fAspectRatio = (float)g_ScreenWidth / (float)g_ScreenHeight;
    g_Viewer.SetProjParams(fov, fAspectRatio, 0.1f, 200.0f);
}


//--------------------------------------------------------------------------------------
// Resize
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

    g_ScreenHeight = pSurfaceDesc->Height;
    g_ScreenWidth  = pSurfaceDesc->Width;

    V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pSurfaceDesc));

    SetCameraProjectionParameters();

    // Set the location and size of the AMD standard HUD
    g_HUD.m_GUI.SetLocation(pSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
    g_HUD.m_GUI.SetSize(AMD::HUD::iDialogWidth, pSurfaceDesc->Height);

    // Magnify tool will capture from the color buffer
    g_MagnifyTool.OnResizedSwapChain(pd3dDevice, pSwapChain, pSurfaceDesc, pUserContext, pSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
    D3D11_RENDER_TARGET_VIEW_DESC RTDesc;
    ID3D11Resource*               pTempRTResource;
    DXUTGetD3D11RenderTargetView()->GetResource(&pTempRTResource);
    DXUTGetD3D11RenderTargetView()->GetDesc(&RTDesc);
    g_MagnifyTool.SetSourceResources(pTempRTResource, RTDesc.Format, g_ScreenWidth, g_ScreenHeight, pSurfaceDesc->SampleDesc.Count);
    g_MagnifyTool.SetPixelRegion(128);
    g_MagnifyTool.SetScale(5);
    SAFE_RELEASE(pTempRTResource);

    // AMD HUD hook
    g_HUD.OnResizedSwapChain(pSurfaceDesc);

    g_AMD_DofFX_Desc.m_screenSize.x = g_ScreenWidth;
    g_AMD_DofFX_Desc.m_screenSize.y = g_ScreenHeight;

    // App specific resources
    // scene render target
    g_appColorBuffer.Release();
    hr = g_appColorBuffer.CreateSurface(pd3dDevice, pSurfaceDesc->Width, pSurfaceDesc->Height, pSurfaceDesc->SampleDesc.Count, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0);

    // scene depth buffer
    g_appDepthBuffer.Release();
    hr = g_appDepthBuffer.CreateSurface(pd3dDevice, pSurfaceDesc->Width, pSurfaceDesc->Height, pSurfaceDesc->SampleDesc.Count, 1, 1, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT,
                                        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0);

    // circle of confusion target
    g_appCoCTexture.Release();
    hr = g_appCoCTexture.CreateSurface(pd3dDevice, pSurfaceDesc->Width, pSurfaceDesc->Height, pSurfaceDesc->SampleDesc.Count, 1, 1, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_UNKNOWN,
                                       DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0);

    // Depth Of Feild Result surface
    g_appDofSurface.Release();
    DXGI_FORMAT appDebugFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    hr = g_appDofSurface.CreateSurface(pd3dDevice, pSurfaceDesc->Width, pSurfaceDesc->Height, pSurfaceDesc->SampleDesc.Count, 1, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, appDebugFormat, appDebugFormat,
                                       DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0);

    g_AMD_DofFX_Desc.m_pCircleOfConfusionSRV = g_appCoCTexture._srv;
    g_AMD_DofFX_Desc.m_pColorSRV             = g_appColorBuffer._srv;
    g_AMD_DofFX_Desc.m_pResultUAV            = g_appDofSurface._uav;
    g_AMD_DofFX_Desc.m_maxBlurRadius         = g_maxRadius;
    AMD::DepthOfFieldFX_Resize(g_AMD_DofFX_Desc);

    return S_OK;
}

void SetCameraConstantBuffer(ID3D11DeviceContext* pd3dContext, ID3D11Buffer* pd3dCameraCB, S_CAMERA_DESC* pCameraDesc, CFirstPersonCamera* pCamera, unsigned int nCount)
{
    if (pd3dContext == NULL)
    {
        OutputDebugString(AMD_FUNCTION_WIDE_NAME L" received a NULL D3D11 Context pointer \n");
        return;
    }
    if (pd3dCameraCB == NULL)
    {
        OutputDebugString(AMD_FUNCTION_WIDE_NAME L" received a NULL D3D11 Constant Buffer pointer \n");
        return;
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource;

    for (unsigned int i = 0; i < nCount; i++)
    {
        CFirstPersonCamera& camera     = pCamera[i];
        S_CAMERA_DESC&      cameraDesc = pCameraDesc[i];

        XMMATRIX view         = camera.GetViewMatrix();
        XMMATRIX proj         = camera.GetProjMatrix();
        XMMATRIX viewproj     = view * proj;
        XMMATRIX view_inv     = XMMatrixInverse(&XMMatrixDeterminant(view), view);
        XMMATRIX proj_inv     = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
        XMMATRIX viewproj_inv = XMMatrixInverse(&XMMatrixDeterminant(viewproj), viewproj);

        cameraDesc.m_View               = XMMatrixTranspose(view);
        cameraDesc.m_Projection         = XMMatrixTranspose(proj);
        cameraDesc.m_View_Inv           = XMMatrixTranspose(view_inv);
        cameraDesc.m_Projection_Inv     = XMMatrixTranspose(proj_inv);
        cameraDesc.m_ViewProjection     = XMMatrixTranspose(viewproj);
        cameraDesc.m_ViewProjection_Inv = XMMatrixTranspose(viewproj_inv);
        cameraDesc.m_Fov                = camera.GetFOV();
        cameraDesc.m_Aspect             = camera.GetAspect();
        cameraDesc.m_NearPlane          = camera.GetNearClip();
        cameraDesc.m_FarPlane           = camera.GetFarClip();

        memcpy(&cameraDesc.m_Position, &(camera.GetEyePt()), sizeof(cameraDesc.m_Position));
        memcpy(&cameraDesc.m_Direction, &(XMVector3Normalize(camera.GetLookAtPt() - camera.GetEyePt())), sizeof(cameraDesc.m_Direction));
        memcpy(&cameraDesc.m_Up, &(camera.GetWorldUp()), sizeof(cameraDesc.m_Position));
    }

    HRESULT hr = pd3dContext->Map(pd3dCameraCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (hr == S_OK && MappedResource.pData)
    {
        memcpy(MappedResource.pData, pCameraDesc, sizeof(S_CAMERA_DESC) * nCount);
        pd3dContext->Unmap(pd3dCameraCB, 0);
    }

    hr = pd3dContext->Map(g_d3dCalcDofCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (hr == S_OK && MappedResource.pData != NULL)
    {
        CalcDOFParams* pParams = reinterpret_cast<CalcDOFParams*>(MappedResource.pData);
        pParams->focalLength   = g_FocalLength / 1000.0f;
        pParams->focusDistance = g_FocalDistance;
        pParams->fStop         = g_fStop;
        pParams->ScreenParamsX = g_ScreenWidth;
        pParams->ScreenParamsY = g_ScreenHeight;
        pParams->zNear         = g_ViewerData.m_NearPlane;
        pParams->zFar          = g_ViewerData.m_FarPlane;
        pParams->maxRadius     = static_cast<float>(g_maxRadius);
        pParams->forceCoc      = g_forceCoc;
        pd3dContext->Unmap(g_d3dCalcDofCb, 0);
    }
}

void SetModelConstantBuffer(ID3D11DeviceContext* pd3dContext, ID3D11Buffer* pd3dModelCB, S_MODEL_DESC* pModelDesc, CFirstPersonCamera* pCamera)
{
    if (pd3dContext == NULL)
    {
        OutputDebugString(AMD_FUNCTION_WIDE_NAME L" received a NULL D3D11 Context pointer \n");
        return;
    }
    if (pd3dModelCB == NULL)
    {
        OutputDebugString(AMD_FUNCTION_WIDE_NAME L" received a NULL D3D11 Constant Buffer pointer \n");
        return;
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource;

    S_MODEL_DESC&       modelDesc = pModelDesc[0];
    CFirstPersonCamera& camera    = pCamera[0];

    XMMATRIX world             = modelDesc.m_World;
    XMMATRIX view              = camera.GetViewMatrix();
    XMMATRIX proj              = camera.GetProjMatrix();
    XMMATRIX viewproj          = view * proj;
    XMMATRIX worldview         = world * view;
    XMMATRIX worldviewproj     = world * viewproj;
    XMMATRIX worldview_inv     = XMMatrixInverse(&XMMatrixDeterminant(worldview), worldview);
    XMMATRIX worldviewproj_inv = XMMatrixInverse(&XMMatrixDeterminant(worldviewproj), worldviewproj);

    modelDesc.m_WorldView               = XMMatrixTranspose(worldview);
    modelDesc.m_WorldView_Inv           = XMMatrixTranspose(worldview_inv);
    modelDesc.m_WorldViewProjection     = XMMatrixTranspose(worldviewproj);
    modelDesc.m_WorldViewProjection_Inv = XMMatrixTranspose(worldviewproj_inv);

    HRESULT hr = pd3dContext->Map(pd3dModelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (hr == S_OK && MappedResource.pData)
    {
        memcpy(MappedResource.pData, &modelDesc, sizeof(modelDesc));
        pd3dContext->Unmap(pd3dModelCB, 0);
    }
}

//--------------------------------------------------------------------------------------
// Render the scene (either for the main scene or the shadow map scene)
//--------------------------------------------------------------------------------------
void RenderScene(ID3D11DeviceContext* pd3dContext, AMD::Mesh** pMesh, S_MODEL_DESC* pMeshDesc, unsigned int nMeshCount,
                 D3D11_VIEWPORT*            pVP,        // ViewPort array
                 unsigned int               nVPCount,   // Viewport count
                 D3D11_RECT*                pSR,        // Scissor Rects array
                 unsigned int               nSRCount,   // Scissor rect count
                 ID3D11RasterizerState*     pRS,        // Raster State
                 ID3D11BlendState*          pBS,        // Blend State
                 float*                     pFactorBS,  // Blend state factor
                 ID3D11DepthStencilState*   pDSS,       // Depth Stencil State
                 unsigned int               dssRef,     // Depth stencil state reference value
                 ID3D11InputLayout*         pIL,        // Input Layout
                 ID3D11VertexShader*        pVS,        // Vertex Shader
                 ID3D11HullShader*          pHS,        // Hull Shader
                 ID3D11DomainShader*        pDS,        // Domain Shader
                 ID3D11GeometryShader*      pGS,        // Geometry SHader
                 ID3D11PixelShader*         pPS,        // Pixel Shader
                 ID3D11Buffer*              pModelCB,
                 ID3D11Buffer**             ppCB,       // Constant Buffer array
                 unsigned int               nCBStart,   // First slot to attach constant buffer array
                 unsigned int               nCBCount,   // Number of constant buffers in the array
                 ID3D11SamplerState**       ppSS,       // Sampler State array
                 unsigned int               nSSStart,   // First slot to attach sampler state array
                 unsigned int               nSSCount,   // Number of sampler states in the array
                 ID3D11ShaderResourceView** ppSRV,      // Shader Resource View array
                 unsigned int               nSRVStart,  // First slot to attach sr views array
                 unsigned int               nSRVCount,  // Number of sr views in the array
                 ID3D11RenderTargetView**   ppRTV,      // Render Target View array
                 unsigned int               nRTVCount,  // Number of rt views in the array
                 ID3D11DepthStencilView*    pDSV,       // Depth Stencil View
                 CFirstPersonCamera*        pCamera)
{
    ID3D11RenderTargetView* const   pNullRTV[8]   = { 0 };
    ID3D11ShaderResourceView* const pNullSRV[128] = { 0 };

    // Unbind anything that could be still bound on input or output
    // If this doesn't happen, DX Runtime will spam with warnings
    pd3dContext->OMSetRenderTargets(AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL);
    pd3dContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pd3dContext->VSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pd3dContext->HSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pd3dContext->DSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pd3dContext->GSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pd3dContext->PSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);

    pd3dContext->IASetInputLayout(pIL);

    pd3dContext->VSSetShader(pVS, NULL, 0);
    pd3dContext->HSSetShader(pHS, NULL, 0);
    pd3dContext->DSSetShader(pDS, NULL, 0);
    pd3dContext->GSSetShader(pGS, NULL, 0);
    pd3dContext->PSSetShader(pPS, NULL, 0);

    if (nSSCount)
    {
        pd3dContext->VSSetSamplers(nSSStart, nSSCount, ppSS);
        pd3dContext->HSSetSamplers(nSSStart, nSSCount, ppSS);
        pd3dContext->DSSetSamplers(nSSStart, nSSCount, ppSS);
        pd3dContext->GSSetSamplers(nSSStart, nSSCount, ppSS);
        pd3dContext->PSSetSamplers(nSSStart, nSSCount, ppSS);
    }

    if (nSRVCount)
    {
        pd3dContext->VSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pd3dContext->HSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pd3dContext->DSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pd3dContext->GSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pd3dContext->PSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
    }

    if (nCBCount)
    {
        pd3dContext->VSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pd3dContext->HSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pd3dContext->DSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pd3dContext->GSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pd3dContext->PSSetConstantBuffers(nCBStart, nCBCount, ppCB);
    }

    pd3dContext->OMSetRenderTargets(nRTVCount, ppRTV, pDSV);
    pd3dContext->OMSetBlendState(pBS, pFactorBS, 0xf);
    pd3dContext->OMSetDepthStencilState(pDSS, dssRef);
    pd3dContext->RSSetState(pRS);
    pd3dContext->RSSetScissorRects(nSRCount, pSR);
    pd3dContext->RSSetViewports(nVPCount, pVP);

    for (unsigned int mesh = 0; mesh < nMeshCount; mesh++)
    {
        SetModelConstantBuffer(pd3dContext, pModelCB, &pMeshDesc[mesh], pCamera);

        pMesh[mesh]->Render(pd3dContext);
    }
}


//--------------------------------------------------------------------------------------
// Render
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, double fTime, float fElapsedTime, void* pUserContext)
{
    D3D11_RECT*                pNullSR  = NULL;
    ID3D11HullShader*          pNullHS  = NULL;
    ID3D11DomainShader*        pNullDS  = NULL;
    ID3D11GeometryShader*      pNullGS  = NULL;
    ID3D11ShaderResourceView*  pNullSRV = NULL;
    ID3D11UnorderedAccessView* pNullUAV = NULL;

    ID3D11RenderTargetView* pOriginalRTV = NULL;
    ID3D11DepthStencilView* pOriginalDSV = NULL;

    const int    nFrameCountMax     = 60;
    static int   nFrameCount        = 0;
    static float fTimeSceneRendering = 0.0f;
    static float fTimeDofRendering  = 0.0f;

    float4 light_blue(0.176f, 0.196f, 0.667f, 0.000f);
    float4 white(1.000f, 1.000f, 1.000f, 1.000f);

    TIMER_Reset();

    SetCameraProjectionParameters();

    // Store the original render target and depth buffer
    pd3dContext->OMGetRenderTargets(1, &pOriginalRTV, &pOriginalDSV);

    // Clear the depth stencil & shadow map
    pd3dContext->ClearRenderTargetView(pOriginalRTV, light_blue.f);
    pd3dContext->ClearDepthStencilView(pOriginalDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
    pd3dContext->ClearRenderTargetView(g_appColorBuffer._rtv, light_blue.f);
    pd3dContext->ClearDepthStencilView(g_appDepthBuffer._dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

    SetCameraConstantBuffer(pd3dContext, g_d3dViewerCB, &g_ViewerData, &g_Viewer, 1);

    TIMER_Begin(0, L"Scene Rendering");
    {
        ID3D11Buffer*           pCB[]  = { g_d3dModelCB, g_d3dViewerCB };
        ID3D11SamplerState*     pSS[]  = { g_d3dLinearWrapSS };
        ID3D11RenderTargetView* pRTV[] = { g_appColorBuffer._rtv };

        AMD::Mesh* meshes[] = { &g_Model };

        RenderScene(pd3dContext, meshes, &g_ModelDesc, 1, &CD3D11_VIEWPORT(0.0f, 0.0f, (float)g_ScreenWidth, (float)g_ScreenHeight), 1, pNullSR, 0,
                    g_d3dBackCullingSolidRS, g_d3dOpaqueBS, white.f, g_d3dDepthLessEqualDSS, 0, g_d3dModelIL, g_d3dModelVS, pNullHS, pNullDS,
                    pNullGS, g_d3dModelPS, g_d3dModelCB, pCB, 0, AMD_ARRAY_SIZE(pCB), pSS, 0, AMD_ARRAY_SIZE(pSS), &pNullSRV, 1, 0, pRTV, AMD_ARRAY_SIZE(pRTV), g_appDepthBuffer._dsv,
                    g_pCurrentCamera);
    }


    pd3dContext->OMSetRenderTargets(1, &pOriginalRTV, pOriginalDSV);

    pd3dContext->CSSetShader(g_pCalcCoc, NULL, 0);
    pd3dContext->CSSetConstantBuffers(0, 1, &g_d3dCalcDofCb);
    pd3dContext->CSSetUnorderedAccessViews(0, 1, &g_appCoCTexture._uav, NULL);
    pd3dContext->CSSetShaderResources(0, 1, &g_appDepthBuffer._srv);
    int threadCountX = (g_ScreenWidth + 7) / 8;
    int threadCountY = (g_ScreenHeight + 7) / 8;
    pd3dContext->Dispatch(threadCountX, threadCountY, 1);
    TIMER_End();

    pd3dContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, NULL);
    pd3dContext->CSSetShaderResources(0, 1, &pNullSRV);

    TIMER_Begin(0, L"Depth Of Field");

    g_AMD_DofFX_Desc.m_scaleFactor = g_scale_factor;

    switch (g_depthOfFieldMode)
    {
    case DOF_BoxFastFilterSpread:
        g_AMD_DofFX_Desc.m_scaleFactor = g_box_scale_factor;
        AMD::DepthOfFieldFX_RenderBox(g_AMD_DofFX_Desc);
        break;
    case DOF_FastFilterSpread:
        AMD::DepthOfFieldFX_Render(g_AMD_DofFX_Desc);
        break;
    case DOF_QuarterResFastFilterSpread:
        AMD::DepthOfFieldFX_RenderQuarterRes(g_AMD_DofFX_Desc);
        break;
    case DOF_Disabled:
    default:
        pd3dContext->CopyResource(g_appDofSurface._t2d, g_appColorBuffer._t2d);
        break;
    }

    TIMER_End();

    if (g_bSaveScreenShot == true)
    {
        DXUTSaveTextureToFile(pd3dContext, g_appDofSurface._t2d, true, L"ScreenShot.dds");
        g_bSaveScreenShot = false;
    }

    if (g_bDebugCircleOfConfusion)
    {
        pd3dContext->CSSetShader(g_pDebugCoc, NULL, 0);
        pd3dContext->CSSetShaderResources(0, 1, &g_appCoCTexture._srv);
        pd3dContext->CSSetUnorderedAccessViews(0, 1, &g_appDofSurface._uav, nullptr);
        pd3dContext->CSSetConstantBuffers(0, 1, &g_d3dCalcDofCb);
        pd3dContext->Dispatch((g_ScreenWidth + 7) / 8, (g_ScreenHeight + 7) / 8, 1);

        pd3dContext->CSSetShaderResources(0, 1, &pNullSRV);
        pd3dContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, NULL);
    }

    pd3dContext->OMSetRenderTargets(1, &pOriginalRTV, pOriginalDSV);
    pd3dContext->VSSetShader(g_d3dFullScreenVS, NULL, 0);
    pd3dContext->PSSetShader(g_d3dFullScreenPS, NULL, 0);
    pd3dContext->PSSetShaderResources(0, 1, g_bShowDOFResult ? &g_appDofSurface._srv : &g_appColorBuffer._srv);
    pd3dContext->PSSetSamplers(0, 1, &g_d3dLinearWrapSS);
    pd3dContext->OMSetBlendState(g_d3dOpaqueBS, white.f, 0xf);
    pd3dContext->OMSetDepthStencilState(g_d3dDepthLessEqualDSS, 0);
    pd3dContext->RSSetState(g_d3dNoCullingSolidRS);
    pd3dContext->Draw(6, 0);

    SAFE_RELEASE(pOriginalRTV);
    SAFE_RELEASE(pOriginalDSV);

    float scene = (float)TIMER_GetTime(Gpu, L"Scene Rendering") * 1000.0f;
    float dof  = (float)TIMER_GetTime(Gpu, L"Depth Of Field") * 1000.0f;

    fTimeSceneRendering += scene;
    fTimeDofRendering += dof;

    if (g_bRenderHUD)
    {
        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");

        g_MagnifyTool.Render();
        g_HUD.OnRender(fElapsedTime);
        RenderText(g_SceneRenderingTime, scene, g_DofRenderingTime, dof);

        DXUT_EndPerfEvent();
    }

    if (nFrameCount++ == nFrameCountMax)
    {
        g_SceneRenderingTime = fTimeSceneRendering / (float)nFrameCountMax;
        g_DofRenderingTime   = fTimeDofRendering / (float)nFrameCountMax;

        fTimeSceneRendering = 0.0f;
        fTimeDofRendering   = 0.0f;
        nFrameCount         = 0;
    }
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    DepthOfFieldFX_Release(g_AMD_DofFX_Desc);

    g_DialogResourceManager.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(g_pTxtHelper);

    g_MagnifyTool.OnDestroyDevice();
    g_HUD.OnDestroyDevice();

    ReleaseMeshes();

    ReleaseShaders();

    SAFE_RELEASE(g_d3dLinearWrapSS);

    SAFE_RELEASE(g_d3dBackCullingSolidRS);

    SAFE_RELEASE(g_d3dDepthLessEqualDSS);
    SAFE_RELEASE(g_d3dOpaqueBS);

    SAFE_RELEASE(g_d3dViewerCB);
    SAFE_RELEASE(g_d3dModelCB);

    g_appColorBuffer.Release();
    g_appDepthBuffer.Release();
    g_appCoCTexture.Release();
    g_appDofSurface.Release();

    SAFE_RELEASE(g_d3dCalcDofCb);
    SAFE_RELEASE(g_d3dNoCullingSolidRS);
    TIMER_Destroy();
}


//--------------------------------------------------------------------------------------
// Release swap chain and backbuffer associated resources
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext) { g_DialogResourceManager.OnD3D11ReleasingSwapChain(); }


HRESULT CompileShader(const wchar_t* pFilename, const char* entryPoint, const char* target, ID3DBlob** ppCode)
{
    ID3DBlob* error_blob = NULL;
    HRESULT   hr         = D3DCompileFromFile(pFilename, NULL, NULL, entryPoint, target, D3DCOMPILE_DEBUG, 0, ppCode, &error_blob);
    if (error_blob != NULL)
    {
        OutputDebugStringA((LPCSTR)error_blob->GetBufferPointer());
    }
    SAFE_RELEASE(error_blob);
    return hr;
}

//--------------------------------------------------------------------------------------
// Compile shaders and create Input Layout
//--------------------------------------------------------------------------------------
HRESULT CompileShaders(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    const D3D11_INPUT_ELEMENT_DESC VSLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    ID3DBlob*     code_blob   = NULL;
    const wchar_t pFilename[] = L"..\\src\\Shaders\\DepthOfFieldFX_Sample.hlsl";

    V_RETURN(CompileShader(pFilename, "VS_RenderModel", "vs_5_0", &code_blob));
    if (hr == S_OK)
    {
        device->CreateVertexShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_d3dModelVS);
        device->CreateInputLayout(VSLayout, AMD_ARRAY_SIZE(VSLayout), code_blob->GetBufferPointer(), code_blob->GetBufferSize(), &g_d3dModelIL);
        SAFE_RELEASE(code_blob);
    }

    V_RETURN(CompileShader(pFilename, "PS_RenderModel", "ps_5_0", &code_blob));
    if (hr == S_OK)
    {
        device->CreatePixelShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_d3dModelPS);
        SAFE_RELEASE(code_blob);
    }

    V_RETURN(CompileShader(L"..\\src\\Shaders\\DepthOfFieldFX_CsCalcDOF.hlsl", "CalcDOF", "cs_5_0", &code_blob));
    if (hr == S_OK)
    {
        device->CreateComputeShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_pCalcCoc);
        SAFE_RELEASE(code_blob);
    }
    V_RETURN(CompileShader(L"..\\src\\Shaders\\DepthOfFieldFX_CsCalcDOF.hlsl", "DebugVisDOF", "cs_5_0", &code_blob));
    if (hr == S_OK)
    {
        device->CreateComputeShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_pDebugCoc);
        SAFE_RELEASE(code_blob);
    }

    AMD::CreateFullscreenPass(&g_d3dFullScreenVS, device);
    AMD::CreateFullscreenPass(&g_d3dFullScreenPS, device);


    return hr;
}

HRESULT ReleaseShaders()
{
    SAFE_RELEASE(g_d3dModelVS);
    SAFE_RELEASE(g_d3dModelPS);
    SAFE_RELEASE(g_d3dModelIL);
    SAFE_RELEASE(g_d3dFullScreenPS);
    SAFE_RELEASE(g_d3dFullScreenVS);
    SAFE_RELEASE(g_pCalcCoc);
    SAFE_RELEASE(g_pDebugCoc);

    return S_OK;
}

HRESULT CreateMeshes(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    // Load the meshe
    hr = g_Model.Create(device, "Tank\\", "TankScene.sdkmesh", true);

    assert(hr == S_OK);

    g_ModelDesc.m_World       = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    g_ModelDesc.m_World_Inv   = XMMatrixInverse(&XMMatrixDeterminant(g_ModelDesc.m_World), g_ModelDesc.m_World);
    g_ModelDesc.m_Position    = float4(0.0f, 0.0f, 0.0f, 1.0f);
    g_ModelDesc.m_Orientation = float4(0.0f, 1.0f, 0.0f, 0.0f);
    g_ModelDesc.m_Scale       = float4(0.001f, 0.001f, 0.001f, 1);
    g_ModelDesc.m_Ambient     = float4(0.1f, 0.1f, 0.1f, 1.0f);
    g_ModelDesc.m_Diffuse     = float4(1.0f, 1.0f, 1.0f, 1.0f);
    g_ModelDesc.m_Specular    = float4(0.5f, 0.5f, 0.0f, 1.0f);


    D3D11_BUFFER_DESC b1d_desc;
    b1d_desc.Usage          = D3D11_USAGE_DYNAMIC;
    b1d_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    b1d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1d_desc.MiscFlags      = 0;
    b1d_desc.ByteWidth      = sizeof(S_MODEL_DESC);
    hr                      = device->CreateBuffer(&b1d_desc, NULL, &g_d3dModelCB);
    assert(hr == S_OK);

    return hr;
}

HRESULT ReleaseMeshes()
{
    g_Model.Release();

    return S_OK;
}

HRESULT SetupCamera(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    SetCameraParameters();

    D3D11_BUFFER_DESC b1d_desc;
    b1d_desc.Usage          = D3D11_USAGE_DYNAMIC;
    b1d_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    b1d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1d_desc.MiscFlags      = 0;
    b1d_desc.ByteWidth      = sizeof(S_CAMERA_DESC);
    hr                      = device->CreateBuffer(&b1d_desc, NULL, &g_d3dViewerCB);
    assert(hr == S_OK);

    return hr;
}

HRESULT SetupDepthOfField(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    if (hr == S_OK)
    {
        D3D11_BUFFER_DESC b1d_desc;
        b1d_desc.Usage          = D3D11_USAGE_DYNAMIC;
        b1d_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        b1d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        b1d_desc.MiscFlags      = 0;
        b1d_desc.ByteWidth      = sizeof(CalcDOFParams);
        hr                      = device->CreateBuffer(&b1d_desc, NULL, &g_d3dCalcDofCb);
    }

    assert(hr == S_OK);
    return hr;
}
