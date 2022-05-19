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
#include "shaders\AttributesPS.hlsl.h"
#include "shaders\FullScreenQuadVS.hlsl.h"
#include "shaders\BlitPS.hlsl.h"
#include "shaders\BlitLdrPS.hlsl.h"
#include "shaders\CompositingPS.hlsl.h"

static const UINT s_ShadowMapSize = 2048;

using namespace DirectX;

SceneRenderer::SceneRenderer(NVRHI::IRendererInterface* pRenderer)
    : m_RendererInterface(pRenderer)
    , m_pScene(NULL)
    , m_pTransparentScene(NULL)
    , m_Width(0)
    , m_Height(0)
    , m_SampleCount(1)
    , m_pVoxelizationGS(NULL)
    , m_pVoxelizationPS(NULL)
    , m_pTransparentGeometryPS(NULL)
{ }

HRESULT SceneRenderer::LoadMesh(const char* strFileName)
{
    m_pScene = new Scene();
    HRESULT result = m_pScene->Load(strFileName, 0);

    if (FAILED(result))
    {
        delete m_pScene;
        m_pScene = nullptr;
    }

    return result;
}

HRESULT SceneRenderer::LoadTransparentMesh(const char* strFileName)
{
    m_pTransparentScene = new Scene();
    HRESULT result = m_pTransparentScene->Load(strFileName, 0);

    if (FAILED(result))
    {
        delete m_pTransparentScene;
        m_pTransparentScene = nullptr;
    }
    else
    {
        m_pTransparentScene->m_WorldMatrix = VXGI::float4x4(
            7.f, 0.f, 0.f, -700.f,
            0.f, 7.f, 0.f, 0.f,
            0.f, 0.f, 7.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        );
    }

    return result;
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

    CREATE_SHADER(VERTEX, g_FullScreenQuadVS, &m_pFullScreenQuadVS);
    CREATE_SHADER(PIXEL, g_AttributesPS, &m_pAttributesPS);
    CREATE_SHADER(PIXEL, g_BlitPS, &m_pBlitPS);
    CREATE_SHADER(PIXEL, g_BlitLdrPS, &m_pBlitLdrPS);
    CREATE_SHADER(PIXEL, g_CompositingPS, &m_pCompositingPS);

    m_RendererInterface->createConstantBuffer(NVRHI::ConstantBufferDesc(sizeof(GlobalConstants), nullptr), nullptr, &m_pGlobalCBuffer);

    NVRHI::SamplerDesc samplerDesc;
    samplerDesc.wrapMode[0] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[1] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[2] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.minFilter = samplerDesc.magFilter = samplerDesc.mipFilter = true;
    samplerDesc.anisotropy = 16;
    m_RendererInterface->createSampler(samplerDesc, &m_pDefaultSamplerState);

    NVRHI::SamplerDesc samplerComparisonDesc;
    samplerComparisonDesc.wrapMode[0] = NVRHI::SamplerDesc::WRAP_MODE_BORDER;
    samplerComparisonDesc.wrapMode[1] = NVRHI::SamplerDesc::WRAP_MODE_BORDER;
    samplerComparisonDesc.wrapMode[2] = NVRHI::SamplerDesc::WRAP_MODE_BORDER;
    samplerComparisonDesc.minFilter = samplerDesc.magFilter = true;
    samplerComparisonDesc.shadowCompare = true;
    samplerComparisonDesc.borderColor = NVRHI::Color(0.f);
    m_RendererInterface->createSampler(samplerComparisonDesc, &m_pComparisonSamplerState);


    NVRHI::TextureDesc shadowMapDesc;
    shadowMapDesc.width = s_ShadowMapSize;
    shadowMapDesc.height = s_ShadowMapSize;
    shadowMapDesc.isRenderTarget = true;
    shadowMapDesc.useClearValue = true;
    shadowMapDesc.clearValue = NVRHI::Color(1.f, 0.f, 0.f, 0.f);
    shadowMapDesc.debugName = "ShadowMap";
    shadowMapDesc.format = NVRHI::Format::D24S8;
    shadowMapDesc.disableGPUsSync = true;
    m_RendererInterface->createTexture(shadowMapDesc, NULL, &m_ShadowMap);

    NVRHI::TextureDesc nullTextureDesc;
    nullTextureDesc.width = 1;
    nullTextureDesc.height = 1;
    nullTextureDesc.format = NVRHI::Format::RGBA8_UNORM;
    nullTextureDesc.debugName = "NullTexture";
    uint32_t nullData = 0;
    m_RendererInterface->createTexture(nullTextureDesc, &nullData, &m_NullTexture);

    if (FAILED(m_pScene->InitResources(m_RendererInterface)))
        return E_FAIL;

    if (FAILED(m_pTransparentScene->InitResources(m_RendererInterface)))
        return E_FAIL;

    if (VXGI_FAILED(CreateVoxelizationGS(pCompiler, pGI)))
        return E_FAIL;

    if (VXGI_FAILED(CreateVoxelizationPS(pCompiler, pGI)))
        return E_FAIL;

    if (VXGI_FAILED(CreateTransparentGeometryPS(pCompiler, pGI)))
        return E_FAIL;

    return S_OK;
}

