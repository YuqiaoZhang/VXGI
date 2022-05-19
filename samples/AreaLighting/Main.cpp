/*
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "SceneRenderer.h"
#include "Camera.h"
#include <AntTweakBar.h>

#if USE_D3D11

#include "DeviceManager11.h"
#include "GFSDK_NVRHI_D3D11.h"
#define API_STRING "D3D11"
NVRHI::RendererInterfaceD3D11* g_pRendererInterface = NULL;
#define FLUSH_COMMAND_LIST 

#elif USE_D3D12

#include "DeviceManager12.h"
#include "GFSDK_NVRHI_D3D12.h"
#define API_STRING "D3D12"
NVRHI::RendererInterfaceD3D12* g_pRendererInterface = NULL;
#define FLUSH_COMMAND_LIST g_pRendererInterface->flushCommandList()

#endif

#include <d3dcompiler.h>

using namespace DirectX;
using VXGI::float3;

CFirstPersonCamera g_Camera;
CFirstPersonCamera g_AreaLightCamera;
DeviceManager* g_DeviceManager = NULL;
SceneRenderer* g_pSceneRenderer = NULL;
VXGI::IGlobalIllumination* g_pGI = NULL;
VXGI::IShaderCompiler* g_pGICompiler = NULL;
VXGI::IBasicViewTracer* g_pGITracer = NULL;

static float g_fCameraClipNear = 1.0f;
static float g_fCameraClipFar = 10000.0f;
static float g_fVoxelSize = 4.0f;
static bool g_bRenderHUD = true;
static bool g_bShowVoxels = false;
static bool g_bInitialized = false;
static float g_fShadowQuality = 0.75f;
static uint32_t g_nMapSize = 128;
static uint32_t g_nStackLevels = 4;
static bool g_bOcclusionHack = true;

static float g_LightWidth = 190.f;
static float g_LightHeight = 160.f;
static bool g_LightTexture = true;
static bool g_bEnableShadows = true;
static bool g_bTexturedShadows = true;
static bool g_bEnableScreenSpaceShadows = true;
static float g_DiffuseIntensity = 2.f;
static float g_SpecularIntensity = 2.f;
static float3 g_LightColor = float3(1.f);
static bool g_bMoveLight = false;
static bool g_bTemporalFiltering = true;
static bool g_bUpdateVoxelization = false;

VXGI::IBasicViewTracer::InputBuffers g_InputBuffersPrev;
bool g_InputBuffersPrevValid = false;


class RendererErrorCallback : public NVRHI::IErrorCallback
{
    void signalError(const char* file, int line, const char* errorDesc)
    {
        char buffer[4096];
        int length = (int)strlen(errorDesc);
        length = std::min(length, 4000); // avoid a "buffer too small" exception for really long error messages
        sprintf_s(buffer, "%s:%i\n%.*s", file, line, length, errorDesc);

        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
        MessageBoxA(NULL, buffer, "ERROR", MB_ICONERROR | MB_OK);
    }
};

RendererErrorCallback g_ErrorCallback;


HRESULT CreateVXGIObject()
{
    VXGI::GIParameters params;
    params.rendererInterface = g_pRendererInterface;
    params.errorCallback = &g_ErrorCallback;

    VXGI::ShaderCompilerParameters comparams;
    comparams.errorCallback = &g_ErrorCallback;
    comparams.graphicsAPI = g_pRendererInterface->getGraphicsAPI();
    comparams.d3dCompilerDLLName = "d3dcompiler_hook.dll";
    comparams.d3dCompileFlags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

    if (VXGI_FAILED(VFX_VXGI_CreateShaderCompiler(comparams, &g_pGICompiler)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI shader compiler.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    if (VXGI_FAILED(VFX_VXGI_CreateGIObject(params, &g_pGI)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI object.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    if (VXGI_FAILED(g_pGI->createBasicTracer(&g_pGITracer, g_pGICompiler)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI tracer.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    VXGI::VoxelizationParameters voxelizationParams;
    voxelizationParams.ambientOcclusionMode = true;
    voxelizationParams.persistentVoxelData = true;
	voxelizationParams.mapSize = VXGI::uint3(g_nMapSize);
    voxelizationParams.stackLevels = g_nStackLevels;
    
    if (VXGI_FAILED(g_pGI->setVoxelizationParameters(voxelizationParams)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to initialize VXGI voxelization.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

    g_bUpdateVoxelization = true;

    return S_OK;
}

class AntTweakBarVisualController : public IVisualController
{
    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        if (g_bRenderHUD || uMsg == WM_KEYDOWN || uMsg == WM_CHAR)
            if (TwEventWin(hWnd, uMsg, wParam, lParam))
            {
                return 0; // Event has been handled by AntTweakBar
            }

        return 1;
    }

    void RenderText()
    {
        TwBeginText(2, 0, 0, 0);
        const unsigned int color = 0xffffc0ff;
        char msg[256];

        double averageTime = g_DeviceManager->GetAverageFrameTime();
        double fps = (averageTime > 0) ? 1.0 / averageTime : 0.0;

        sprintf_s(msg, "%.1f FPS", fps);
        TwAddTextLine(msg, color, 0);
        TwAddTextLine("Use the right mouse button to rotate the light.", color, 0);

        TwEndText();
    }

    virtual void Render(RenderTargetView RTV) override
    {
#if USE_D3D12
        TwSetD3D12RenderTargetView((void*)RTV.ptr);
#else
        (void)RTV;
#endif

        if (g_bRenderHUD)
        {
            RenderText();
            TwDraw();
        }
    }

    virtual HRESULT DeviceCreated() override
    {
#if USE_D3D11
        TwInit(TW_DIRECT3D11, g_DeviceManager->GetDevice());
#elif USE_D3D12
        TwD3D12DeviceInfo info;
        info.Device = g_DeviceManager->GetDevice();
        info.DefaultCommandQueue = g_DeviceManager->GetDefaultQueue();
        info.RenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        info.RenderTargetSampleCount = 1;
        info.RenderTargetSampleQuality = 0;
        info.UploadBufferSize = 1024 * 1024;
        TwInit(TW_DIRECT3D12, &info);
#endif
        InitDialogs();
        return S_OK;
    }

    virtual void DeviceDestroyed() override
    {
        TwTerminate();
    }

    virtual void BackBufferResized(uint32_t width, uint32_t height, uint32_t sampleCount) override
    {
        (void)sampleCount;
        TwWindowSize(width, height);
    }

    void InitDialogs()
    {
        TwBar* bar = TwNewBar("barMain");
        TwDefine("barMain label='Settings' position='10 35' size='250 300' valueswidth=100");

        TwAddVarRW(bar, "Diffuse Intensity", TW_TYPE_FLOAT, &g_DiffuseIntensity, "min=0 max=10 step=0.01");
        TwAddVarRW(bar, "Specular Intensity", TW_TYPE_FLOAT, &g_SpecularIntensity, "min=0 max=10 step=0.01");
        TwAddVarRW(bar, "Texture", TW_TYPE_BOOLCPP, &g_LightTexture, nullptr);

        TwAddSeparator(bar, nullptr, nullptr);

        TwAddVarRW(bar, "Color", TW_TYPE_COLOR3F, &g_LightColor, nullptr);
        TwAddVarRW(bar, "Width", TW_TYPE_FLOAT, &g_LightWidth, "min=1 max=1000 step=1");
        TwAddVarRW(bar, "Height", TW_TYPE_FLOAT, &g_LightHeight, "min=1 max=1000 step=1");
        TwAddVarRW(bar, "Move Light", TW_TYPE_BOOLCPP, &g_bMoveLight, "key=l");

        TwAddSeparator(bar, nullptr, nullptr);

        TwAddVarRW(bar, "Shadows", TW_TYPE_BOOLCPP, &g_bEnableShadows, "key=o");
        TwAddVarRW(bar, "Textured Shadows", TW_TYPE_BOOLCPP, &g_bTexturedShadows, "");
        TwAddVarRW(bar, "Screen Space Shadows", TW_TYPE_BOOLCPP, &g_bEnableScreenSpaceShadows, "");
        TwAddVarRW(bar, "Shadow Quality", TW_TYPE_FLOAT, &g_fShadowQuality, "min=0 max=1 step=0.01");
        TwAddVarRW(bar, "Temporal Filtering", TW_TYPE_BOOLCPP, &g_bTemporalFiltering, nullptr);
        TwAddVarCB(bar, "Reduce Floor Occlusion", TW_TYPE_BOOLCPP, 
            [](const void* value, void*) { g_bOcclusionHack = *((const bool*)value); g_bUpdateVoxelization = true; },
            [](void* value, void*) { *((bool*)value) = g_bOcclusionHack; },
            nullptr, nullptr);
        TwAddVarRW(bar, "Show Voxels", TW_TYPE_BOOLCPP, &g_bShowVoxels, "");
    }
};

class MainVisualController : public IVisualController
{
    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        if(!g_bMoveLight)
            g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

        if(g_bMoveLight || uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
            g_AreaLightCamera.HandleMessages(hWnd, uMsg, wParam, lParam);

        if (uMsg == WM_KEYDOWN)
        {
            int iKeyPressed = static_cast<int>(wParam);

            switch (iKeyPressed)
            {
            case VK_TAB:
                g_bRenderHUD = !g_bRenderHUD;
                return 0;
                break;
            }
        }

        return 1;
    }

    virtual void Animate(double fElapsedTimeSeconds) override
    {
        g_Camera.FrameMove((float)fElapsedTimeSeconds);
        g_AreaLightCamera.FrameMove((float)fElapsedTimeSeconds);
    }

    virtual void Render(RenderTargetView RTV) override
    {
#if USE_D3D11
        ID3D11Resource* pMainResource = NULL;
        RTV->GetResource(&pMainResource);
        NVRHI::TextureHandle mainRenderTarget = g_pRendererInterface->getHandleForTexture(pMainResource);
        pMainResource->Release();
#elif USE_D3D12
        (void)RTV;
        NVRHI::TextureHandle mainRenderTarget = g_pRendererInterface->getHandleForTexture(g_DeviceManager->GetCurrentBackBuffer());
        g_pRendererInterface->setNonManagedTextureResourceState(mainRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
#endif

        XMVECTOR eyePt = g_Camera.GetEyePt();
        XMVECTOR viewForward = g_Camera.GetWorldAhead();
        XMMATRIX viewMatrix = g_Camera.GetViewMatrix();
        XMMATRIX projMatrix = g_Camera.GetProjMatrix();
        XMMATRIX viewProjMatrix = viewMatrix * projMatrix;

        {
            XMVECTOR centerPt = eyePt + viewForward * g_fVoxelSize * float(g_nMapSize) * 0.25f;

            VXGI::UpdateVoxelizationParameters params;
            params.clipmapAnchor = VXGI::float3(centerPt.m128_f32);
            params.finestVoxelSize = g_fVoxelSize;

            VXGI::ComputeVoxelizationViewParametersInput cvvpi;
            cvvpi.clipmapAnchor = params.clipmapAnchor;
            cvvpi.finestVoxelSize = params.finestVoxelSize;
            cvvpi.mapSize = VXGI::uint3(g_nMapSize);
            cvvpi.stackLevels = g_nStackLevels;
            VXGI::VoxelizationViewParameters viewParams;
            VXGI::VFX_VXGI_ComputeVoxelizationViewParameters(cvvpi, viewParams);

            if (g_bUpdateVoxelization)
            {
                params.invalidatedRegionCount = 1;
                params.invalidatedRegions = &viewParams.clipmapExtents;
                g_bUpdateVoxelization = false;
            }

            bool performOpacityVoxelization = false;
            bool performEmittanceVoxelization = false;

            g_pGI->prepareForVoxelization(
                params,
                performOpacityVoxelization,
                performEmittanceVoxelization);

            FLUSH_COMMAND_LIST;

            if (performOpacityVoxelization)
            {
                VXGI::float4x4 voxelizationMatrix = viewParams.viewMatrix * viewParams.projectionMatrix;

                const uint32_t maxRegions = 128;
                uint32_t numRegions = 0;
                VXGI::Box3f regions[maxRegions];

                if (VXGI_SUCCEEDED(g_pGI->getInvalidatedRegions(regions, maxRegions, numRegions)))
                {
                    NVRHI::DrawCallState emptyState;
                    g_pSceneRenderer->RenderForVoxelization(emptyState, g_pGI, regions, numRegions, voxelizationMatrix, NULL, g_bOcclusionHack);
                }
            }

            FLUSH_COMMAND_LIST;

            g_pGI->finalizeVoxelization();
        }

        g_pSceneRenderer->RenderToGBuffer((VXGI::float4x4&)viewProjMatrix);

        VXGI::IBasicViewTracer::InputBuffers inputBuffers;
        g_pSceneRenderer->FillTracingInputBuffers(inputBuffers);
        memcpy(&inputBuffers.viewMatrix, &viewMatrix, sizeof(viewMatrix));
        memcpy(&inputBuffers.projMatrix, &projMatrix, sizeof(projMatrix));

        NVRHI::TextureHandle gbufferAlbedo = g_pSceneRenderer->GetAlbedoBufferHandle();

        if (g_bShowVoxels)
        {
            VXGI::DebugRenderParameters params;
            params.debugMode = VXGI::DebugRenderMode::OPACITY_TEXTURE;
            params.viewMatrix = *(VXGI::float4x4*)&viewMatrix;
            params.projMatrix = *(VXGI::float4x4*)&projMatrix;
            params.viewport = inputBuffers.gbufferViewport;
            params.destinationTexture = gbufferAlbedo;
            params.destinationDepth = inputBuffers.gbufferDepth;
            params.level = -1;
            params.blendState.blendEnable[0] = true;
            params.blendState.srcBlend[0] = NVRHI::BlendState::BLEND_SRC_ALPHA;
            params.blendState.destBlend[0] = NVRHI::BlendState::BLEND_INV_SRC_ALPHA;

            g_pGI->renderDebug(params);

            g_pSceneRenderer->Blit(gbufferAlbedo, mainRenderTarget);
        }
        else
        {
            g_pGITracer->beginFrame();

            VXGI::BasicAreaLightTracingParameters areaLightParams;
            areaLightParams.enableTemporalReprojection = g_bTemporalFiltering;
            areaLightParams.enableTemporalJitter = g_bTemporalFiltering;
            areaLightParams.coplanarOffsetFactor = g_bOcclusionHack ? 0.f : 5.f;
            
            VXGI::AreaLight light;
            light.center = float3(g_AreaLightCamera.GetEyePt().m128_f32);
            light.majorAxis = float3(g_AreaLightCamera.GetWorldRight().m128_f32) * (-g_LightWidth);
            light.minorAxis = float3(g_AreaLightCamera.GetWorldUp().m128_f32) * g_LightHeight;
            light.enableOcclusion = g_bEnableShadows;
            light.enableScreenSpaceOcclusion = g_bEnableScreenSpaceShadows;
            light.texturedShadows = g_bTexturedShadows;
            light.diffuseIntensity = g_DiffuseIntensity;
            light.specularIntensity = g_SpecularIntensity;
            light.color = g_LightColor;
            light.quality = g_fShadowQuality;
            light.texture = g_LightTexture ? g_pSceneRenderer->GetAreaLightTexture() : nullptr;
            
            NVRHI::TextureHandle areaLightDiffuse = nullptr;
            NVRHI::TextureHandle areaLightSpecular = nullptr;
            g_pGITracer->computeAreaLightChannelsBasic(areaLightParams, inputBuffers, g_InputBuffersPrevValid ? &g_InputBuffersPrev : nullptr, &light, 1, areaLightDiffuse, areaLightSpecular);

            //g_pSceneRenderer->Blit(areaLightSpecular, mainRenderTarget);    
            g_pSceneRenderer->CompositeLighting(areaLightDiffuse, areaLightSpecular, mainRenderTarget);

            g_pSceneRenderer->RenderAreaLight(light, (VXGI::float4x4&)viewProjMatrix, mainRenderTarget);
        }

        g_InputBuffersPrev = inputBuffers;
        g_InputBuffersPrevValid = true;


#if USE_D3D11
        g_pRendererInterface->forgetAboutTexture(pMainResource);
#elif USE_D3D12
        // This needs to be done before resizing the window, but there's no PreResize event from DeviceManager
        g_pRendererInterface->releaseNonManagedTextures();

        g_pRendererInterface->flushCommandList();
#endif
    }

    virtual HRESULT DeviceCreated() override
    {
#if USE_D3D11
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D11(&g_ErrorCallback, g_DeviceManager->GetImmediateContext());
#elif USE_D3D12
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D12(&g_ErrorCallback, g_DeviceManager->GetDevice(), g_DeviceManager->GetDefaultQueue());
#endif
        g_pSceneRenderer = new SceneRenderer(g_pRendererInterface);

        if (FAILED(CreateVXGIObject()))
            return E_FAIL;

        if (FAILED(g_pSceneRenderer->LoadMesh("..\\..\\media\\Sponza\\SponzaNoFlag.obj")))
            if (FAILED(g_pSceneRenderer->LoadMesh("..\\media\\Sponza\\SponzaNoFlag.obj")))
                return E_FAIL;

        if (FAILED(g_pSceneRenderer->LoadMesh2("..\\..\\media\\dragon.obj")))
            if (FAILED(g_pSceneRenderer->LoadMesh2("..\\media\\dragon.obj")))
                return E_FAIL;

        if (FAILED(g_pSceneRenderer->LoadAreaLightTexture("..\\..\\media\\arealight.jpg")))
            if (FAILED(g_pSceneRenderer->LoadAreaLightTexture("..\\media\\arealight.jpg")))
                return E_FAIL;

        if (FAILED(g_pSceneRenderer->AllocateResources(g_pGI, g_pGICompiler)))
            return E_FAIL;

        g_bInitialized = true;

        return S_OK;
    }

    virtual void DeviceDestroyed() override
    {
        if (g_pSceneRenderer)
        {
            g_pSceneRenderer->ReleaseViewDependentResources();
            g_pSceneRenderer->ReleaseResources(g_pGI);
        }

        if (g_pGI)
        {
            if (g_pGITracer) g_pGI->destroyTracer(g_pGITracer);
            g_pGITracer = NULL;

            VFX_VXGI_DestroyGIObject(g_pGI);
            g_pGI = NULL;
        }

        if (g_pSceneRenderer)
        {
            delete g_pSceneRenderer;
            g_pSceneRenderer = NULL;
        }
    }

    virtual void BackBufferResized(uint32_t width, uint32_t height, uint32_t sampleCount) override
    {
        g_InputBuffersPrevValid = false;

        g_pSceneRenderer->ReleaseViewDependentResources();

        g_pSceneRenderer->AllocateViewDependentResources(width, height, sampleCount);

        // Setup the camera's projection parameters    
        float fAspectRatio = width / (FLOAT)height;
        g_Camera.SetProjParams(XM_PIDIV4, fAspectRatio, g_fCameraClipNear, g_fCameraClipFar);
    }
};


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    g_DeviceManager = new DeviceManager();

    MainVisualController sceneController;
    AntTweakBarVisualController atbController;
    g_DeviceManager->AddControllerToFront(&sceneController);
    g_DeviceManager->AddControllerToFront(&atbController);

    DeviceCreationParameters deviceParams;
    deviceParams.backBufferWidth = 1280;
    deviceParams.backBufferHeight = 800;
    deviceParams.swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    deviceParams.swapChainSampleCount = 1;
    deviceParams.swapChainBufferCount = 4;
    deviceParams.startFullscreen = false;
#ifdef DEBUG
    deviceParams.enableDebugRuntime = true;
#endif

    if (FAILED(g_DeviceManager->CreateWindowDeviceAndSwapChain(deviceParams, L"VXGI Sample: Area Lighting (" API_STRING ")")))
    {
        MessageBox(NULL, L"Cannot initialize the " API_STRING " device with the requested parameters", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    XMVECTOR eyePt = XMVectorSet(230.0f, 175.0f, -40.0f, 0);
    XMVECTOR lookAtPt = XMVectorSet(0.0f, 175.0f, -40.0f, 0);
    g_Camera.SetViewParams(eyePt, lookAtPt);
    g_Camera.SetScalers(0.005f, 500.0f);
    g_Camera.SetRotateButtons(true, false, false, false);

    XMVECTOR areaLightPt = XMVectorSet(-850.0f, 165.0f, -38.0f, 0);
    XMVECTOR areaLightLookAt = XMVectorSet(-100.0f, 165.0f, -38.0f, 0);
    g_AreaLightCamera.SetViewParams(areaLightPt, areaLightLookAt);
    g_AreaLightCamera.SetScalers(0.005f, 500.0f);
    g_AreaLightCamera.SetRotateButtons(false, false, true, false);

    if (g_bInitialized)
        g_DeviceManager->MessageLoop();

    g_DeviceManager->Shutdown();
    delete g_DeviceManager;

    return 0;
}