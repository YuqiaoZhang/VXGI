bool CullTriangle(float3 v1, float3 v2, float3 v3, float3 normal)
{
	return false;
}
#define REGISTER(type, slot) register(type##slot)
#define NV_SHADER_EXTN_SLOT u0
#define VXGI_VOXELIZE_CB_SLOT 0
#define VXGI_SCISSOR_REGIONS_SRV_SLOT 0
#define VXGI_ALLOCATION_MAP_UAV_SLOT 1
#define VXGI_VOXELTEX_0_UAV_SLOT 2
#define VXGI_VOXELTEX_1_UAV_SLOT 3
#define VXGI_VOXELTEX_2_UAV_SLOT 4
#define VXGI_VOXELTEX_3_UAV_SLOT 5
#define VXGI_VOXELTEX_4_UAV_SLOT 6
#define VXGI_VOXELTEX_5_UAV_SLOT 7
#define VXGI_SCISSOR_STATS_UAV_SLOT 8
#define VXGI_COVERAGE_MASKS_SRV_SLOT 1
#define VXGI_VOXELIZE_MATERIAL_CB_SLOT 1
#define VXGI_IRRADIANCE_MAP_SRV_SLOT 2
#define VXGI_IRRADIANCE_MAP_SAMPLER_SLOT 0
#define VXGI_INVALIDATE_BITMAP_SRV_SLOT 3
#define SV_POSITION_ATTR_IN(N) IN[N].UserData.VXGI_D3DNoName0
#define SV_POSITION_ATTR_OUT OUT.UserData.VXGI_D3DNoName0
#define EXTRAPOLATE_USER_ATTRS                                                                                                                                            \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName1[0], TMP[1].UserData.VXGI_D3DNoName1[0], TMP[2].UserData.VXGI_D3DNoName1[0], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName1[1], TMP[1].UserData.VXGI_D3DNoName1[1], TMP[2].UserData.VXGI_D3DNoName1[1], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName2[0], TMP[1].UserData.VXGI_D3DNoName2[0], TMP[2].UserData.VXGI_D3DNoName2[0], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName2[1], TMP[1].UserData.VXGI_D3DNoName2[1], TMP[2].UserData.VXGI_D3DNoName2[1], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName2[2], TMP[1].UserData.VXGI_D3DNoName2[2], TMP[2].UserData.VXGI_D3DNoName2[2], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName3[0], TMP[1].UserData.VXGI_D3DNoName3[0], TMP[2].UserData.VXGI_D3DNoName3[0], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName3[1], TMP[1].UserData.VXGI_D3DNoName3[1], TMP[2].UserData.VXGI_D3DNoName3[1], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName3[2], TMP[1].UserData.VXGI_D3DNoName3[2], TMP[2].UserData.VXGI_D3DNoName3[2], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName4[0], TMP[1].UserData.VXGI_D3DNoName4[0], TMP[2].UserData.VXGI_D3DNoName4[0], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName4[1], TMP[1].UserData.VXGI_D3DNoName4[1], TMP[2].UserData.VXGI_D3DNoName4[1], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName4[2], TMP[1].UserData.VXGI_D3DNoName4[2], TMP[2].UserData.VXGI_D3DNoName4[2], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName5[0], TMP[1].UserData.VXGI_D3DNoName5[0], TMP[2].UserData.VXGI_D3DNoName5[0], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName5[1], TMP[1].UserData.VXGI_D3DNoName5[1], TMP[2].UserData.VXGI_D3DNoName5[1], factors, newPos0, newPos1, newPos2); \
    ExtrapolateAttribute(TMP[0].UserData.VXGI_D3DNoName5[2], TMP[1].UserData.VXGI_D3DNoName5[2], TMP[2].UserData.VXGI_D3DNoName5[2], factors, newPos0, newPos1, newPos2);
struct UserAttributesWithPos
{
	float4 VXGI_D3DNoName0 : SV_POSITION;
	float2 VXGI_D3DNoName1 : TEXCOORD0;
	float3 VXGI_D3DNoName2 : NORMAL0;
	float3 VXGI_D3DNoName3 : TANGENT0;
	float3 VXGI_D3DNoName4 : BINORMAL0;
	float3 VXGI_D3DNoName5 : WSPOSITION0;
};

//////////////////APP CODE BOUNDARY/////////////
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

	nointerpolation float3 Edge1 : EDGE1;
	nointerpolation float3 Edge2 : EDGE2;
	nointerpolation float3 Edge3 : EDGE3;
	nointerpolation float4 ProjectedBoundingBox : PROJECTED_BBOX;
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

Texture3D<uint> t_InvalidateBitmap : REGISTER(t, VXGI_INVALIDATE_BITMAP_SRV_SLOT);