HRESULT SceneRenderer::CreateVoxelizationGS(VXGI::IShaderCompiler* pCompiler, VXGI::IGlobalIllumination* pGI)
{
    VXGI::IBlob* blob = nullptr;

    if (VXGI_FAILED(pCompiler->compileVoxelizationGeometryShaderFromVS(&blob, g_DefaultVS, sizeof(g_DefaultVS))))
        return E_FAIL;

    if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationGS, blob->getData(), blob->getSize())))
        return E_FAIL;

    blob->dispose();

    return S_OK;
}

HRESULT SceneRenderer::CreateVoxelizationPS(VXGI::IShaderCompiler* pCompiler, VXGI::IGlobalIllumination* pGI)
{
    HRSRC resource = FindResourceA(NULL, "VoxelizationPS", "TEXTFILE");

    if (resource == NULL) // should never happen if the app is built correctly
        return E_FAIL;

    void* data = LockResource(LoadResource(NULL, resource));
    uint32_t size = SizeofResource(NULL, resource);

    // Enumerate resource slots (constant buffers, textures, samplers, potentially UAVs)
    // that are used by the user part of the voxelization pixel shader
    VXGI::ShaderResources resources;
    resources.constantBufferCount = 1;
    resources.constantBufferSlots[0] = 0;
    resources.textureCount = 2;
    resources.textureSlots[0] = 0;
    resources.textureSlots[1] = 1;
    resources.samplerCount = 2;
    resources.samplerSlots[0] = 0;
    resources.samplerSlots[1] = 1;

    VXGI::IBlob* blob = nullptr;

    if (VXGI_FAILED(pCompiler->compileVoxelizationPixelShader(&blob, (char*)data, size, "main", resources)))
        return E_FAIL;

    if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationPS, blob->getData(), blob->getSize())))
        return E_FAIL;

    blob->dispose();

    return S_OK;
}

