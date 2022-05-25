#define REGISTER(type, slot) register(type##slot)
#define NV_SHADER_EXTN_SLOT u0
#define VXGI_VOXELIZE_CB_SLOT 1
#define VXGI_SCISSOR_REGIONS_SRV_SLOT 2
#define VXGI_ALLOCATION_MAP_UAV_SLOT 1
#define VXGI_VOXELTEX_0_UAV_SLOT 2
#define VXGI_VOXELTEX_1_UAV_SLOT 3
#define VXGI_VOXELTEX_2_UAV_SLOT 4
#define VXGI_VOXELTEX_3_UAV_SLOT 5
#define VXGI_VOXELTEX_4_UAV_SLOT 6
#define VXGI_VOXELTEX_5_UAV_SLOT 7
#define VXGI_SCISSOR_STATS_UAV_SLOT 8
#define VXGI_COVERAGE_MASKS_SRV_SLOT 3
#define VXGI_VOXELIZE_MATERIAL_CB_SLOT 2
#define VXGI_IRRADIANCE_MAP_SRV_SLOT 4
#define VXGI_IRRADIANCE_MAP_SAMPLER_SLOT 2
#define VXGI_INVALIDATE_BITMAP_SRV_SLOT 5

#pragma pack_matrix(row_major)
static const float VxgiPI = 3.14159265;

float2 float2scalar(float x) { return float2(x, x); }
float3 float3scalar(float x) { return float3(x, x, x); }
float4 float4scalar(float x) { return float4(x, x, x, x); }
int2 int2scalar(int x) { return int2(x, x); }
int3 int3scalar(int x) { return int3(x, x, x); }
int4 int4scalar(int x) { return int4(x, x, x, x); }
uint2 uint2scalar(uint x) { return uint2(x, x); }
uint3 uint3scalar(uint x) { return uint3(x, x, x); }
uint4 uint4scalar(uint x) { return uint4(x, x, x, x); }

struct VxgiFullScreenQuadOutput
{
	float2 uv : TEXCOORD;
	float4 posProj : RAY;
	float instanceID : INSTANCEID;
};

float3 VxgiProjToRay(float4 posProj, float3 cameraPos, float4x4 viewProjMatrixInv)
{
	float4 farPoint = mul(posProj, viewProjMatrixInv);
	farPoint.xyz /= farPoint.w;

	return normalize(farPoint.xyz - cameraPos.xyz);
}

struct VxgiBox4i
{
	int4 lower;
	int4 upper;
};

struct VxgiBox4f
{
	float4 lower;
	float4 upper;
};

bool VxgiPartOfRegion(int3 coords, VxgiBox4i region)
{
	return coords.x <= region.upper.x && coords.y <= region.upper.y && coords.z <= region.upper.z &&
		coords.x >= region.lower.x && coords.y >= region.lower.y && coords.z >= region.lower.z;
}

bool VxgiPartOfRegionWithBorder(int3 coords, VxgiBox4i region, int border)
{
	return coords.x <= region.upper.x + border && coords.y <= region.upper.y + border && coords.z <= region.upper.z + border &&
		coords.x >= region.lower.x - border && coords.y >= region.lower.y - border && coords.z >= region.lower.z - border;
}

bool VxgiPartOfRegionf(float3 coords, float3 lower, float3 upper)
{
	return coords.x <= upper.x && coords.y <= upper.y && coords.z <= upper.z &&
		coords.x >= lower.x && coords.y >= lower.y && coords.z >= lower.z;
}

float VxgiToSRGB(float x)
{
	x = saturate(x);
	return x <= 0.0031308f ? x * 12.92f : saturate(1.055f * pow(x, 0.4166667f) - 0.055f);
}

float VxgiFromSRGB(float x)
{
	return x <= 0.04045f ? saturate(x * 0.0773994f) : pow(saturate(x * 0.9478672f + 0.0521327f), 2.4f);
}

uint VxgiPackEmittance(float4 v)
{
	v.rgb = float3(VxgiToSRGB(v.r), VxgiToSRGB(v.g), VxgiToSRGB(v.b));
	uint result = uint(255.0f * v.x) | (uint(255.0f * v.y) << 8) | (uint(255.0f * v.z) << 16);
	return result;
}

