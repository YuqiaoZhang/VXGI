/*
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
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

#if USE_GL4

std::string LoadShader(const char* name)
{
    HRSRC resource = FindResourceA(NULL, name, "TEXTFILE");

    if (resource == nullptr) // should never happen if the app is built correctly
        return nullptr;

    void* data = LockResource(LoadResource(NULL, resource));
    uint32_t size = SizeofResource(NULL, resource);

    return std::string((const char*)data, size);
}

const std::string g_DefaultVS = LoadShader("DefaultVS");
const std::string g_AttributesPS = LoadShader("AttributesPS");
const std::string g_FullScreenQuadVS = LoadShader("FullScreenQuadVS");
const std::string g_BlitPS = LoadShader("BlitPS");
const std::string g_CompositingPS = LoadShader("CompositingPS");

#else
#include "shaders\DefaultVS.hlsl.h"
#include "shaders\AttributesPS.hlsl.h"
#include "shaders\FullScreenQuadVS.hlsl.h"
#include "shaders\BlitPS.hlsl.h"
#include "shaders\CompositingPS.hlsl.h"
#endif


static const UINT s_ShadowMapSize = 2048;

using namespace DirectX;

SceneRenderer::SceneRenderer(NVRHI::IRendererInterface* pRenderer)
    : m_RendererInterface(pRenderer)
    , m_pScene(NULL)
    , m_pInputLayout(NULL)
    , m_pDefaultVS(NULL)
    , m_pFullScreenQuadVS(NULL)
    , m_pAttributesPS(NULL)
    , m_pBlitPS(NULL)
    , m_pCompositingPS(NULL)
    , m_pGlobalCBuffer(NULL)
    , m_pDefaultSamplerState(NULL)
    , m_pComparisonSamplerState(NULL)
    , m_Width(0)
    , m_Height(0)
    , m_SampleCount(1)
    , m_pVoxelizationGS(NULL)
    , m_pVoxelizationPS(NULL)
    , m_TargetAlbedo(NULL)
    , m_TargetNormal(NULL)
    , m_TargetDepth(NULL)
    , m_ShadowMap(NULL)
    , m_NullTexture(NULL)
{ }

HRESULT SceneRenderer::LoadMesh(const char* strFileName)
{
    m_pScene = new Scene();
    return m_pScene->Load(strFileName, 0);
}

HRESULT SceneRenderer::AllocateResources(VXGI::IGlobalIllumination* pGI, VXGI::IShaderCompiler* pCompiler)
{
    m_pDefaultVS = CREATE_SHADER(VERTEX, g_DefaultVS);

    const NVRHI::VertexAttributeDesc SceneLayout[] =
    {
        { "POSITION", NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, position), false },
        { "TEXCOORD", NVRHI::Format::RG32_FLOAT,  0, offsetof(VertexBufferEntry, texCoord), false },
        { "NORMAL",   NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, normal),   false },
        { "TANGENT",  NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, tangent),  false },
        { "BINORMAL", NVRHI::Format::RGB32_FLOAT, 0, offsetof(VertexBufferEntry, binormal), false },
    };

#if USE_GL4
    m_pInputLayout = m_RendererInterface->createInputLayout(SceneLayout, _countof(SceneLayout), nullptr, 0);
#else
    m_pInputLayout = m_RendererInterface->createInputLayout(SceneLayout, _countof(SceneLayout), g_DefaultVS, sizeof(g_DefaultVS));
#endif

    m_pFullScreenQuadVS = CREATE_SHADER(VERTEX, g_FullScreenQuadVS);
    m_pAttributesPS = CREATE_SHADER(PIXEL, g_AttributesPS);
    m_pBlitPS = CREATE_SHADER(PIXEL, g_BlitPS);
    m_pCompositingPS = CREATE_SHADER(PIXEL, g_CompositingPS);

    m_pGlobalCBuffer = m_RendererInterface->createConstantBuffer(NVRHI::ConstantBufferDesc(sizeof(GlobalConstants), nullptr), nullptr);

    NVRHI::SamplerDesc samplerDesc;
    samplerDesc.wrapMode[0] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[1] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.wrapMode[2] = NVRHI::SamplerDesc::WRAP_MODE_WRAP;
    samplerDesc.minFilter = samplerDesc.magFilter = samplerDesc.mipFilter = true;
    samplerDesc.anisotropy = 16;
    m_pDefaultSamplerState = m_RendererInterface->createSampler(samplerDesc);

    NVRHI::SamplerDesc samplerComparisonDesc;
    samplerComparisonDesc.wrapMode[0] = NVRHI::SamplerDesc::WRAP_MODE_BORDER;
    samplerComparisonDesc.wrapMode[1] = NVRHI::SamplerDesc::WRAP_MODE_BORDER;
    samplerComparisonDesc.wrapMode[2] = NVRHI::SamplerDesc::WRAP_MODE_BORDER;
    samplerComparisonDesc.minFilter = samplerDesc.magFilter = true;
    samplerComparisonDesc.shadowCompare = true;
    samplerComparisonDesc.borderColor = NVRHI::Color(0.f);
    m_pComparisonSamplerState = m_RendererInterface->createSampler(samplerComparisonDesc);


    NVRHI::TextureDesc shadowMapDesc;
    shadowMapDesc.width = s_ShadowMapSize;
    shadowMapDesc.height = s_ShadowMapSize;
    shadowMapDesc.isRenderTarget = true;
    shadowMapDesc.useClearValue = true;
    shadowMapDesc.clearValue = NVRHI::Color(1.f, 0.f, 0.f, 0.f);
    shadowMapDesc.debugName = "ShadowMap";
    shadowMapDesc.format = NVRHI::Format::D24S8;
    shadowMapDesc.disableGPUsSync = true;
    m_ShadowMap = m_RendererInterface->createTexture(shadowMapDesc, NULL);

    NVRHI::TextureDesc nullTextureDesc;
    nullTextureDesc.width = 1;
    nullTextureDesc.height = 1;
    nullTextureDesc.format = NVRHI::Format::RGBA8_UNORM;
    nullTextureDesc.debugName = "NullTexture";
    uint32_t nullData = 0;
    m_NullTexture = m_RendererInterface->createTexture(nullTextureDesc, &nullData);

    if (FAILED(m_pScene->InitResources(m_RendererInterface)))
        return E_FAIL;

    VXGI::IBlob* blob = nullptr;

#if USE_GL4
    {
        VXGI::VoxelizationGeometryShaderDesc desc;
        desc.pixelShaderInputCount = 5;
        strcpy_s(desc.pixelShaderInputs[0].name, "v_texCoord");   desc.pixelShaderInputs[0].width = 2;
        strcpy_s(desc.pixelShaderInputs[1].name, "v_normal");     desc.pixelShaderInputs[1].width = 3;
        strcpy_s(desc.pixelShaderInputs[2].name, "v_tangent");    desc.pixelShaderInputs[2].width = 3;
        strcpy_s(desc.pixelShaderInputs[3].name, "v_binormal");   desc.pixelShaderInputs[3].width = 3;
        strcpy_s(desc.pixelShaderInputs[4].name, "v_positionWS"); desc.pixelShaderInputs[4].width = 3;

        if (VXGI_FAILED(pCompiler->compileVoxelizationGeometryShader(&blob, desc)))
            return E_FAIL;
    }
#else
    if (VXGI_FAILED(pCompiler->compileVoxelizationGeometryShaderFromVS(&blob, g_DefaultVS, sizeof(g_DefaultVS))))
        return E_FAIL;
#endif
        
    if (VXGI_FAILED(pGI->loadUserDefinedShaderSet(&m_pVoxelizationGS, blob->getData(), blob->getSize())))
        return E_FAIL;

    blob->dispose();
    

#if USE_GL4
    HRSRC resource = FindResourceA(NULL, "VoxelizationPSGL", "TEXTFILE");
#else
    HRSRC resource = FindResourceA(NULL, "VoxelizationPS", "TEXTFILE");
#endif

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

    VXGI::VoxelizationPixelShaderDesc desc;
    desc.userShaderCodeResources = &resources;
    desc.source = (char*)data;
    desc.sourceSize = size;

    if (VXGI_FAILED(pCompiler->compileVoxelizationPixelShader(&blob, desc)))
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
    m_TargetAlbedo = m_RendererInterface->createTexture(gbufferDesc, NULL);

    gbufferDesc.format = NVRHI::Format::RGBA16_FLOAT;
    gbufferDesc.clearValue = NVRHI::Color(0.f);
    gbufferDesc.debugName = "GbufferNormals";
    m_TargetNormal = m_RendererInterface->createTexture(gbufferDesc, NULL);

    gbufferDesc.format = NVRHI::Format::D24S8;
    gbufferDesc.clearValue = NVRHI::Color(1.f, 0.f, 0.f, 0.f);
    gbufferDesc.debugName = "GbufferDepth";
    m_TargetDepth = m_RendererInterface->createTexture(gbufferDesc, NULL);
}

void SceneRenderer::ReleaseResources(VXGI::IGlobalIllumination* pGI)
{
    if (m_pScene)
    {
        delete m_pScene;
        m_pScene = NULL;
    }

    DESTROY_INPUT_LAYOUT(m_pInputLayout);
    DESTROY_SHADER(m_pDefaultVS);
    DESTROY_SHADER(m_pFullScreenQuadVS);
    DESTROY_SHADER(m_pAttributesPS);
    DESTROY_SHADER(m_pBlitPS);
    DESTROY_SHADER(m_pCompositingPS);
    
    DESTROY_CONSTANT_BUFFER(m_pGlobalCBuffer);

    DESTROY_SAMPLER(m_pDefaultSamplerState);
    DESTROY_SAMPLER(m_pComparisonSamplerState);

    DESTROY_TEXTURE(m_ShadowMap);
    DESTROY_TEXTURE(m_NullTexture);

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
    DESTROY_TEXTURE(m_TargetAlbedo);
    DESTROY_TEXTURE(m_TargetNormal);
    DESTROY_TEXTURE(m_TargetDepth);
}

void SceneRenderer::SetLightDirection(VXGI::Vector3f direction)
{
    m_LightDirection = direction.normalize();
}

XMVECTOR XMVectorSet(VXGI::Vector3f v)
{
    return XMVectorSet(v.x, v.y, v.z, 0.f);
}

void SceneRenderer::RenderShadowMap(const VXGI::Vector3f cameraPosition, float lightSize)
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
    state.renderState.rasterState.slopeScaledDepthBias = 4.0f;

    RenderSceneCommon(state, NULL, NULL, 0, m_LightViewProjMatrix, NULL, false, false);
}

void SceneRenderer::RenderToGBuffer(const VXGI::Matrix4f& viewProjMatrix)
{
    NVRHI::DrawCallState state;

    MaterialCallback onChangeMaterial = [this, &state](const MeshMaterialInfo& material)
    {
        NVRHI::BindTextureAndSampler(state.PS, 0, material.diffuseTexture ? material.diffuseTexture : m_NullTexture, m_pDefaultSamplerState);
        NVRHI::BindTextureAndSampler(state.PS, 1, material.specularTexture ? material.specularTexture : m_NullTexture, m_pDefaultSamplerState);
        NVRHI::BindTextureAndSampler(state.PS, 2, material.normalsTexture ? material.normalsTexture : m_NullTexture, m_pDefaultSamplerState);
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

    RenderSceneCommon(state, NULL, NULL, 0, viewProjMatrix, &onChangeMaterial, false, false);
}

void SceneRenderer::RenderSceneCommon(
    NVRHI::DrawCallState& state,
    VXGI::IGlobalIllumination* pGI,
    const VXGI::Box3f *clippingBoxes,
    uint32_t numBoxes,
    const VXGI::Matrix4f& viewProjMatrix,
    MaterialCallback* onChangeMaterial,
    bool voxelization,
    bool emittance)
{
    static const UINT SRV_SLOT_DIFFUSE_TEXTURE = 0;
    static const UINT SRV_SLOT_SHADOW_MAP = 1;

    GlobalConstants globalConstants;
    globalConstants.viewProjMatrix = viewProjMatrix;
    globalConstants.lightMatrix = (VXGI::Matrix4f&)m_LightViewProjMatrix;
    globalConstants.lightDirection = (VXGI::Vector3f&)m_LightDirection;
    globalConstants.diffuseColor = VXGI::Vector4f(0.f);
    globalConstants.lightColor = VXGI::Vector4f(1.f);
    globalConstants.rShadowMapSize = 1.0f / s_ShadowMapSize;
    m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));

    state.inputLayout = m_pInputLayout;
    state.primType = NVRHI::PrimitiveType::TRIANGLE_LIST;
    state.indexBufferFormat = NVRHI::Format::R32_UINT;
    state.indexBuffer = m_pScene->GetIndexBuffer();

    state.vertexBufferCount = 1;
    state.vertexBuffers[0].buffer = m_pScene->GetVertexBuffer();
    state.vertexBuffers[0].slot = 0; 
    state.vertexBuffers[0].stride = sizeof(VertexBufferEntry);

    bool extraStateSetup = false;

    state.VS.shader = m_pDefaultVS;
    NVRHI::BindConstantBuffer(state.VS, 0, m_pGlobalCBuffer);

    if (voxelization)
    {
        pGI->beginVoxelizationDrawCallGroup();
    }

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
                    if (VXGI_FAILED(pGI->getVoxelizationState(materialInfo, voxelizationState)))
                        continue;

                    if (voxelizationState.renderState.setupExtraVoxelizationState && !extraStateSetup)
                    {
                        pGI->setupExtraVoxelizationState();
                        extraStateSetup = true;
                    }

                    state.GS = voxelizationState.GS;
                    state.PS = voxelizationState.PS;
                    state.renderState = voxelizationState.renderState;

                    if (emittance)
                    {
                        NVRHI::BindTextureAndSampler(state.PS, SRV_SLOT_SHADOW_MAP, m_ShadowMap, m_pComparisonSamplerState);
#if !USE_GL4
                        NVRHI::BindSampler(state.PS, 0, m_pDefaultSamplerState);
                        NVRHI::BindSampler(state.PS, 1, m_pComparisonSamplerState);
#endif
                        NVRHI::BindConstantBuffer(state.PS, 0, m_pGlobalCBuffer);

                        globalConstants.diffuseColor = VXGI::Vector4f(materialInfo.diffuseColor, materialInfo.diffuseTexture ? 1.f : 0.f);
                        m_RendererInterface->writeConstantBuffer(m_pGlobalCBuffer, &globalConstants, sizeof(globalConstants));
                    }
                }

                if (emittance)
                {
                    NVRHI::BindTextureAndSampler(state.PS, SRV_SLOT_DIFFUSE_TEXTURE, materialInfo.diffuseTexture ? materialInfo.diffuseTexture : m_NullTexture, m_pDefaultSamplerState);
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
        if (extraStateSetup)
            pGI->removeExtraVoxelizationState();

        pGI->endVoxelizationDrawCallGroup();
    }
}

void SceneRenderer::GetMaterialInfo(UINT meshID, OUT MeshMaterialInfo& materialInfo)
{
    materialInfo.diffuseTexture = m_pScene->GetTextureSRV(aiTextureType_DIFFUSE, meshID);
    materialInfo.specularTexture = m_pScene->GetTextureSRV(aiTextureType_SPECULAR, meshID);
    materialInfo.normalsTexture = m_pScene->GetTextureSRV(aiTextureType_NORMALS, meshID);

    materialInfo.diffuseColor = m_pScene->GetColor(aiTextureType_DIFFUSE, meshID);

    materialInfo.voxelizationThickness = 1.0f;
    materialInfo.twoSided = false;

    materialInfo.geometryShader = m_pVoxelizationGS;
    materialInfo.pixelShader = m_pVoxelizationPS;
}

void SceneRenderer::FillTracingInputBuffers(VXGI::IViewTracer::InputBuffers& inputBuffers)
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

void SceneRenderer::Shade(NVRHI::TextureHandle indirectDiffuse, NVRHI::TextureHandle indirectSpecular, NVRHI::TextureHandle pDest, const VXGI::Matrix4f& viewProjMatrix, VXGI::Vector3f ambientColor)
{
    GlobalConstants CB;
    memset(&CB, 0, sizeof(CB));
    CB.viewProjMatrix = viewProjMatrix;
    CB.viewProjMatrixInv = viewProjMatrix.invert();
    CB.lightMatrix = m_LightViewProjMatrix;
    CB.lightDirection = m_LightDirection;
    CB.lightColor = VXGI::Vector4f(1.f);
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
    NVRHI::BindTextureAndSampler(state.PS, 5, m_ShadowMap, m_pComparisonSamplerState);
#if !USE_GL4
    NVRHI::BindSampler(state.PS, 0, m_pComparisonSamplerState);
#endif

    NVRHI::DrawArguments args;
    args.vertexCount = 4;
    m_RendererInterface->draw(state, &args, 1);
}

VXGI::Frustum SceneRenderer::GetLightFrustum()
{
    return VXGI::Frustum(m_LightViewProjMatrix);
}