HRESULT SceneRenderer::CreateTransparentGeometryPS(VXGI::IShaderCompiler* pCompiler, VXGI::IGlobalIllumination* pGI)
{
    HRSRC resource = FindResourceA(NULL, "TransparentGeometryPS", "TEXTFILE");

    if (resource == NULL) // should never happen if the app is built correctly
        return E_FAIL;

    void* data = LockResource(LoadResource(NULL, resource));
    uint32_t size = SizeofResource(NULL, resource);

    // Enumerate resource slots (constant buffers, textures, samplers, potentially UAVs)
    // that are used by the user part of the voxelization pixel shader
    VXGI::ShaderResources resources;
    resources.constantBufferCount = 1;
    resources.constantBufferSlots[0] = 0;

    VXGI::IBlob* blob = nullptr;

    if (VXGI_FAILED(pCompiler->compileConeTracingPixelShader(&blob, (char*)data, size, "main", resources)))
        return E_FAIL;

    if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pTransparentGeometryPS, blob->getData(), blob->getSize())))
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

    if (m_pTransparentGeometryPS)
    {
        pGI->destroyUserDefinedShaderSet(m_pTransparentGeometryPS);
        m_pTransparentGeometryPS = NULL;
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

void SceneRenderer::SetLightDirection(VXGI::float3 direction)
{
    m_LightDirection = direction.normalize();
}

XMVECTOR XMVectorSet(VXGI::float3 v)
{
    return XMVectorSet(v.x, v.y, v.z, 0.f);
}

void SceneRenderer::RenderShadowMap(const VXGI::float3 cameraPosition, float lightSize, bool drawTransparent)
{
    const float lightRange = lightSize * 10.0f;

    XMVECTOR lightPos = XMVectorSet(cameraPosition - m_LightDirection * lightRange * 0.5f);
    XMVECTOR lookAt = XMVectorSet(cameraPosition);
    XMVECTOR lightUp = XMVectorSet(0, 1.0f, 0, 0);

    XMMATRIX viewMatrix = XMMatrixLookAtLH(lightPos, lookAt, lightUp);
    XMMATRIX projMatrix = XMMatrixOrthographicLH(lightSize, lightSize, lightRange * 0.01f, lightRange);
    XMMATRIX viewProjMatrix = viewMatrix * projMatrix;
    memcpy(&m_LightViewMatrix, &viewMatrix, sizeof(viewMatrix));
    memcpy(&m_LightProjMatrix, &projMatrix, sizeof(projMatrix));
    memcpy(&m_LightViewProjMatrix, &viewProjMatrix, sizeof(viewProjMatrix));
    
    NVRHI::DrawCallState state;

    state.renderState.depthTarget = m_ShadowMap;
    state.renderState.clearDepthTarget = true;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(s_ShadowMapSize), float(s_ShadowMapSize));
    state.renderState.rasterState.cullMode = NVRHI::RasterState::CULL_NONE;
    state.renderState.rasterState.depthBias = 16;
    state.renderState.rasterState.slopeScaledDepthBias = 4.0f;

    GlobalConstants constants = {};
    constants.viewProjMatrix = m_LightViewProjMatrix;

    RenderSceneCommon(m_pScene, state, NULL, NULL, 0, constants, NULL, false);

    if(drawTransparent)
        RenderSceneCommon(m_pTransparentScene, state, NULL, NULL, 0, constants, NULL, false);
}