float4 VxgiUnpackEmittance(uint n)
{
	float3 result;
	result.x = ((n >> 0) & 0xFF) / 255.0f;
	result.y = ((n >> 8) & 0xFF) / 255.0f;
	result.z = ((n >> 16) & 0xFF) / 255.0f;
	result = float3(VxgiFromSRGB(result.r), VxgiFromSRGB(result.g), VxgiFromSRGB(result.b));
	return float4(result, 0);
}

uint3 VxgiPackEmittanceForAtomic(float3 v)
{
	float fixedPointScale = 1 << 20;
	return uint3(v.rgb * fixedPointScale);
}

uint VxgiPackOpacity(float3 opacity)
{
	return uint(1023 * opacity.x) | (uint(1023 * opacity.y) << 10) | (uint(1023 * opacity.z) << 20);
}

float4 VxgiUnpackOpacity(uint opacity)
{
	float4 fOpacity;
	fOpacity.x = float((opacity) & 0x3ff) / 1023.0;
	fOpacity.y = float((opacity >> 10) & 0x3ff) / 1023.0;
	fOpacity.z = float((opacity >> 20) & 0x3ff) / 1023.0;
	fOpacity.w = 0;
	return fOpacity;
}

float VxgiGetNormalProjection(float3 normal, uint direction)
{
	float normalProjection = 0;

	switch (direction)
	{
	case 0:
		normalProjection = saturate(normal.x);
		break;
	case 3:
		normalProjection = saturate(-normal.x);
		break;
	case 1:
		normalProjection = saturate(normal.y);
		break;
	case 4:
		normalProjection = saturate(-normal.y);
		break;
	case 2:
		normalProjection = saturate(normal.z);
		break;
	case 5:
		normalProjection = saturate(-normal.z);
		break;
	}

	return normalProjection;
}

float VxgiAverage4(float a, float b, float c, float d)
{
	return (a + b + c + d) / 4;
}

float VxgiMultiplyComplements(float a, float b)
{
	return 1 - (1 - a) * (1 - b);
}

bool VxgiIsOdd(int x)
{
	return (x & 1) != 0;
}

struct VxgiVoxelizationConstants
{
	float4 GridCenter;
	float4 GridCenterPrevious;
	int4 ToroidalOffset;
	VxgiBox4f ScissorRegionsClipSpace[5];
	int4 TextureToAmapTranslation[5];
	float IrradianceMapSize;
	uint ClipLevelSize;
	uint PackingStride;
	uint AllocationMapSize;
	uint ClipLevelMask;
	uint MaxClipLevel;
	int FirstLevelToDiscard;
	float DiscardLower;
	float DiscardUpper;
	float DiscardClipSpace;
	uint UseCullFunction;
	float EmittanceStorageScale;
	uint UseIrradianceMap;
	uint Use6DOpacity;
	uint UseFP32Emittance;
	uint PersistentVoxelData;
	uint UseInvalidateBitmap;
};

struct VxgiVoxelizationMaterialConstants
{
	float4 ResolutionFactors[5];
	float NoiseScale;
	float NoiseBias;
	int TwoSided;
	int ProportionalEmittance;
	int DepthSamples;
	int FrontCCW;
	int OmnidirectionalLight;
};

cbuffer VoxelizationCB : REGISTER(b, VXGI_VOXELIZE_CB_SLOT)
{
	VxgiVoxelizationConstants g_VxgiVoxelizationCB;
};

cbuffer VoxelizationMaterialCB : REGISTER(b, VXGI_VOXELIZE_MATERIAL_CB_SLOT)
{
	VxgiVoxelizationMaterialConstants g_VxgiVoxelizationMaterialCB;
};

struct VxgiVoxelizationGSOutputData
{
	nointerpolation uint gl_ViewportIndex : SV_ViewportArrayIndex;
};

struct VxgiVoxelizationPSInputData
{
	VxgiVoxelizationGSOutputData GSData;
	float4 gl_FragCoord : SV_Position;
	uint gl_SampleMaskIn : SV_Coverage;
	bool gl_FrontFacing : SV_IsFrontFace;
};

struct VxgiScissorStats
{
	uint pixelsRasterized;
	uint pixelsKilledByZViewport;
	uint pixelsKilledByScissorBox;
	uint pixelsKilledByFinerLOD;
	uint voxelsWithNoCoverage;
	uint voxelsKilledByScissorBox;
	uint voxelsKilledByAllocationMap;
};