struct Voxelize_GSIn
{
	UserAttributesWithPos UserData;
};

struct Voxelize_GSOut
{
	UserAttributesWithPos UserData;
	VxgiVoxelizationGSOutputData VXGIData;
};

bool IsTriangleInReadOnlyPage(float3 lower, float3 upper)
{
	float3 lowerPage = (lower * 0.5 + 0.5) * float(g_VxgiVoxelizationCB.AllocationMapSize);
	float3 upperPage = (upper * 0.5 + 0.5) * float(g_VxgiVoxelizationCB.AllocationMapSize);

	uint mask = ~0u;
	int3 page = int3(floor(lowerPage));

	if (all((floor(lowerPage) == floor(upperPage))))
	{
		mask = t_InvalidateBitmap[int3(page.xy, page.z >> 5)].x;
	}

	return ((mask & (1 << (page.z & 31))) == 0);
}

bool TestTriangleDiscard(float3 lower, float3 upper, int level)
{

	float finerLevelSize = g_VxgiVoxelizationCB.DiscardClipSpace * float(1 << level);

	VxgiBox4f levelScissor = g_VxgiVoxelizationCB.ScissorRegionsClipSpace[level];

	if (any((lower > levelScissor.upper.xyz)) || any((upper < levelScissor.lower.xyz)))
		return true;

	if ((level >= g_VxgiVoxelizationCB.FirstLevelToDiscard) && all((lower.xyz >= float3scalar(-finerLevelSize))) && all((upper.xyz <= float3scalar(finerLevelSize))))
		return true;

	return false;
}

float3 GetEdgeEquation(float2 v1, float2 v2)
{
	float a = v1.y - v2.y;
	float b = v2.x - v1.x;
	float c = -0.5 * (a * (v1.x + v2.x) + b * (v1.y + v2.y));
	return float3(a, b, c);
}

float2 Intersect2D(float2 p00, float2 p01, float2 p10, float2 p11)
{
	float2 _point;

	float d = rcp((p00.x - p01.x) * (p10.y - p11.y) - (p00.y - p01.y) * (p10.x - p11.x));

	_point.x = ((p10.x - p11.x) * (p00.x * p01.y - p00.y * p01.x) - (p00.x - p01.x) * (p10.x * p11.y - p10.y * p11.x)) * d;
	_point.y = ((p10.y - p11.y) * (p00.x * p01.y - p00.y * p01.x) - (p00.y - p01.y) * (p10.x * p11.y - p10.y * p11.x)) * d;

	return _point;
}

void RasterizeConservative2D(inout float2 p0, inout float2 p1, inout float2 p2, float2 rOffset)
{
	float2 edge0 = p0 - p2;
	float2 edge1 = p1 - p0;
	float2 edge2 = p2 - p1;

	float frontFace = edge0.x * edge1.y - edge0.y * edge1.x;

	float2 normal0 = float2(edge0.y * frontFace, -edge0.x * frontFace);
	float2 normal1 = float2(edge1.y * frontFace, -edge1.x * frontFace);
	float2 normal2 = float2(edge2.y * frontFace, -edge2.x * frontFace);

	float2 offsetDir0 = float2(sign(normal0.x) * rOffset.x, sign(normal0.y) * rOffset.y);
	float2 offsetDir1 = float2(sign(normal1.x) * rOffset.x, sign(normal1.y) * rOffset.y);
	float2 offsetDir2 = float2(sign(normal2.x) * rOffset.x, sign(normal2.y) * rOffset.y);

	float2 _p0 = Intersect2D(p0 + offsetDir0, p2 + offsetDir0, p0 + offsetDir1, p1 + offsetDir1);
	float2 _p1 = Intersect2D(p1 + offsetDir1, p0 + offsetDir1, p1 + offsetDir2, p2 + offsetDir2);
	float2 _p2 = Intersect2D(p2 + offsetDir2, p1 + offsetDir2, p2 + offsetDir0, p0 + offsetDir0);

	p0 = _p0;
	p1 = _p1;
	p2 = _p2;
}

void ExtrapolateAttribute(inout float z1, inout float z2, inout float z3, float3x3 factors, float2 newp1, float2 newp2, float2 newp3)
{
	float3 equation = mul(transpose(factors), float3(z1, z2, z3));
	z1 = dot(equation.xy, newp1) + equation.z;
	z2 = dot(equation.xy, newp2) + equation.z;
	z3 = dot(equation.xy, newp3) + equation.z;
}