void SceneRenderer::RenderToGBuffer(const VXGI::float4x4& viewProjMatrix, VXGI::float3 cameraPos, bool drawTransparent)
{
    std::swap(m_TargetNormal, m_TargetNormalPrev);
    std::swap(m_TargetDepth, m_TargetDepthPrev);

    NVRHI::DrawCallState state;

    MaterialCallback onChangeMaterial = [this, &state](const MeshMaterialInfo& material)
    {
        NVRHI::BindTexture(state.PS, 0, material.diffuseTexture ? material.diffuseTexture : m_NullTexture, false, NVRHI::Format::UNKNOWN, ~0u);
        NVRHI::BindTexture(state.PS, 1, material.specularTexture ? material.specularTexture : m_NullTexture, false, NVRHI::Format::UNKNOWN, ~0u);
        NVRHI::BindTexture(state.PS, 2, material.normalsTexture ? material.normalsTexture : m_NullTexture, false, NVRHI::Format::UNKNOWN, ~0u);
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

    GlobalConstants constants = {};
    constants.viewProjMatrix = viewProjMatrix;
    constants.cameraPos = cameraPos;

    RenderSceneCommon(m_pScene, state, NULL, NULL, 0, constants, &onChangeMaterial, false);

    if (drawTransparent)
        RenderSceneCommon(m_pTransparentScene, state, NULL, NULL, 0, constants, &onChangeMaterial, false);
}

void SceneRenderer::RenderTransparentScene(VXGI::IGlobalIllumination* pGI, NVRHI::TextureHandle pDest, const VXGI::float4x4& viewProjMatrix, VXGI::float3 cameraPos, float transparentRoughness, float transparentReflectance)
{
    NVRHI::DrawCallState state;

    if (VXGI_FAILED(pGI->setupUserDefinedConeTracingPixelShaderState(m_pTransparentGeometryPS, state)))
        return;

    NVRHI::BindConstantBuffer(state.PS, 0, m_pGlobalCBuffer);

    state.renderState.targetCount = 1;
    state.renderState.targets[0] = pDest;
    state.renderState.viewportCount = 1;
    state.renderState.viewports[0] = NVRHI::Viewport(float(m_Width), float(m_Height));
    state.renderState.depthTarget = m_TargetDepth;
    state.renderState.depthStencilState.depthEnable = true;
    state.renderState.depthStencilState.depthWriteMask = NVRHI::DepthStencilState::DEPTH_WRITE_MASK_ZERO;
    state.renderState.depthStencilState.depthFunc = NVRHI::DepthStencilState::COMPARISON_EQUAL;

    GlobalConstants constants = {};
    constants.viewProjMatrix = viewProjMatrix;
    constants.cameraPos = cameraPos;
    constants.transparentRoughness = transparentRoughness;
    constants.transparentReflectance = transparentReflectance;

    RenderSceneCommon(m_pTransparentScene, state, pGI, nullptr, 0, constants, nullptr, false);
}

void SceneRenderer::RenderSceneCommon(
    Scene* pScene,
    NVRHI::DrawCallState& state,
    VXGI::IGlobalIllumination* pGI,
    const VXGI::Box3f *clippingBoxes,
    uint32_t numBoxes,
    const GlobalConstants& constants,
    MaterialCallback* onChangeMaterial,
    bool voxelization)
{
    static const UINT SRV_SLOT_DIFFUSE_TEXTURE = 0;
    static const UINT SRV_SLOT_SHADOW_MAP = 1;

    GlobalConstants globalConstants = constants;
    globalConstants.worldMatrix = pScene->m_WorldMatrix;
    globalConstants.lightMatrix = (VXGI::float4x4&)m_LightViewProjMatrix;
    globalConstants.lightDirection = (VXGI::float3&)m_LightDirection;
    globalConstants.diffuseColor = VXGI::float4(0.f);
    globalConstants.lightColor = VXGI::float4(1.f);
    globalConstants.rShadowMapSize = 1.0f / s_ShadowMapSize;
    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));

    state.inputLayout = m_pInputLayout;
    state.primType = NVRHI::PrimitiveType::TRIANGLE_LIST;
    state.indexBufferFormat = NVRHI::Format::R32_UINT;
    state.indexBuffer = pScene->GetIndexBuffer();

    state.vertexBufferCount = 1;
    state.vertexBuffers[0].buffer = pScene->GetVertexBuffer();
    state.vertexBuffers[0].slot = 0; 
    state.vertexBuffers[0].stride = sizeof(VertexBufferEntry);

    state.VS.shader = m_pDefaultVS;
    NVRHI::BindConstantBuffer(state.VS, 0, m_pGlobalCBuffer);

    m_RendererInterface->beginRenderingPass();

    if (voxelization)
    {
        pGI->beginVoxelizationDrawCallGroup();
    }

    int lastMaterial = -2;
    VXGI::MaterialInfo lastMaterialInfo;

    UINT numMeshes = pScene->GetMeshesNum();

    std::vector<NVRHI::DrawArguments> drawCalls;

    for (UINT i = 0; i < numMeshes; ++i)
    {
        VXGI::Box3f meshBounds = pScene->GetMeshBounds(i);

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

        int material = pScene->GetMaterialIndex(i);

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
            GetMaterialInfo(pScene, i, materialInfo);

            if (voxelization)
            {
                if (lastMaterial < 0 || lastMaterialInfo != materialInfo)
                {
                    NVRHI::DrawCallState voxelizationState;
                    if (VXGI_FAILED(pGI->getVoxelizationState(materialInfo, true, voxelizationState)))
                        continue;
                    
                    state.GS = voxelizationState.GS;
                    state.PS = voxelizationState.PS;
                    state.renderState = voxelizationState.renderState;
                    state.renderState.rasterState.frontCounterClockwise = true;

                    NVRHI::BindTexture(state.PS, SRV_SLOT_SHADOW_MAP, m_ShadowMap);
                    NVRHI::BindSampler(state.PS, 0, m_pDefaultSamplerState);
                    NVRHI::BindSampler(state.PS, 1, m_pComparisonSamplerState);
                    NVRHI::BindConstantBuffer(state.PS, 0, m_pGlobalCBuffer);

                    globalConstants.diffuseColor = VXGI::float4(materialInfo.diffuseColor, materialInfo.diffuseTexture ? 1.f : 0.f);
                    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));
                }

                NVRHI::BindTexture(state.PS, SRV_SLOT_DIFFUSE_TEXTURE, materialInfo.diffuseTexture ? materialInfo.diffuseTexture : m_NullTexture, false, NVRHI::Format::UNKNOWN, ~0u);
            }

            if (onChangeMaterial)
            {
                (*onChangeMaterial)(materialInfo);
            }

            lastMaterial = material;
            lastMaterialInfo = materialInfo;
        }

        drawCalls.push_back(pScene->GetMeshDrawArguments(i));
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