static const float2 g_VxgiSamplePositions[] = {
	float2(0.0625, -0.1875), float2(-0.0625, 0.1875), float2(0.3125, 0.0625), float2(-0.1875, -0.3125),
	float2(-0.3125, 0.3125), float2(-0.4375, 0.0625), float2(0.1875, 0.4375), float2(0.4375, -0.4375),
	float2(0, 0) };

static const float g_VxgiSampleZOffsets[] = {
	0.9, 0.4, 0.5, 0.6, 0.1, 0.8, 0.7, 0.3, 0.2 };

bool VxgiIsVoxelInDiscardArea(float3 voxelCoordinates, int clipLevel)
{

	if (clipLevel >= g_VxgiVoxelizationCB.FirstLevelToDiscard && all((voxelCoordinates >= float3scalar(g_VxgiVoxelizationCB.DiscardLower))) && all((voxelCoordinates <= float3scalar(g_VxgiVoxelizationCB.DiscardUpper))))
	{
		;
		return true;
	}
	return false;
}

static const int VxgiIsEmissiveVoxelizationPass = 1;

RWTexture3D<uint> u_AllocationMap : REGISTER(u, VXGI_ALLOCATION_MAP_UAV_SLOT);

RWTexture3D<uint> u_EmittanceEvenR : REGISTER(u, VXGI_VOXELTEX_0_UAV_SLOT);
RWTexture3D<uint> u_EmittanceEvenG : REGISTER(u, VXGI_VOXELTEX_1_UAV_SLOT);
RWTexture3D<uint> u_EmittanceEvenB : REGISTER(u, VXGI_VOXELTEX_2_UAV_SLOT);
RWTexture3D<uint> u_EmittanceOddR : REGISTER(u, VXGI_VOXELTEX_3_UAV_SLOT);
RWTexture3D<uint> u_EmittanceOddG : REGISTER(u, VXGI_VOXELTEX_4_UAV_SLOT);
RWTexture3D<uint> u_EmittanceOddB : REGISTER(u, VXGI_VOXELTEX_5_UAV_SLOT);

RWStructuredBuffer<VxgiScissorStats> u_ScissorStats : REGISTER(u, VXGI_SCISSOR_STATS_UAV_SLOT);

SamplerState s_IrradianceMapSampler : REGISTER(s, VXGI_IRRADIANCE_MAP_SAMPLER_SLOT);
Texture3D<float4> t_IrradianceMap : REGISTER(t, VXGI_IRRADIANCE_MAP_SRV_SLOT);

float3 VxgiGetLayeredEmittance(float inputZ, uint coverage)
{
	float3 layeredEmittance = float3(0, 0, 0);

	float2 dZ = float2(ddx(inputZ), ddy(inputZ));
	float fracZ = frac(inputZ);

	[unroll] for (int nSample = 0; nSample < 8; nSample++)
	{
		if ((coverage & (1 << nSample)) != 0)
		{
			float relativeZ = fracZ + dot(dZ, g_VxgiSamplePositions[nSample]);
			const float delta = 1.0f / 8;

			if (g_VxgiVoxelizationMaterialCB.ProportionalEmittance != 0)
			{
				layeredEmittance.x += delta * saturate(1 - abs(relativeZ + 0.5));
				layeredEmittance.y += delta * saturate(1 - abs(relativeZ - 0.5));
				layeredEmittance.z += delta * saturate(1 - abs(relativeZ - 1.5));
			}
			else
			{
				if (relativeZ < 0)
					layeredEmittance.x += delta;
				else if (relativeZ >= 1)
					layeredEmittance.z += delta;
				else
					layeredEmittance.y += delta;
			}
		}
	}

	return layeredEmittance;
}

