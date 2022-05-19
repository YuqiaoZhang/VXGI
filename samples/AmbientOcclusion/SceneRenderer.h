/*
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "GFSDK_VXGI.h"
#include "Scene.h"
#include <functional>

#pragma warning(disable : 4324)

__declspec(align(16)) struct GlobalConstants
{
    VXGI::float4x4 viewProjMatrix;
    uint32_t visualizeAO;
};

struct MeshMaterialInfo : public VXGI::MaterialInfo
{
    NVRHI::TextureHandle    diffuseTexture;
    NVRHI::TextureHandle    normalsTexture;
    NVRHI::TextureHandle    opacityTexture;
    VXGI::float3          diffuseColor;

    MeshMaterialInfo() :
        diffuseTexture(NULL),
        normalsTexture(NULL),
        opacityTexture(NULL),
        diffuseColor(0.f)
    {}
};

typedef std::function<void(const MeshMaterialInfo&)> MaterialCallback;

class SceneRenderer
{
private:

    Scene*                  m_pScene;
    NVRHI::IRendererInterface* m_RendererInterface;

    NVRHI::InputLayoutRef    m_pInputLayout;
    NVRHI::ShaderRef         m_pDefaultVS;
    NVRHI::ShaderRef         m_pVoxelizationVS;
    NVRHI::ShaderRef         m_pFullScreenQuadVS;
    NVRHI::ShaderRef         m_pAttributesPS;
    NVRHI::ShaderRef         m_pBlitPS;
    NVRHI::ShaderRef         m_pCompositingPS;

    NVRHI::ConstantBufferRef m_pGlobalCBuffer;

    NVRHI::SamplerRef        m_pDefaultSamplerState;

    UINT                    m_Width;
    UINT                    m_Height;
    UINT                    m_SampleCount;

    NVRHI::TextureRef       m_TargetAlbedo;
    NVRHI::TextureRef       m_TargetNormal;
    NVRHI::TextureRef       m_TargetDepth;
    NVRHI::TextureRef       m_TargetNormalPrev;
    NVRHI::TextureRef       m_TargetDepthPrev;
    
    NVRHI::TextureRef       m_NullTexture;
    NVRHI::TextureRef       m_WhiteDummy;

    VXGI::IUserDefinedShaderSet* m_pVoxelizationGS;
    VXGI::IUserDefinedShaderSet* m_pVoxelizationPS;

public:
    SceneRenderer(NVRHI::IRendererInterface* pRenderer);

    HRESULT LoadMesh(const char* strFileName);

    HRESULT AllocateResources(VXGI::IGlobalIllumination* pGI, VXGI::IShaderCompiler* pCompiler);
    void AllocateViewDependentResources(UINT width, UINT height, UINT sampleCount = 1);
    void ReleaseResources(VXGI::IGlobalIllumination* pGI);
    void ReleaseViewDependentResources();

    void RenderToGBuffer(const VXGI::float4x4& viewProjMatrix);

    void GetMaterialInfo(UINT meshID, OUT MeshMaterialInfo& materialInfo);

    void FillTracingInputBuffers(VXGI::IBasicViewTracer::InputBuffers& inputBuffers);
    NVRHI::TextureHandle GetAlbedoBufferHandle();

    void Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest);
    void ComposeAO(NVRHI::TextureHandle pVXAO, NVRHI::TextureHandle pSSAO, NVRHI::TextureHandle pDest, bool visualizeAO);

    void RenderSceneCommon(
        NVRHI::DrawCallState& state,
        VXGI::IGlobalIllumination* pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        const VXGI::float4x4& viewProjMatrix,
        MaterialCallback* onChangeMaterial,
        bool voxelization);
};