void SceneRenderer::RenderForVoxelization(NVRHI::DrawCallState& state, VXGI::IGlobalIllumination* pGI, const VXGI::Box3f *clippingBoxes, uint32_t numBoxes, const VXGI::float4x4& viewProjMatrix, MaterialCallback* onChangeMaterial)
{
    GlobalConstants constants = {};
    constants.viewProjMatrix = viewProjMatrix;

    RenderSceneCommon(m_pScene, state, pGI, clippingBoxes, numBoxes, constants, onChangeMaterial, true);
}

void SceneRenderer::GetMaterialInfo(Scene* pScene, UINT meshID, OUT MeshMaterialInfo& materialInfo)
{
    materialInfo.diffuseTexture = pScene->GetTextureSRV(aiTextureType_DIFFUSE, meshID);
    materialInfo.specularTexture = pScene->GetTextureSRV(aiTextureType_SPECULAR, meshID);
    materialInfo.normalsTexture = pScene->GetTextureSRV(aiTextureType_NORMALS, meshID);

    materialInfo.diffuseColor = pScene->GetColor(aiTextureType_DIFFUSE, meshID);

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

void SceneRenderer::Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest, bool convertToLdr)
{
    NVRHI::DrawCallState state;

    state.primType = NVRHI::PrimitiveType::TRIANGLE_STRIP;
    state.VS.shader = m_pFullScreenQuadVS;
    state.PS.shader = convertToLdr ? m_pBlitLdrPS : m_pBlitPS;

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

void SceneRenderer::Shade(NVRHI::TextureHandle indirectDiffuse, NVRHI::TextureHandle indirectSpecular, NVRHI::TextureHandle indirectConfidence, NVRHI::TextureHandle pDest, const VXGI::float4x4& viewProjMatrix, VXGI::float3 ambientColor)
{
    GlobalConstants CB;
    memset(&CB, 0, sizeof(CB));
    CB.viewProjMatrix = viewProjMatrix;
    CB.viewProjMatrixInv = viewProjMatrix.invert();
    CB.lightMatrix = m_LightViewProjMatrix;
    CB.lightDirection = m_LightDirection;
    CB.lightColor = VXGI::float4(1.f);
    CB.ambientColor = ambientColor;
    CB.rShadowMapSize = 1.0f / s_ShadowMapSize;
    CB.enableIndirectDiffuse = indirectDiffuse ? 1 : 0;
    CB.enableIndirectSpecular = indirectSpecular ? 1 : 0;
    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &CB, sizeof(CB));


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

    NVRHI::BindConstantBuffer(state.PS, 0, m_pGlobalCBuffer);
    NVRHI::BindTexture(state.PS, 0, m_TargetAlbedo);
    NVRHI::BindTexture(state.PS, 1, m_TargetNormal);
    NVRHI::BindTexture(state.PS, 2, m_TargetDepth);
    NVRHI::BindTexture(state.PS, 3, indirectDiffuse ? indirectDiffuse : m_NullTexture);
    NVRHI::BindTexture(state.PS, 4, indirectSpecular ? indirectSpecular : m_NullTexture);
    NVRHI::BindTexture(state.PS, 5, indirectConfidence ? indirectConfidence : m_NullTexture);
    NVRHI::BindTexture(state.PS, 6, m_ShadowMap);
    NVRHI::BindSampler(state.PS, 0, m_pComparisonSamplerState);

    NVRHI::DrawArguments args;
    args.vertexCount = 4;
    m_RendererInterface->draw(state, &args, 1);
}

VXGI::Frustum SceneRenderer::GetLightFrustum()
{
    return VXGI::Frustum(m_LightViewProjMatrix);
}