void VxgiWriteEmittance(int3 coordinates, int clipLevel, float perVoxelScale, float3 emittance, float3 normal, inout int prevPageCoordinatesHash, inout uint page)
{
	emittance = max(emittance * perVoxelScale, 0);

	if (!any((emittance != float3(0, 0, 0))))
	{
		;
		return;
	}

	if (((coordinates.x | coordinates.y | coordinates.z) & ~(g_VxgiVoxelizationCB.ClipLevelSize - 1)) != 0)
	{
		return;
	}

	{
		int4 translation = g_VxgiVoxelizationCB.TextureToAmapTranslation[clipLevel];
		int3 pageCoordinates = int3(((coordinates >> translation.w) + translation.xyz) & (g_VxgiVoxelizationCB.AllocationMapSize - 1));
		int pageCoordinatesHash = pageCoordinates.x + pageCoordinates.y + pageCoordinates.z;

		if (pageCoordinatesHash != prevPageCoordinatesHash)
			page = u_AllocationMap[pageCoordinates].x;

		prevPageCoordinatesHash = pageCoordinatesHash;

		if ((page & 0x08) == 0)
			return;

		if ((page & 0x02) == 0)
		{
			page |= 0x02;
			(u_AllocationMap[pageCoordinates] = page);
		}
	}

	int3 voxelCoords = int3(((coordinates)+(g_VxgiVoxelizationCB.ToroidalOffset.xyz >> clipLevel)) & ((g_VxgiVoxelizationCB.ClipLevelSize) - 1));
	int3 address = voxelCoords + int3(1, 0, int(g_VxgiVoxelizationCB.PackingStride) * (clipLevel >> 1) + 1);

	emittance.rgb *= g_VxgiVoxelizationCB.EmittanceStorageScale;

	if (bool(g_VxgiVoxelizationCB.UseFP32Emittance))
	{
		bool omni = bool(g_VxgiVoxelizationMaterialCB.OmnidirectionalLight) || all((normal == float3(0, 0, 0)));

		[unroll] for (uint direction = 0; direction < 6; ++direction)
		{
			float multiplier = omni ? rcp(6.0) : VxgiGetNormalProjection(normal, direction);

			if (multiplier > 0)
			{
				uint3 packedEmittance = VxgiPackEmittanceForAtomic(emittance * multiplier);

				if (VxgiIsOdd(clipLevel))
				{
					InterlockedAdd(u_EmittanceOddR[address], packedEmittance.r);
					InterlockedAdd(u_EmittanceOddG[address], packedEmittance.g);
					InterlockedAdd(u_EmittanceOddB[address], packedEmittance.b);
				}
				else
				{
					InterlockedAdd(u_EmittanceEvenR[address], packedEmittance.r);
					InterlockedAdd(u_EmittanceEvenG[address], packedEmittance.g);
					InterlockedAdd(u_EmittanceEvenB[address], packedEmittance.b);
				}
			}

			address.x += int(g_VxgiVoxelizationCB.PackingStride);
		}
	}
	else
	{
		float fixedPointScale = 1 << 20;

		uint data;
		if (emittance.r != 0)
		{
			data = uint(emittance.r * fixedPointScale);
			if (VxgiIsOdd(clipLevel))
				InterlockedAdd(u_EmittanceOddR[address], data);
			else
				InterlockedAdd(u_EmittanceEvenR[address], data);
		}
		address.x += int(g_VxgiVoxelizationCB.PackingStride);

		if (emittance.g != 0)
		{
			data = uint(emittance.g * fixedPointScale);
			if (VxgiIsOdd(clipLevel))
				InterlockedAdd(u_EmittanceOddR[address], data);
			else
				InterlockedAdd(u_EmittanceEvenR[address], data);
		}
		address.x += int(g_VxgiVoxelizationCB.PackingStride);

		if (emittance.b != 0)
		{
			data = uint(emittance.b * fixedPointScale);
			if (VxgiIsOdd(clipLevel))
				InterlockedAdd(u_EmittanceOddR[address], data);
			else
				InterlockedAdd(u_EmittanceEvenR[address], data);
		}
		address.x += int(g_VxgiVoxelizationCB.PackingStride);

		if (bool(g_VxgiVoxelizationMaterialCB.OmnidirectionalLight))
		{
			if (VxgiIsOdd(clipLevel))
				InterlockedOr(u_EmittanceOddR[address], 1);
			else
				InterlockedOr(u_EmittanceEvenR[address], 1);
		}
		else
		{
			if (normal.x != 0)
			{
				data = uint(int(normal.x * fixedPointScale * perVoxelScale)) & ~1;
				if (VxgiIsOdd(clipLevel))
					InterlockedAdd(u_EmittanceOddR[address], data);
				else
					InterlockedAdd(u_EmittanceEvenR[address], data);
			}
			address.x += int(g_VxgiVoxelizationCB.PackingStride);

			if (normal.y != 0)
			{
				data = uint(int(normal.y * fixedPointScale * perVoxelScale));
				if (VxgiIsOdd(clipLevel))
					InterlockedAdd(u_EmittanceOddR[address], data);
				else
					InterlockedAdd(u_EmittanceEvenR[address], data);
			}
			address.x += int(g_VxgiVoxelizationCB.PackingStride);

			if (normal.z != 0)
			{
				data = uint(int(normal.z * fixedPointScale * perVoxelScale));
				if (VxgiIsOdd(clipLevel))
					InterlockedAdd(u_EmittanceOddR[address], data);
				else
					InterlockedAdd(u_EmittanceEvenR[address], data);
			}
		}
	}
}

