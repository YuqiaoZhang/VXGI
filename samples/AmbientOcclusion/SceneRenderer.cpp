/*
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include <string>
#include "SceneRenderer.h"
#include "BindingHelpers.h"
#include "DirectXMath.h"

#include "shaders\DefaultVS.hlsl.h"
#include "shaders\VoxelizationVS.hlsl.h"
#include "shaders\AttributesPS.hlsl.h"
#include "shaders\FullScreenQuadVS.hlsl.h"
#include "shaders\BlitPS.hlsl.h"
#include "shaders\CompositingPS.hlsl.h"


static const UINT s_ShadowMapSize = 2048;

using namespace DirectX;

SceneRenderer::SceneRenderer(NVRHI::IRendererInterface* pRenderer)
    : m_RendererInterface(pRenderer)
    , m_pScene(NULL)
    , m_Width(0)
    , m_Height(0)
    , m_SampleCount(1)
    , m_pVoxelizationGS(NULL)
    , m_pVoxelizationPS(NULL)
{ }

HRESULT SceneRenderer::LoadMesh(const char* strFileName)
{
    m_pScene = new Scene();
    return m_pScene->Load(strFileName, 0);
}

HRESULT SceneRenderer::AllocateResources(VXGI::IGlobalIllumination* pGI, VXGI::IShaderCompiler* pCompiler)
{
     CREATE_SHADER(VERTEX, g_DefaultVS, &m_pDefaultVS);

    const NVRHI::VertexAttributeDesc SceneLayout[] =
    {
        { "POSITION", 0, NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, position), false },
        { "TEXCOORD", 0, NVRHI::Format::RG32_FLOAT,  0, offsetof(VertexBufferEntry, texCoord), false },
        { "NORMAL",   0, NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, normal),   false },
        { "TANGENT",  0, NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, tangent),  false },
        { "BINORMAL", 0, NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, binormal), false },
    };

    m_RendererInterface->createInputLayout(SceneLayout, _countof(SceneLayout), g_DefaultVS, sizeof(g_DefaultVS), &m_pInputLayout);

    CREATE_SHADER(VERTEX, g_VoxelizationVS, &m_pVoxelizationVS);
    CREATE_SHADER(VERTEX, g_FullScreenQuadVS, &m_pFullScreenQuadVS);
    CREATE_SHADER(PIXEL, g_AttributesPS, &m_pAttributesPS);
    CREATE_SHADER(PIXEL, g_BlitPS, &m_pBlitPS);
    CREATE_SHADER(PIXEL, g_CompositingPS, &m_pCompositingPS);

    m_RendererInterface->createConstantBuffer(NVRHI::ConstantBufferDesc(sizeof(GlobalConstants), nullptr), nullptr, &m_pGlobalCBuffer);

    NVRHI::SamplerDesc samplerDesc;
    samplerDesc.wrapMode[0] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[1] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[2] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.minFilter = samplerDesc.magFilter = samplerDesc.mipFilter = true;
    samplerDesc.anisotropy = 16;
    m_RendererInterface->createSampler(samplerDesc, &m_pDefaultSamplerState);

    NVRHI::TextureDesc nullTextureDesc;
    nullTextureDesc.width = 1;
    nullTextureDesc.height = 1;
    nullTextureDesc.format = NVRHI::Format::RGBA8_UNORM;
    nullTextureDesc.debugName = "NullTexture";
    uint32_t nullData = 0;
    m_RendererInterface->createTexture(nullTextureDesc, &nullData, &m_NullTexture);

    uint32_t whiteData = ~0u;
    m_RendererInterface->createTexture(nullTextureDesc, &whiteData, &m_WhiteDummy);

    if (FAILED(m_pScene->InitResources(m_RendererInterface)))
        return E_FAIL;

    VXGI::IBlob* blob = nullptr;

    if (VXGI_FAILED(pCompiler->compileVoxelizationGeometryShaderFromVS(&blob, g_VoxelizationVS, sizeof(g_VoxelizationVS))))
        return E_FAIL;

    if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationGS, blob->getData(), blob->getSize())))
        return E_FAIL;

    blob->dispose();

    if (VXGI_FAILED(pCompiler->compileVoxelizationDefaultPixelShader(&blob)))
        return E_FAIL;

    if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationPS, blob->getData(), blob->getSize())))
        return E_FAIL;

    blob->dispose();

    return S_OK;
}

void SceneRenderer::AllocateViewDependentResources(UINT width, UINT height, UINT sampleCount)
{
    m_Width = width;
    m_Height = height;
    m_SampleCount = sampleCount;

    NVRHI::TextureDesc gbufferDesc;
    gbufferDesc.width = m_Width;
    gbufferDesc.height = m_Height;
    gbufferDesc.isRenderTarget = true;
    gbufferDesc.useClearValue = true;
    gbufferDesc.sampleCount = m_SampleCount;
    gbufferDesc.disableGPUsSync = true;

    gbufferDesc.format = NVRHI::Format::RGBA8_UNORM;
    gbufferDesc.clearValue = NVRHI::Color(0.f);
    gbufferDesc.debugName = "GbufferAlbedo";
    m_RendererInterface->createTexture(gbufferDesc, NULL, &m_TargetAlbedo);

    gbufferDesc.format = NVRHI::Format::RGBA16_FLOAT;
    gbufferDesc.clearValue = NVRHI::Color(0.f);
    gbufferDesc.debugName = "GbufferNormals";
    m_RendererInterface->createTexture(gbufferDesc, NULL, &m_TargetNormal);
    m_RendererInterface->createTexture(gbufferDesc, NULL, &m_TargetNormalPrev);

    gbufferDesc.format = NVRHI::Format::D24S8;
    gbufferDesc.clearValue = NVRHI::Color(1.f, 0.f, 0.f, 0.f);
    gbufferDesc.debugName = "GbufferDepth";
    m_RendererInterface->createTexture(gbufferDesc, NULL, &m_TargetDepth);
    m_RendererInterface->createTexture(gbufferDesc, NULL, &m_TargetDepthPrev);
}

void SceneRenderer::ReleaseResources(VXGI::IGlobalIllumination* pGI)
{
    if (m_pScene)
    {
        delete m_pScene;
        m_pScene = NULL;
    }

    if (m_pVoxelizationGS)
    {
        pGI->destroyUserDefinedShaderSet(m_pVoxelizationGS);
        m_pVoxelizationGS = NULL;
    }

    if (m_pVoxelizationPS)
    {
        pGI->destroyUserDefinedShaderSet(m_pVoxelizationPS);
        m_pVoxelizationPS = NULL;
    }
}

void SceneRenderer::ReleaseViewDependentResources()
{
    m_TargetAlbedo = nullptr;
    m_TargetNormal = nullptr;
    m_TargetNormalPrev = nullptr;
    m_TargetDepth = nullptr;
    m_TargetDepthPrev = nullptr;
}

void SceneRenderer::RenderToGBuffer(const VXGI::float4x4& viewProjMatrix)
{
    std::swap(m_TargetNormal, m_TargetNormalPrev);
    std::swap(m_TargetDepth, m_TargetDepthPrev);

    NVRHI::DrawCallState state;

    MaterialCallback onChangeMaterial = [this, &state](const MeshMaterialInfo& material)
    {
        NVRHI::BindTexture(state.PS, 0, material.diffuseTexture ? material.diffuseTexture : m_NullTexture, false, NVRHI::Format::UNKNOWN, ~0u);
        NVRHI::BindTexture(state.PS, 1, material.normalsTexture ? material.normalsTexture : m_NullTexture, false, NVRHI::Format::UNKNOWN, ~0u);
        NVRHI::BindTexture(state.PS, 2, material.opacityTexture ? material.opacityTexture : m_WhiteDummy, false, NVRHI::Format::UNKNOWN, ~0u);
    };

    state.PS.shader = m_pAttributesPS;
    NVRHI::BindSampler(state.PS, 0, m_pDefaultSamplerState);

    state.renderState.targetCount = 2;
    state.renderState.targets[0] = m_TargetAlbedo;
    state.renderState.targets[1] = m_TargetNormal;
    state.renderState.clearColorTarget = true;
    state.renderState.depthTarget = m_TargetDepth;
    state.renderState.clearDepthTarget = true;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));

    RenderSceneCommon(state, NULL, NULL, 0, viewProjMatrix, &onChangeMaterial, false);
}

void SceneRenderer::RenderSceneCommon(
    NVRHI::DrawCallState& state,
    VXGI::IGlobalIllumination* pGI,
    const VXGI::Box3f *clippingBoxes,
    uint32_t numBoxes,
    const VXGI::float4x4& viewProjMatrix,
    MaterialCallback* onChangeMaterial,
    bool voxelization)
{
    GlobalConstants globalConstants;
    globalConstants.viewProjMatrix = viewProjMatrix;
    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));

    state.inputLayout = m_pInputLayout;
    state.primType = NVRHI::PrimitiveType::TRIANGLE_LIST;
    state.indexBufferFormat = NVRHI::Format::R32_UINT;
    state.indexBuffer = m_pScene->GetIndexBuffer();

    state.vertexBufferCount = 1;
    state.vertexBuffers[0].buffer = m_pScene->GetVertexBuffer();
    state.vertexBuffers[0].slot = 0; 
    state.vertexBuffers[0].stride = sizeof(VertexBufferEntry);
    
    NVRHI::BindConstantBuffer(state.VS, 0, m_pGlobalCBuffer);

    m_RendererInterface->beginRenderingPass();

    if (voxelization)
    {
        state.VS.shader = m_pVoxelizationVS;
        pGI->beginVoxelizationDrawCallGroup();
    }
    else
        state.VS.shader = m_pDefaultVS;


    int lastMaterial = -2;
    VXGI::MaterialInfo lastMaterialInfo;

    UINT numMeshes = m_pScene->GetMeshesNum();

    std::vector<NVRHI::DrawArguments> drawCalls;

    for (UINT i = 0; i < numMeshes; ++i)
    {
        VXGI::Box3f meshBounds = m_pScene->GetMeshBounds(i);

        if (clippingBoxes && numBoxes)
        {
            bool contained = false;
            for (UINT clipbox = 0; clipbox < numBoxes; ++clipbox)
            {
                if (clippingBoxes[clipbox].intersectsWith(meshBounds))
                {
                    contained = true;
                    break;
                }
            }

            if (!contained)
                continue;
        }

        int material = m_pScene->GetMaterialIndex(i);

        if (material != lastMaterial)
        {
            if (!drawCalls.empty())
            {
                m_RendererInterface->drawIndexed(state, &drawCalls[0], uint32_t(drawCalls.size()));
                drawCalls.clear();

                state.renderState.clearDepthTarget = false;
                state.renderState.clearColorTarget = false;
            }

            MeshMaterialInfo materialInfo;
            GetMaterialInfo(i, materialInfo);

            if (voxelization)
            {
                if (lastMaterial < 0 || lastMaterialInfo != materialInfo)
                {
                    NVRHI::DrawCallState voxelizationState;
                    if (VXGI_FAILED(pGI->getVoxelizationState(materialInfo, false, voxelizationState)))
                        continue;
                    
                    state.GS = voxelizationState.GS;
                    state.PS = voxelizationState.PS;
                    state.renderState = voxelizationState.renderState;
                    state.renderState.rasterState.frontCounterClockwise = true;
                }
            }
            
            if (onChangeMaterial)
            {
                (*onChangeMaterial)(materialInfo);
            }

            lastMaterial = material;
            lastMaterialInfo = materialInfo;
        }

        drawCalls.push_back(m_pScene->GetMeshDrawArguments(i));
    }

    if (!drawCalls.empty())
    {
        m_RendererInterface->drawIndexed(state, &drawCalls[0], uint32_t(drawCalls.size()));
    }

    if (voxelization)
    {
        pGI->endVoxelizationDrawCallGroup();
    }

    m_RendererInterface->endRenderingPass();
}

void SceneRenderer::GetMaterialInfo(UINT meshID, OUT MeshMaterialInfo& materialInfo)
{
    materialInfo.diffuseTexture = m_pScene->GetTextureSRV(aiTextureType_DIFFUSE, meshID);
    materialInfo.normalsTexture = m_pScene->GetTextureSRV(aiTextureType_NORMALS, meshID);
    materialInfo.opacityTexture = m_pScene->GetTextureSRV(aiTextureType_OPACITY, meshID);

    materialInfo.diffuseColor = m_pScene->GetColor(aiTextureType_DIFFUSE, meshID);

    materialInfo.geometryShader = m_pVoxelizationGS;
    materialInfo.pixelShader = m_pVoxelizationPS;
}

void SceneRenderer::FillTracingInputBuffers(VXGI::IBasicViewTracer::InputBuffers& inputBuffers)
{
    inputBuffers.gbufferViewport = NVRHI::Viewport(float(m_Width), float(m_Height));
    inputBuffers.gbufferDepth = m_TargetDepth;
    inputBuffers.gbufferNormal = m_TargetNormal;
}

NVRHI::TextureHandle SceneRenderer::GetAlbedoBufferHandle()
{
    return m_TargetAlbedo;
}

void SceneRenderer::Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest)
{
    NVRHI::DrawCallState state;

    state.primType = NVRHI::PrimitiveType::TRIANGLE_STRIP;
    state.VS.shader = m_pFullScreenQuadVS;
    state.PS.shader = m_pBlitPS;

    state.renderState.targetCount = 1;
    state.renderState.targets[0] = pDest;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));
    state.renderState.depthStencilState.depthEnable = false;
    state.renderState.rasterState.cullMode = NVRHI::RasterState::CULL_NONE;

    NVRHI::BindTexture(state.PS, 0, pSource);

    NVRHI::DrawArguments args;
    args.vertexCount = 4;
    m_RendererInterface->draw(state, &args, 1);
}

void SceneRenderer::ComposeAO(NVRHI::TextureHandle pVXAO, NVRHI::TextureHandle pSSAO, NVRHI::TextureHandle pDest, bool visualizeAO)
{
    GlobalConstants globalConstants;
    globalConstants.visualizeAO = visualizeAO;
    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));

    NVRHI::DrawCallState state;

    state.primType = NVRHI::PrimitiveType::TRIANGLE_STRIP;
    state.VS.shader = m_pFullScreenQuadVS;
    state.PS.shader = m_pCompositingPS;

    state.renderState.targetCount = 1;
    state.renderState.targets[0] = pDest;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));
    state.renderState.depthStencilState.depthEnable = false;
    state.renderState.rasterState.cullMode = NVRHI::RasterState::CULL_NONE;

    NVRHI::BindTexture(state.PS, 0, pVXAO ? pVXAO : m_NullTexture);
    NVRHI::BindTexture(state.PS, 1, pSSAO ? pSSAO : m_NullTexture);
    NVRHI::BindTexture(state.PS, 2, m_TargetAlbedo);
    NVRHI::BindConstantBuffer(state.PS, 0, m_pGlobalCBuffer);

    NVRHI::DrawArguments args;
    args.vertexCount = 4;
    m_RendererInterface->draw(state, &args, 1);
}