[instance(5)]
[maxvertexcount(3)]
void main(triangle Voxelize_GSIn IN[3], int gfsdk_InvocationID : SV_GSInstanceID, inout TriangleStream<Voxelize_GSOut> outStream)
{
	Voxelize_GSOut OUT;

	int clipLevel = gfsdk_InvocationID;
	if ((g_VxgiVoxelizationCB.ClipLevelMask & (1 << (clipLevel * 3))) != 0)

	{
		float3 pos0 = SV_POSITION_ATTR_IN(0).xyz;
		float3 pos1 = SV_POSITION_ATTR_IN(1).xyz;
		float3 pos2 = SV_POSITION_ATTR_IN(2).xyz;

		float3 edge01 = pos1.xyz - pos0.xyz;
		float3 edge02 = pos2.xyz - pos0.xyz;
		float3 normal = normalize(cross(edge01, edge02));

		bool culled = false;

		if ((g_VxgiVoxelizationCB.UseCullFunction != 0) && CullTriangle(pos0, pos1, pos2, normal))
			culled = true;

		float3 lower = min(min(pos0, pos1), pos2);
		float3 upper = max(max(pos0, pos1), pos2);

		if (bool(g_VxgiVoxelizationCB.UseInvalidateBitmap))
		{
			if (IsTriangleInReadOnlyPage(lower, upper))
				culled = true;
		}

		if (TestTriangleDiscard(lower, upper, clipLevel))
			culled = true;

		if (culled)
			return;

		float3 absNormal = abs(normal);

		int projectionIndex = 0;

		if (absNormal.z > absNormal.y && absNormal.z > absNormal.x)
		{

			pos0 = pos0.xyz;
			pos1 = pos1.xyz;
			pos2 = pos2.xyz;

			projectionIndex = 0;
		}
		else if (absNormal.y > absNormal.x)
		{

			pos0 = pos0.zxy;
			pos1 = pos1.zxy;
			pos2 = pos2.zxy;

			projectionIndex = 1;
		}
		else
		{

			pos0 = pos0.yzx;
			pos1 = pos1.yzx;
			pos2 = pos2.yzx;

			projectionIndex = 2;
		}

		Voxelize_GSIn TMP[3];
		TMP[0] = IN[0];
		TMP[1] = IN[1];
		TMP[2] = IN[2];

		float doubleArea = (-pos1.x * pos0.y + pos2.x * pos0.y + pos0.x * pos1.y - pos2.x * pos1.y - pos0.x * pos2.y + pos1.x * pos2.y);
		float halfInvArea = rcp(doubleArea);
		float areaSign = sign(doubleArea);
		halfInvArea = abs(halfInvArea);

		OUT.VXGIData.Edge1.xyz = GetEdgeEquation(pos0.xy, pos1.xy) * areaSign;
		OUT.VXGIData.Edge2.xyz = GetEdgeEquation(pos1.xy, pos2.xy) * areaSign;
		OUT.VXGIData.Edge3.xyz = GetEdgeEquation(pos2.xy, pos0.xy) * areaSign;

		OUT.VXGIData.ProjectedBoundingBox.xy = min(min(pos0.xy, pos1.xy), pos2.xy);
		OUT.VXGIData.ProjectedBoundingBox.zw = max(max(pos0.xy, pos1.xy), pos2.xy);

		float2 newPos0 = pos0.xy;
		float2 newPos1 = pos1.xy;
		float2 newPos2 = pos2.xy;
		float halfVoxelSize = rcp(g_VxgiVoxelizationCB.ClipLevelSize) * pow(0.5, g_VxgiVoxelizationCB.MaxClipLevel - clipLevel - 1) * g_VxgiVoxelizationMaterialCB.ResolutionFactors[clipLevel].y;
		RasterizeConservative2D(newPos0, newPos1, newPos2, float2(halfVoxelSize, halfVoxelSize));

		float3x3 factors;
		factors[0] = OUT.VXGIData.Edge2.xyz * halfInvArea;
		factors[1] = OUT.VXGIData.Edge3.xyz * halfInvArea;
		factors[2] = OUT.VXGIData.Edge1.xyz * halfInvArea;
		ExtrapolateAttribute(pos0.z, pos1.z, pos2.z, factors, newPos0, newPos1, newPos2);
		pos0.xy = newPos0;
		pos1.xy = newPos1;
		pos2.xy = newPos2;

		EXTRAPOLATE_USER_ATTRS

		OUT.VXGIData.gl_ViewportIndex = clipLevel * 3 + projectionIndex;

		OUT.UserData = TMP[0].UserData;
		SV_POSITION_ATTR_OUT = float4(pos0.xyz, 1);
		outStream.Append(OUT);

		OUT.UserData = TMP[1].UserData;
		SV_POSITION_ATTR_OUT = float4(pos1.xyz, 1);
		outStream.Append(OUT);

		OUT.UserData = TMP[2].UserData;
		SV_POSITION_ATTR_OUT = float4(pos2.xyz, 1);
		outStream.Append(OUT);
		outStream.RestartStrip();
	}
}
//////////////////APP CODE BOUNDARY/////////////