void VoxelizeEmittancePS(VxgiVoxelizationPSInputData IN, float3 emissiveColor)
{
	;

	bool frontFacing = IN.gl_FrontFacing;
	if (g_VxgiVoxelizationMaterialCB.FrontCCW != 0)
		frontFacing = !frontFacing;

	float fVP = float(IN.GSData.gl_ViewportIndex) * 0.334;
	int projectionIndex = int(floor(frac(fVP) * 3));
	int clipLevel = int(floor(fVP));
	float inputZ = IN.gl_FragCoord.z;

	float unscaledZ = inputZ;

	inputZ *= (1 << (g_VxgiVoxelizationCB.MaxClipLevel - clipLevel));
	inputZ = inputZ * 0.5 + 0.5;
	inputZ *= g_VxgiVoxelizationCB.ClipLevelSize;

	const float DesnapQuantum = 0.0078125;
	if (abs(inputZ - round(inputZ)) < DesnapQuantum)
		inputZ += 2 * DesnapQuantum;

	float resolutionFactor = g_VxgiVoxelizationMaterialCB.ResolutionFactors[clipLevel].x;
	float rResolutionFactor = g_VxgiVoxelizationMaterialCB.ResolutionFactors[clipLevel].y;

	float3 texelSpace = float3(IN.gl_FragCoord.xy, inputZ);
	texelSpace.xy *= rResolutionFactor;

	texelSpace.y = g_VxgiVoxelizationCB.ClipLevelSize - texelSpace.y;

	if (VxgiIsVoxelInDiscardArea(texelSpace, clipLevel))
	{
		discard;
		return;
	}

	int3 offsetDir;
	bool kill = false;

	if (projectionIndex == 0)
	{
		kill = unscaledZ < g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].lower.z || unscaledZ > g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].upper.z;

		offsetDir = int3(0, 0, 1);
	}
	else if (projectionIndex == 1)
	{
		kill = unscaledZ < g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].lower.y || unscaledZ > g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].upper.y;

		texelSpace.zxy = texelSpace.xyz;
		offsetDir = int3(0, 1, 0);
	}
	else
	{
		kill = unscaledZ < g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].lower.x || unscaledZ > g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].upper.x;

		texelSpace.yzx = texelSpace.xyz;
		offsetDir = int3(1, 0, 0);
	}

	if (kill)
	{
		discard;
		return;
	}

	int3 voxelCoordinates = int3(floor(texelSpace));

	uint coverage = uint(IN.gl_SampleMaskIn);

	float3 layeredEmittance = VxgiGetLayeredEmittance(inputZ, coverage);

	float3 normal = normalize(cross(ddx(texelSpace), ddy(texelSpace)));
	if (!frontFacing)
		normal = -normal;

	float areaScale = rcp(max(abs(normal.x), max(abs(normal.y), abs(normal.z))));

	areaScale *= rResolutionFactor * rResolutionFactor;

	int pageCoordinatesHash = -1;
	uint page = 0;

	voxelCoordinates -= offsetDir;

	[loop] for (uint voxel = 0; voxel < 3; voxel++)
	{
		VxgiWriteEmittance(voxelCoordinates, clipLevel, layeredEmittance.x * areaScale, emissiveColor, normal, pageCoordinatesHash, page);
		voxelCoordinates += offsetDir;
		layeredEmittance.xy = layeredEmittance.yz;
	}

	discard;
}

float4 VxgiNormalizeIrradiance(float4 irradiance)
{
	if (irradiance.a <= 0)
		return float4scalar(0);

	return float4(irradiance.rgb / irradiance.a, 1);
}

