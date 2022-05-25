/*
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
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

#elif USE_D3D12

#include "DeviceManager12.h"
#include "GFSDK_NVRHI_D3D12.h"
#define API_STRING "D3D12"
NVRHI::RendererInterfaceD3D12* g_pRendererInterface = NULL;

#elif USE_GL4

#include "DeviceManagerGL4.h"
#include "GFSDK_NVRHI_OpenGL4.h"
#define API_STRING "OpenGL"
NVRHI::RendererInterfaceOGL* g_pRendererInterface = NULL;

#endif

using namespace DirectX;

CFirstPersonCamera g_Camera;
CModelViewerCamera g_LightCamera;
DeviceManager* g_DeviceManager = NULL;
SceneRenderer* g_pSceneRenderer = NULL;
VXGI::IGlobalIllumination* g_pGI = NULL;
VXGI::IShaderCompiler* g_pGICompiler = NULL;
VXGI::IViewTracer* g_pGITracer = NULL;

static float g_fCameraClipNear = 1.0f;
static float g_fCameraClipFar = 10000.0f;
static float g_fClipmapRange = 512.0f;
static float g_fAmbientScale = 0.0f;
static float g_fDiffuseScale = 1.0f;
static float g_fSpecularScale = 1.0f;
static bool g_bEnableMultiBounce = true;
static float g_fMultiBounceScale = 1.0f;
static float g_fLightSize = 2500.0f;
static bool g_bEnableGI = true;
static bool g_bRenderHUD = true;
static int g_iDebugLevel = 0;
static bool g_bInitialized = false;
static bool g_bEnableNvidiaExtensions = true;
static VXGI::DebugRenderMode::Enum g_DebugRenderMode = VXGI::DebugRenderMode::DISABLED;
static VXGI::EmittanceFormat::Enum g_EmittanceFormat = VXGI::EmittanceFormat::PERFORMANCE;

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

    if (VXGI_FAILED(g_pGI->createNewTracer(&g_pGITracer)))
    {
        MessageBoxA(g_DeviceManager->GetHWND(), "Failed to create a VXGI tracer.", "VXGI Sample", MB_ICONERROR);
        return E_FAIL;
    }

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
#elif USE_GL4
        TwInit(TW_OPENGL_CORE, nullptr);
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
        TwDefine("barMain label='Settings' size='250 200' valueswidth=100");

        TwAddVarRW(bar, "Enable GI", TW_TYPE_BOOLCPP, &g_bEnableGI, "");
        TwAddVarRW(bar, "Ambient scale", TW_TYPE_FLOAT, &g_fAmbientScale, "min=0 max=10 step=0.01");
        TwAddVarRW(bar, "Diffuse scale", TW_TYPE_FLOAT, &g_fDiffuseScale, "min=0 max=10 step=0.01");
        TwAddVarRW(bar, "Specular scale", TW_TYPE_FLOAT, &g_fSpecularScale, "min=0 max=10 step=0.01");
        TwAddVarRW(bar, "Multi-bounce", TW_TYPE_BOOLCPP, &g_bEnableMultiBounce, "");
        TwAddVarRW(bar, "Multi-bounce scale", TW_TYPE_FLOAT, &g_fMultiBounceScale, "min=0 max=1 step=0.01");
        TwAddVarRW(bar, "Enable NV extensions", TW_TYPE_BOOLCPP, &g_bEnableNvidiaExtensions, "");

        {   // Emittance texture format
            TwEnumVal emittanceFormatEV[] = {
                { VXGI::EmittanceFormat::PERFORMANCE, "Performance (default)" },
                { VXGI::EmittanceFormat::QUALITY, "Quality" },
                { VXGI::EmittanceFormat::UNORM8,  "UNORM8" },
                { VXGI::EmittanceFormat::FLOAT16, "FLOAT16 (GM20x only)" },
                { VXGI::EmittanceFormat::FLOAT32, "FLOAT32" }
            };
            TwType emittanceFormatType = TwDefineEnum("Emittance format", emittanceFormatEV, sizeof(emittanceFormatEV) / sizeof(emittanceFormatEV[0]));
            TwAddVarRW(bar, "Emittance format", emittanceFormatType, &g_EmittanceFormat, "");
        }

        { // Debug rendering mode
            TwEnumVal debugRenderModeEV[] = {
                { VXGI::DebugRenderMode::DISABLED,          "No debug rendering" },
                { VXGI::DebugRenderMode::OPACITY_TEXTURE,   "Opacity map" },
                { VXGI::DebugRenderMode::EMITTANCE_TEXTURE, "Emittance map" },
                { VXGI::DebugRenderMode::INDIRECT_IRRADIANCE_TEXTURE, "Indirect irradiance map" }
            };
            TwType debugRenderModeType = TwDefineEnum("Debug rendering mode", debugRenderModeEV, sizeof(debugRenderModeEV) / sizeof(debugRenderModeEV[0]));
            TwAddVarRW(bar, "Debug mode", debugRenderModeType, &g_DebugRenderMode, "keyIncr=g keyDecr=G");
        }

        TwAddVarRW(bar, "Debug level", TW_TYPE_INT32, &g_iDebugLevel, "min=0 max=4");
    }
};

class MainVisualController : public IVisualController
{
    virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        g_LightCamera.HandleMessages(hWnd, uMsg, wParam, lParam);
        g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

        if (uMsg == WM_KEYDOWN)
        {
            int iKeyPressed = static_cast<int>(wParam);

            switch (iKeyPressed)
            {
            case VK_TAB:
                g_bRenderHUD = !g_bRenderHUD;
                return 0;
                break;

            case VK_SPACE:
                g_DebugRenderMode = VXGI::DebugRenderMode::DISABLED;
                return 0;
                break;
            }

            if (g_DebugRenderMode != VXGI::DebugRenderMode::DISABLED && iKeyPressed >= '1' && iKeyPressed <= '9')
            {
                g_iDebugLevel = iKeyPressed - '1';
                return 0;
            }
        }

        return 1;
    }

    virtual void Animate(double fElapsedTimeSeconds) override
    {
        g_Camera.FrameMove((float)fElapsedTimeSeconds);
        g_LightCamera.FrameMove((float)fElapsedTimeSeconds);

        XMVECTOR lightDir = g_LightCamera.GetLookAtPt() - g_LightCamera.GetEyePt();
        g_pSceneRenderer->SetLightDirection(VXGI::Vector3f(lightDir.m128_f32));
    }

    void SetVoxelizationParameters()
    {
        static VXGI::VoxelizationParameters previousParams;

        VXGI::VoxelizationParameters voxelizationParams;
        voxelizationParams.opacityDirectionCount = VXGI::OpacityDirections::SIX_DIMENSIONAL;
        voxelizationParams.mapSize = 128;
        voxelizationParams.enableMultiBounce = g_bEnableMultiBounce;
        voxelizationParams.persistentVoxelData = !g_bEnableMultiBounce;
        voxelizationParams.emittanceFormat = g_EmittanceFormat;
        voxelizationParams.enableNvidiaExtensions = g_bEnableNvidiaExtensions;
        voxelizationParams.enableGeometryShaderPassthrough = true;

        if (previousParams != voxelizationParams)
        {
            if(VXGI_SUCCEEDED(g_pGI->validateVoxelizationParameters(voxelizationParams)))
                g_pGI->setVoxelizationParameters(voxelizationParams);

            previousParams = voxelizationParams;
        }
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
#elif USE_GL4
        (void)RTV;
        NVRHI::TextureHandle mainRenderTarget = g_pRendererInterface->getHandleForDefaultBackBuffer();
#endif

        XMVECTOR eyePt = g_Camera.GetEyePt();
        XMVECTOR viewForward = g_Camera.GetWorldAhead();
        XMMATRIX viewMatrix = g_Camera.GetViewMatrix();
        XMMATRIX projMatrix = g_Camera.GetProjMatrix();
        XMMATRIX viewProjMatrixXM = viewMatrix * projMatrix;
        VXGI::Matrix4f viewProjMatrix = *reinterpret_cast<VXGI::Matrix4f*>(&viewProjMatrixXM);

        // Render the shadow map before calling g_pGI->updateGlobalIllumination
        // because that function will voxelize the scene using the shadow map
        g_pSceneRenderer->RenderShadowMap(VXGI::Vector3f(0.f), g_fLightSize);

        SetVoxelizationParameters();

        if (g_bEnableGI)
        {
            XMVECTOR centerPt = eyePt + viewForward * g_fClipmapRange;

            static VXGI::Frustum lightFrusta[2];
            lightFrusta[0] = g_pSceneRenderer->GetLightFrustum();

            VXGI::UpdateVoxelizationParameters params;
            params.clipmapAnchor = VXGI::Vector3f(centerPt.m128_f32);
            params.giRange = g_fClipmapRange;
            params.indirectIrradianceMapTracingParameters.irradianceScale = g_fMultiBounceScale;
            params.indirectIrradianceMapTracingParameters.useAutoNormalization = true;

            if (memcmp(&lightFrusta[0], &lightFrusta[1], sizeof(VXGI::Frustum)) != 0)
            {
                params.invalidatedFrustumCount = 2;
                params.invalidatedLightFrusta = lightFrusta;
                lightFrusta[1] = lightFrusta[0];
            }

            bool performOpacityVoxelization = false;
            bool performEmittanceVoxelization = false;

            g_pGI->prepareForOpacityVoxelization(
                params,
                performOpacityVoxelization,
                performEmittanceVoxelization);

            if (performOpacityVoxelization || performEmittanceVoxelization)
            {
                VXGI::Matrix4f voxelizationMatrix;
                g_pGI->getVoxelizationViewMatrix(voxelizationMatrix);

                const uint32_t maxRegions = 128;
                uint32_t numRegions = 0;
                VXGI::Box3f regions[maxRegions];

                if (VXGI_SUCCEEDED(g_pGI->getInvalidatedRegions(regions, maxRegions, numRegions)))
                {
                    if (performOpacityVoxelization)
                    {
                        NVRHI::DrawCallState emptyState;
                        g_pSceneRenderer->RenderSceneCommon(emptyState, g_pGI, regions, numRegions, voxelizationMatrix, NULL, true, false);
                    }

                    if (performEmittanceVoxelization)
                    {
                        g_pGI->prepareForEmittanceVoxelization();
                        NVRHI::DrawCallState emptyState;
                        g_pSceneRenderer->RenderSceneCommon(emptyState, g_pGI, NULL, 0, voxelizationMatrix, NULL, true, true);
                    }
                }
            }

            g_pGI->finalizeVoxelization();
        }

        g_pSceneRenderer->RenderToGBuffer(viewProjMatrix);

        VXGI::IViewTracer::InputBuffers inputBuffers;
        g_pSceneRenderer->FillTracingInputBuffers(inputBuffers);
        memcpy(&inputBuffers.viewMatrix, &viewMatrix, sizeof(viewMatrix));
        memcpy(&inputBuffers.projMatrix, &projMatrix, sizeof(projMatrix));

        if (g_DebugRenderMode != VXGI::DebugRenderMode::DISABLED)
        {
            // Voxel texture visualization is rendered over the albedo channel, no GI

            NVRHI::TextureHandle gbufferAlbedo = g_pSceneRenderer->GetAlbedoBufferHandle();

            VXGI::DebugRenderParameters params;
            params.debugMode = g_DebugRenderMode;
            params.viewMatrix = *(VXGI::Matrix4f*)&viewMatrix;
            params.projMatrix = *(VXGI::Matrix4f*)&projMatrix;
            params.viewport = inputBuffers.gbufferViewport;
            params.destinationTexture = gbufferAlbedo;
            params.destinationDepth = inputBuffers.gbufferDepth;
            params.level = g_iDebugLevel;
            params.blendState.blendEnable[0] = true;
            params.blendState.srcBlend[0] = NVRHI::BlendState::BLEND_SRC_ALPHA;
            params.blendState.destBlend[0] = NVRHI::BlendState::BLEND_INV_SRC_ALPHA;

            g_pGI->renderDebug(params);

            g_pSceneRenderer->Blit(gbufferAlbedo, mainRenderTarget);
        }
        else
        {
            NVRHI::TextureHandle indirectDiffuse = NULL;
            NVRHI::TextureHandle indirectSpecular = NULL;
            VXGI::Vector3f ambientColor(g_fAmbientScale);

            if (g_bEnableGI)
            {
                VXGI::DiffuseTracingParameters diffuseParams;
                VXGI::SpecularTracingParameters specularParams;
                diffuseParams.numCones = 8;
                diffuseParams.tracingSparsity = 4;
                diffuseParams.enableConeRotation = false;
                diffuseParams.irradianceScale = g_fDiffuseScale;
                diffuseParams.ambientColor = ambientColor;
                specularParams.irradianceScale = g_fSpecularScale;
                specularParams.filter = VXGI::SpecularTracingParameters::FILTER_NONE;

                if (g_fDiffuseScale > 0)
                    g_pGITracer->computeDiffuseChannel(diffuseParams, indirectDiffuse, inputBuffers);

                if (g_fSpecularScale > 0)
                    g_pGITracer->computeSpecularChannel(specularParams, indirectSpecular, inputBuffers);
            }

            g_pSceneRenderer->Shade(indirectDiffuse, indirectSpecular, mainRenderTarget, viewProjMatrix, ambientColor * 0.5f);
        }

#if USE_D3D11
        g_pRendererInterface->forgetAboutTexture(pMainResource);
#elif USE_D3D12
        // This needs to be done before resizing the window, but there's no PreResize event from DeviceManager
        g_pRendererInterface->releaseNonManagedTextures();

        g_pRendererInterface->flushCommandList();
#elif USE_GL4
        g_pRendererInterface->UnbindFrameBuffer();
#endif
    }

    virtual HRESULT DeviceCreated() override
    {
#if USE_D3D11
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D11(&g_ErrorCallback, g_DeviceManager->GetImmediateContext());
#elif USE_D3D12
        g_pRendererInterface = new NVRHI::RendererInterfaceD3D12(&g_ErrorCallback, g_DeviceManager->GetDevice(), g_DeviceManager->GetDefaultQueue());
#elif USE_GL4
        g_pRendererInterface = new NVRHI::RendererInterfaceOGL(&g_ErrorCallback);
        g_pRendererInterface->init();
#endif

        g_pSceneRenderer = new SceneRenderer(g_pRendererInterface);

        if (FAILED(CreateVXGIObject()))
            return E_FAIL;

        if (FAILED(g_pSceneRenderer->LoadMesh("..\\..\\media\\Sponza\\SponzaNoFlag.obj")))
            if (FAILED(g_pSceneRenderer->LoadMesh("..\\media\\Sponza\\SponzaNoFlag.obj")))
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
        g_pSceneRenderer->ReleaseViewDependentResources();

        g_pSceneRenderer->AllocateViewDependentResources(width, height, sampleCount);

        // Setup the camera's projection parameters    
        float fAspectRatio = width / (FLOAT)height;
        g_Camera.SetProjParams(XM_PIDIV4, fAspectRatio, g_fCameraClipNear, g_fCameraClipFar);

        // Setup the light camera's projection params
        g_LightCamera.SetProjParams(XM_PIDIV4, 1.0f, g_fCameraClipNear, g_fCameraClipFar);
        g_LightCamera.SetWindow(width, height);
        g_LightCamera.SetButtonMasks(MOUSE_RIGHT_BUTTON, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON);
    }
};


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_DeviceManager = new DeviceManager();

    MainVisualController sceneController;
    AntTweakBarVisualController atbController;
    g_DeviceManager->AddControllerToFront(&sceneController);
    g_DeviceManager->AddControllerToFront(&atbController);

    DeviceCreationParameters deviceParams;
    deviceParams.backBufferWidth = 1280;
    deviceParams.backBufferHeight = 800;
#if !USE_GL4
    deviceParams.swapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    deviceParams.swapChainSampleCount = 1;
    deviceParams.swapChainBufferCount = 4;
    deviceParams.startFullscreen = false;
#endif
#ifdef DEBUG
    deviceParams.enableDebugRuntime = true;
#endif

    if (FAILED(g_DeviceManager->CreateWindowDeviceAndSwapChain(deviceParams, L"VXGI Sample: Basic Global Illumination (" API_STRING ")")))
    {
        MessageBox(NULL, L"Cannot initialize the " API_STRING " device with the requested parameters", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    XMVECTOR eyePt = XMVectorSet(0.0f, 200.0f, 0.0f, 0);
    XMVECTOR lookAtPt = XMVectorSet(-100.0f, 200.0f, 0.0f, 0);
    g_Camera.SetViewParams(eyePt, lookAtPt);
    g_Camera.SetScalers(0.005f, 500.0f);
    g_Camera.SetRotateButtons(true, false, false, false);

    eyePt = XMVectorSet(20.0f, 30.0f, -2.0f, 0);
    lookAtPt = XMVectorSet(0, 0, 0, 0);
    g_LightCamera.SetViewParams(eyePt, lookAtPt);
    g_LightCamera.SetScalers(0.00025f, 100.0f);

    if (g_bInitialized)
        g_DeviceManager->MessageLoop();

    g_DeviceManager->Shutdown();
    delete g_DeviceManager;

    return 0;
}