float3 VxgiGetIndirectIrradiance(float3 worldPos, float3 normal)
{
	if (bool(g_VxgiVoxelizationCB.UseIrradianceMap))
	{
		float3 relativePos = (worldPos - g_VxgiVoxelizationCB.GridCenterPrevious.xyz) * g_VxgiVoxelizationCB.GridCenterPrevious.w;

		float3 validities = saturate(g_VxgiVoxelizationCB.IrradianceMapSize * (float3scalar(1.0) - 2 * abs(relativePos.xyz)));
		float validity = validities.x * validities.y * validities.z;

		if (validity == 0)
			return float3scalar(0.0);

		relativePos += 0.5;

		const float zstep = rcp(6.0);
		relativePos.z *= zstep;

		float4 irradianceX = t_IrradianceMap.SampleLevel(s_IrradianceMapSampler, relativePos + float3(0, 0, zstep * (normal.x > 0 ? 0 : 3)), 0);
		float4 irradianceY = t_IrradianceMap.SampleLevel(s_IrradianceMapSampler, relativePos + float3(0, 0, zstep * (normal.y > 0 ? 1 : 4)), 0);
		float4 irradianceZ = t_IrradianceMap.SampleLevel(s_IrradianceMapSampler, relativePos + float3(0, 0, zstep * (normal.z > 0 ? 2 : 5)), 0);

		return (VxgiNormalizeIrradiance(irradianceX).rgb * abs(normal.x) + VxgiNormalizeIrradiance(irradianceY).rgb * abs(normal.y) + VxgiNormalizeIrradiance(irradianceZ).rgb * abs(normal.z)) * validity;
	}

	return float3scalar(0);
}

void VxgiStoreVoxelizationData(VxgiVoxelizationPSInputData inputData, float3 emissiveColor)
{
	VoxelizeEmittancePS(inputData, emissiveColor);
}

//////////////////APP CODE BOUNDARY/////////////
/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float3 positionWS : WSPOSITION;
	VxgiVoxelizationPSInputData vxgiData;
};

cbuffer GlobalConstants : register(b0)
{
	float4x4 g_ViewProjMatrix;
	float4x4 g_ViewProjMatrixInv;
	float4x4 g_LightViewProjMatrix;
	float4 g_LightDirection;
	float4 g_DiffuseColor;
	float4 g_LightColor;
	float g_rShadowMapSize;
	uint g_EnableIndirectDiffuse;
	uint g_EnableIndirectSpecular;
};

Texture2D<float4> t_DiffuseColor : register(t0);
Texture2D t_ShadowMap : register(t1);
SamplerState g_SamplerLinearWrap : register(s0);
SamplerComparisonState g_SamplerComparison : register(s1);

static const float PI = 3.14159265;

float GetShadowFast(float3 fragmentPos)
{
	float4 clipPos = mul(float4(fragmentPos, 1.0f), g_LightViewProjMatrix);

	// Early out
	if (abs(clipPos.x) > clipPos.w || abs(clipPos.y) > clipPos.w || clipPos.z <= 0)
	{
		return 0;
	}

	clipPos.xyz /= clipPos.w;
	clipPos.x = clipPos.x * 0.5f + 0.5f;
	clipPos.y = 0.5f - clipPos.y * 0.5f;

	return t_ShadowMap.SampleCmpLevelZero(g_SamplerComparison, clipPos.xy, clipPos.z);
}

void main(PSInput IN)
{
	if (VxgiIsEmissiveVoxelizationPass)
	{
		float3 worldPos = IN.positionWS.xyz;
		float3 normal = normalize(IN.normal.xyz);

		float3 albedo = g_DiffuseColor.rgb;

		if (g_DiffuseColor.a > 0)
			albedo = t_DiffuseColor.Sample(g_SamplerLinearWrap, IN.texCoord.xy).rgb;

		float NdotL = saturate(-dot(normal, g_LightDirection.xyz));

		float3 radiosity = 0;

		if (NdotL > 0)
		{
			float shadow = GetShadowFast(worldPos);

			radiosity += albedo.rgb * g_LightColor.rgb * (NdotL * shadow);
		}

		radiosity += albedo.rgb * VxgiGetIndirectIrradiance(worldPos, normal) / PI;

		VxgiStoreVoxelizationData(IN.vxgiData, radiosity);
	}
	else
	{
		VxgiStoreVoxelizationData(IN.vxgiData, 0);
	}
};
//////////////////APP CODE BOUNDARY/////////////
