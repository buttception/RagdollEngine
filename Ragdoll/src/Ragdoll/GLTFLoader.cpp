#include "ragdollpch.h"
#include "GLTFLoader.h"

#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"

#include "Executor.h"
#include "Profiler.h"

#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Ragdoll/Components/PointLightComp.h"
#include "Scene.h"
#include "meshoptimizer.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_USE_CPP14
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include <tiny_gltf.h>

#include <nvrhi/common/dxgi-format.h>
#include "DirectXTex.h"

struct DDS_HEADER {
	DWORD           dwSize;
	DWORD           dwFlags;
	DWORD           dwHeight;
	DWORD           dwWidth;
	DWORD           dwPitchOrLinearSize;
	DWORD           dwDepth;
	DWORD           dwMipMapCount;
	DWORD           dwReserved1[11];
	struct DDS_PIXELFORMAT {
		DWORD dwSize;
		DWORD dwFlags;
		DWORD dwFourCC;
		DWORD dwRGBBitCount;
		DWORD dwRBitMask;
		DWORD dwGBitMask;
		DWORD dwBBitMask;
		DWORD dwABitMask;
	} ddspf;
	DWORD           dwCaps;
	DWORD           dwCaps2;
	DWORD           dwCaps3;
	DWORD           dwCaps4;
	DWORD           dwReserved2;
};

struct DDS_HEADER_DXT10 {
	uint32_t dxgiFormat;
	uint32_t resourceDimension;
	uint32_t miscFlag;
	uint32_t arraySize;
	uint32_t miscFlags2;
};

size_t DirectX::BitsPerPixel(DXGI_FORMAT fmt) noexcept
{
	switch (static_cast<int>(fmt))
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 128;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 96;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		return 64;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
		/*
	case XBOX_DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
	case XBOX_DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
	case XBOX_DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
		*/
		return 32;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		/*
	case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
	case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
	case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
	case WIN10_DXGI_FORMAT_V408:
		*/
		return 24;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_A8P8:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		/*
	case WIN10_DXGI_FORMAT_P208:
	case WIN10_DXGI_FORMAT_V208:
	case WIN11_DXGI_FORMAT_A4B4G4R4_UNORM:
		*/
		return 16;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
		return 12;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
		/*
	case XBOX_DXGI_FORMAT_R4G4_UNORM:
		*/
		return 8;

	case DXGI_FORMAT_R1_UNORM:
		return 1;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 4;

	default:
		return 0;
	}
}

bool DirectX::IsPacked(DXGI_FORMAT fmt) noexcept
{
	switch (static_cast<int>(fmt))
	{
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2: // 4:2:2 8-bit
	case DXGI_FORMAT_Y210: // 4:2:2 10-bit
	case DXGI_FORMAT_Y216: // 4:2:2 16-bit
		return true;

	default:
		return false;
	}
}

bool DirectX::IsPlanar(DXGI_FORMAT fmt) noexcept
{
	switch (static_cast<int>(fmt))
	{
	case DXGI_FORMAT_NV12:      // 4:2:0 8-bit
	case DXGI_FORMAT_P010:      // 4:2:0 10-bit
	case DXGI_FORMAT_P016:      // 4:2:0 16-bit
	case DXGI_FORMAT_420_OPAQUE:// 4:2:0 8-bit
	case DXGI_FORMAT_NV11:      // 4:1:1 8-bit

		/*
	case WIN10_DXGI_FORMAT_P208: // 4:2:2 8-bit
	case WIN10_DXGI_FORMAT_V208: // 4:4:0 8-bit
	case WIN10_DXGI_FORMAT_V408: // 4:4:4 8-bit
		// These are JPEG Hardware decode formats (DXGI 1.4)

	case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
	case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
	case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
		// These are Xbox One platform specific types
		*/
		return true;

	default:
		return false;
	}
}


HRESULT DirectX::ComputePitch(DXGI_FORMAT fmt, size_t width, size_t height,
	size_t& rowPitch, size_t& slicePitch, CP_FLAGS flags) noexcept
{
	uint64_t pitch = 0;
	uint64_t slice = 0;

	switch (static_cast<int>(fmt))
	{
	case DXGI_FORMAT_UNKNOWN:
		return E_INVALIDARG;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		assert(IsCompressed(fmt));
		{
			if (flags & CP_FLAGS_BAD_DXTN_TAILS)
			{
				const size_t nbw = width >> 2;
				const size_t nbh = height >> 2;
				pitch = std::max<uint64_t>(1u, uint64_t(nbw) * 8u);
				slice = std::max<uint64_t>(1u, pitch * uint64_t(nbh));
			}
			else
			{
				const uint64_t nbw = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
				const uint64_t nbh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
				pitch = nbw * 8u;
				slice = pitch * nbh;
			}
		}
		break;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		assert(IsCompressed(fmt));
		{
			if (flags & CP_FLAGS_BAD_DXTN_TAILS)
			{
				const size_t nbw = width >> 2;
				const size_t nbh = height >> 2;
				pitch = std::max<uint64_t>(1u, uint64_t(nbw) * 16u);
				slice = std::max<uint64_t>(1u, pitch * uint64_t(nbh));
			}
			else
			{
				const uint64_t nbw = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
				const uint64_t nbh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
				pitch = nbw * 16u;
				slice = pitch * nbh;
			}
		}
		break;

	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2:
		assert(IsPacked(fmt));
		pitch = ((uint64_t(width) + 1u) >> 1) * 4u;
		slice = pitch * uint64_t(height);
		break;

	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		assert(IsPacked(fmt));
		pitch = ((uint64_t(width) + 1u) >> 1) * 8u;
		slice = pitch * uint64_t(height);
		break;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
		if ((height % 2) != 0)
		{
			// Requires a height alignment of 2.
			return E_INVALIDARG;
		}
		assert(IsPlanar(fmt));
		pitch = ((uint64_t(width) + 1u) >> 1) * 2u;
		slice = pitch * (uint64_t(height) + ((uint64_t(height) + 1u) >> 1));
		break;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		if ((height % 2) != 0)
		{
			// Requires a height alignment of 2.
			return E_INVALIDARG;
		}

#if (__cplusplus >= 201703L)
		[[fallthrough]];
#elif defined(__clang__)
		[[clang::fallthrough]];
#elif defined(_MSC_VER)
		__fallthrough;
#endif

	/*format i do not need
	case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
	case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
	case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
		assert(IsPlanar(fmt));
		pitch = ((uint64_t(width) + 1u) >> 1) * 4u;
		slice = pitch * (uint64_t(height) + ((uint64_t(height) + 1u) >> 1));
		break;

	case DXGI_FORMAT_NV11:
		assert(IsPlanar(fmt));
		pitch = ((uint64_t(width) + 3u) >> 2) * 4u;
		slice = pitch * uint64_t(height) * 2u;
		break;

	case WIN10_DXGI_FORMAT_P208:
		assert(IsPlanar(fmt));
		pitch = ((uint64_t(width) + 1u) >> 1) * 2u;
		slice = pitch * uint64_t(height) * 2u;
		break;

	case WIN10_DXGI_FORMAT_V208:
		if ((height % 2) != 0)
		{
			// Requires a height alignment of 2.
			return E_INVALIDARG;
		}
		assert(IsPlanar(fmt));
		pitch = uint64_t(width);
		slice = pitch * (uint64_t(height) + (((uint64_t(height) + 1u) >> 1) * 2u));
		break;

	case WIN10_DXGI_FORMAT_V408:
		assert(IsPlanar(fmt));
		pitch = uint64_t(width);
		slice = pitch * (uint64_t(height) + (uint64_t(height >> 1) * 4u));
		break;
	*/

	default:
		assert(!IsCompressed(fmt) && !IsPacked(fmt) && !IsPlanar(fmt));
		{
			size_t bpp;

			if (flags & CP_FLAGS_24BPP)
				bpp = 24;
			else if (flags & CP_FLAGS_16BPP)
				bpp = 16;
			else if (flags & CP_FLAGS_8BPP)
				bpp = 8;
			else
				bpp = BitsPerPixel(fmt);

			if (!bpp)
				return E_INVALIDARG;

			if (flags & (CP_FLAGS_LEGACY_DWORD | CP_FLAGS_PARAGRAPH | CP_FLAGS_YMM | CP_FLAGS_ZMM | CP_FLAGS_PAGE4K))
			{
				if (flags & CP_FLAGS_PAGE4K)
				{
					pitch = ((uint64_t(width) * bpp + 32767u) / 32768u) * 4096u;
					slice = pitch * uint64_t(height);
				}
				else if (flags & CP_FLAGS_ZMM)
				{
					pitch = ((uint64_t(width) * bpp + 511u) / 512u) * 64u;
					slice = pitch * uint64_t(height);
				}
				else if (flags & CP_FLAGS_YMM)
				{
					pitch = ((uint64_t(width) * bpp + 255u) / 256u) * 32u;
					slice = pitch * uint64_t(height);
				}
				else if (flags & CP_FLAGS_PARAGRAPH)
				{
					pitch = ((uint64_t(width) * bpp + 127u) / 128u) * 16u;
					slice = pitch * uint64_t(height);
				}
				else // DWORD alignment
				{
					// Special computation for some incorrectly created DDS files based on
					// legacy DirectDraw assumptions about pitch alignment
					pitch = ((uint64_t(width) * bpp + 31u) / 32u) * sizeof(uint32_t);
					slice = pitch * uint64_t(height);
				}
			}
			else
			{
				// Default byte alignment
				pitch = (uint64_t(width) * bpp + 7u) / 8u;
				slice = pitch * uint64_t(height);
			}
		}
		break;
	}

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
	static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
	if (pitch > UINT32_MAX || slice > UINT32_MAX)
	{
		rowPitch = slicePitch = 0;
		return HRESULT_E_ARITHMETIC_OVERFLOW;
	}
#else
	static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
#endif

	rowPitch = static_cast<size_t>(pitch);
	slicePitch = static_cast<size_t>(slice);

	return S_OK;
}

nvrhi::Format SearchForFormat(uint32_t flag)
{
	for (int i = 0; i < (int)nvrhi::Format::COUNT; ++i)
	{
		nvrhi::DxgiFormatMapping mapping = nvrhi::getDxgiFormatMapping((nvrhi::Format)i);
		if (mapping.resourceFormat == flag)
			return mapping.abstractFormat;
		if (mapping.rtvFormat == flag)
			return mapping.abstractFormat;
		if (mapping.srvFormat == flag)
			return mapping.abstractFormat;
	}
}

void GLTFLoader::Init(std::filesystem::path root, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene)
{
	Root = root;
	FileManagerRef = fm;
	EntityManagerRef = em;
	SceneRef = scene;
}

void AddToFurthestSibling(ragdoll::Guid child, ragdoll::Guid newChild, std::shared_ptr<ragdoll::EntityManager> em)
{
	TransformComp* trans = em->GetComponent<TransformComp>(child);
	if (trans->m_Sibling.m_RawId != 0) {
		AddToFurthestSibling(trans->m_Sibling, newChild, em);
	}
	else
		trans->m_Sibling = newChild;
}

ragdoll::Guid TraverseNode(int32_t currIndex, int32_t level, uint32_t meshIndicesOffset, const tinygltf::Model& model, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene)
{
	static std::stack<Matrix> modelStack;
	const tinygltf::Node& curr = model.nodes[currIndex];
	//create the entity
	entt::entity ent = em->CreateEntity();
	ragdoll::Guid currId = em->GetGuid(ent);

	TransformComp* transComp = em->AddComponent<TransformComp>(ent);
	transComp->glTFId = currIndex;
	transComp->m_Dirty = true;
	//root level entities
	if (level == 0)
	{
		scene->AddEntityAtRootLevel(currId);
	}

	if (curr.matrix.size() > 0) {
		const std::vector<double>& gltfMat = curr.matrix;
		Matrix mat = {
			(float)gltfMat[0], (float)gltfMat[1], (float)gltfMat[2], (float)gltfMat[3], 
			(float)gltfMat[4], (float)gltfMat[5], (float)gltfMat[6], (float)gltfMat[7], 
			(float)gltfMat[8], (float)gltfMat[9], (float)gltfMat[10], (float)gltfMat[11],
			(float)gltfMat[12], (float)gltfMat[13], (float)gltfMat[14], (float)gltfMat[15]};
		mat.Decompose(transComp->m_LocalScale, transComp->m_LocalRotation, transComp->m_LocalPosition);
	}
	else {
		if (curr.translation.size() > 0)
			transComp->m_LocalPosition = { (float)curr.translation[0], (float)curr.translation[1], (float)curr.translation[2] };
		if (curr.rotation.size() > 0)
			transComp->m_LocalRotation = { (float)curr.rotation[0], (float)curr.rotation[1], (float)curr.rotation[2], (float)curr.rotation[3] };
		if (curr.scale.size() > 0)
			transComp->m_LocalScale = { (float)curr.scale[0], (float)curr.scale[1], (float)curr.scale[2] };
	}

	if (curr.mesh >= 0) {
		RenderableComp* renderableComp = em->AddComponent<RenderableComp>(ent);
		renderableComp->meshIndex = curr.mesh + meshIndicesOffset;
	}
	else
	{
		//if there is no mesh, there is a chance it is camera
		for (ragdoll::SceneCamera& SceneCamera : scene->SceneInfo.Cameras)
		{
			if (SceneCamera.Name == curr.name)
			{
				SceneCamera.Position = transComp->m_LocalPosition;
				SceneCamera.Rotation = transComp->m_LocalRotation.ToEuler();
				SceneCamera.Rotation.y += DirectX::XM_PI;
				SceneCamera.Rotation.x = -SceneCamera.Rotation.x;
				break;
			}
		}
	}

	if (curr.light >= 0)
	{
		PointLightComp* lightComp = em->AddComponent<PointLightComp>(ent);
		const tinygltf::Light& light = model.lights[curr.light];
		if (light.color.size() == 0)
			lightComp->Color = { 1.f, 1.f, 1.f };
		else
			lightComp->Color = { (float)light.color[0], (float)light.color[1], (float)light.color[2] };
		lightComp->Intensity = (float)light.intensity;
		lightComp->Range = (float)light.range;
	}

	const tinygltf::Node& parent = model.nodes[currIndex];
	for (const int& childIndex : parent.children) {
		ragdoll::Guid childId = TraverseNode(childIndex, level + 1, meshIndicesOffset, model, em, scene);
		if (transComp->m_Child.m_RawId == 0) {
			transComp->m_Child = childId;
		}
		else {
			AddToFurthestSibling(transComp->m_Child, childId, em);
		}
	}

	return currId;
}

enum AttributeType {
	None,
	Position,
	Color,
	Normal,
	Tangent,
	Binormal,
	Texcoord,

	Count
};

void GLTFLoader::LoadAndCreateModel(const std::string& fileName)
{
	RD_SCOPE(Load, Load GLTF);
	//ownself open command list
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err, warn;
	std::filesystem::path path = Root / fileName;
	std::filesystem::path modelRoot = path.parent_path().lexically_relative(Root);
	{
		RD_SCOPE(Load, Load GLTF File);
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
		RD_ASSERT(ret == false, "Issue loading {}", path.string());
	}

	{
		//need to add cameras first with name, so when i load the node it can be assigned
		RD_SCOPE(Load, Load Cameras);
		SceneRef->SceneInfo.Cameras.clear();
		for (const tinygltf::Camera& itCam : model.cameras)
		{
			SceneRef->SceneInfo.Cameras.push_back({ itCam.name });
		}
	}

	uint32_t meshIndicesOffset = static_cast<uint32_t>(AssetManager::GetInstance()->Meshes.size());
	//load meshes
	{
		RD_SCOPE(Load, Load Meshes);
		// go downwards from meshes
		for (const auto& itMesh : model.meshes) {
			//should not create a new input layout handle, use the one provided by the renderer
			std::vector<Vertex> newBuffer;
			std::unordered_map<std::string, tinygltf::Accessor> attributeToAccessors;
			//for every mesh, create a new mesh object
			Mesh mesh;
			//keep track of what is the currently loaded buffer and data
			uint32_t binSize = 0;
			uint32_t vertexCount{};

			//load all the submeshes
			size_t materialIndicesOffset = AssetManager::GetInstance()->Materials.size();
			for (const tinygltf::Primitive& itPrim : itMesh.primitives)
			{
				RD_SCOPE(Load, Mesh);
				Submesh submesh{};
				DirectX::BoundingBox box;
				VertexBufferInfo buffer;
				IndexStagingBuffer.clear();
				VertexStagingBuffer.clear();

				//set the material index for the primitive
				submesh.MaterialIndex = itPrim.material + materialIndicesOffset;

				//load the indices first
				{
					const tinygltf::Accessor& accessor = model.accessors[itPrim.indices];
					const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
					IndexStagingBuffer.resize(accessor.count);
					uint8_t* data = model.buffers[bufferView.buffer].data.data();
					size_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
					//manually assign to reconcil things like short to uint
					for (int i = 0; i < accessor.count; ++i) {
						switch (accessor.componentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
							IndexStagingBuffer[i] = *reinterpret_cast<uint8_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						case TINYGLTF_COMPONENT_TYPE_SHORT:
							IndexStagingBuffer[i] = *reinterpret_cast<int16_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							IndexStagingBuffer[i] = *reinterpret_cast<uint16_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
							IndexStagingBuffer[i] = *reinterpret_cast<uint32_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						default:
							RD_ASSERT(true, "Unsupport index type for {}", itMesh.name);
						}
						if (accessor.maxValues.size() != 0)
							RD_ASSERT(IndexStagingBuffer[i] > accessor.maxValues[0], "");
					}
#if 0
					RD_CORE_TRACE("Loaded {} indices at byte offest {}", accessor.count, accessor.byteOffset);
					RD_CORE_TRACE("Largest index is {}", *std::max_element(indices.begin(), indices.end()));
#endif
					buffer.IndicesCount = (uint32_t)accessor.count;
				}

				//add the relevant data into the map to use
				{
					for (const auto& itAttrib : itPrim.attributes) {
						tinygltf::Accessor vertexAccessor = model.accessors[itAttrib.second];
						tinygltf::BufferView bufferView = model.bufferViews[vertexAccessor.bufferView];
						attributeToAccessors[itAttrib.first] = vertexAccessor;
						vertexCount = (uint32_t)vertexAccessor.count;
					}
					RD_ASSERT(vertexCount == 0, "There are no vertices?");
				}

				//for every attribute, check if there is one that corresponds with renderer attributes
				bool tangentExist = false, binormalExist = false;
				{
					VertexStagingBuffer.resize(vertexCount);
					for (const auto& it : attributeToAccessors) {
						nvrhi::VertexAttributeDesc const* desc = nullptr;
						const tinygltf::Accessor& accessor = it.second;
						const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
						AttributeType type;
						for (const nvrhi::VertexAttributeDesc& itDesc : AssetManager::GetInstance()->InstancedVertexAttributes) {
							if (it.first.find(itDesc.name) != std::string::npos)
							{
								if (itDesc.name == "POSITION")
									type = AttributeType::Position;
								else if (itDesc.name == "COLOR")
									continue;	//skip vertex color
								else if (itDesc.name == "NORMAL")
									type = AttributeType::Normal;
								else if (itDesc.name == "TANGENT") {
									type = AttributeType::Tangent;
									tangentExist = true;
								}
								else if (itDesc.name == "BINORMAL") {
									type = AttributeType::Binormal;
									binormalExist = true;
								}
								else if (itDesc.name == "TEXCOORD")
									type = AttributeType::Texcoord;
								desc = &itDesc;
							}
						}
						RD_ASSERT(desc == nullptr, "Loaded mesh contains a attribute not supported by the renderer: {}", it.first);
						uint32_t size = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
						//memcpy the data from the buffer over into the vertices
						uint8_t* data = model.buffers[bufferView.buffer].data.data();
						for (uint32_t i = 0; i < vertexCount; ++i) {
							Vertex& v = VertexStagingBuffer[i];
							uint8_t* bytePos = reinterpret_cast<uint8_t*>(&v) + desc->offset;
							size_t byteOffset = accessor.byteOffset + bufferView.byteOffset;
							size_t stride = bufferView.byteStride == 0 ? size : bufferView.byteStride;
							memcpy(bytePos, data + byteOffset + i * stride, size);
						}

						if (type == AttributeType::Position)
						{
							if (accessor.maxValues.size() == 3 && accessor.minValues.size() == 3)
							{
								Vector3 max{ (float)accessor.maxValues[0], (float)accessor.maxValues[1], (float)accessor.maxValues[2] };
								Vector3 min{ (float)accessor.minValues[0], (float)accessor.minValues[1], (float)accessor.minValues[2] };
								DirectX::BoundingBox::CreateFromPoints(box, min, max);
							}
							else
							{
								Vector3 min, max;
								min = max = VertexStagingBuffer[0].position;
								for (const Vertex& v : VertexStagingBuffer) {
									min.x = std::min(v.position.x, min.x); max.x = std::max(v.position.x, max.x);
									min.y = std::min(v.position.y, min.y); max.y = std::max(v.position.y, max.y);
									min.z = std::min(v.position.z, min.z); max.z = std::max(v.position.z, max.z);
								}
								DirectX::BoundingBox::CreateFromPoints(box, min, max);
							}
						}
#if 0
						RD_CORE_INFO("Loaded {} vertices of attribute: {}", vertexCount, desc->name);
#endif
					}
				}

				//if tangent or binormal does not exist, generate them
				{
					if (!tangentExist || !binormalExist)
					{
						//generate the tangents and binormals
						for (size_t i = 0; i < IndexStagingBuffer.size(); i += 3) {
							Vertex& v0 = VertexStagingBuffer[IndexStagingBuffer[i]];
							Vertex& v1 = VertexStagingBuffer[IndexStagingBuffer[i + 1]];
							Vertex& v2 = VertexStagingBuffer[IndexStagingBuffer[i + 2]];

							Vector3 edge1 = v1.position - v0.position;
							Vector3 edge2 = v2.position - v0.position;

							Vector2 deltaUV1 = v1.texcoord - v0.texcoord;
							Vector2 deltaUV2 = v2.texcoord - v0.texcoord;

							float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

							Vector3 tangent;
							if (tangentExist)
								tangent = VertexStagingBuffer[IndexStagingBuffer[i]].tangent;
							else {
								tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
								tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
								tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

								// Normalize and store tangent
								tangent.Normalize();
								v0.tangent = v1.tangent = v2.tangent = tangent;
							}
						}
					}
				}

				//add to the asset manager vertices
				{
					submesh.VertexBufferIndex = AssetManager::GetInstance()->AddVertices(VertexStagingBuffer, IndexStagingBuffer);
					AssetManager::GetInstance()->VertexBufferInfos[submesh.VertexBufferIndex].BestFitBox = box;
					mesh.Submeshes.emplace_back(submesh);
				}

				//create meshlets
				{
					//temp
					const float cone_weight = 0.f;	//not using cone weight, it is used for culling

					size_t max_meshlets = meshopt_buildMeshletsBound(IndexStagingBuffer.size(), max_vertices, max_triangles);
					Submesh& submesh = mesh.Submeshes.back();
					submesh.Meshlets.resize(max_meshlets);
					submesh.MeshletVertices.resize(max_meshlets* max_vertices);
					std::vector<uint8_t> MeshletTrianglesUnpacked(max_meshlets* max_triangles * 3);

					submesh.MeshletCount = meshopt_buildMeshlets(submesh.Meshlets.data(), submesh.MeshletVertices.data(), MeshletTrianglesUnpacked.data(), IndexStagingBuffer.data(), IndexStagingBuffer.size(), (float*)VertexStagingBuffer.data(), VertexStagingBuffer.size(), sizeof(Vertex), max_vertices, max_triangles, cone_weight);

					const meshopt_Meshlet& last = submesh.Meshlets[submesh.MeshletCount - 1];

					submesh.MeshletVertices.resize(last.vertex_offset + last.vertex_count);
					MeshletTrianglesUnpacked.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
					submesh.MeshletTrianglesPacked.clear();
					submesh.MeshletTrianglesPacked.reserve((last.triangle_offset + last.triangle_count * 3) / 3);
					submesh.Meshlets.resize(submesh.MeshletCount);

					for (int i = 0; i < submesh.MeshletCount; ++i)
					{
						uint32_t triangle_offset = submesh.MeshletTrianglesPacked.size();
						for (int j = submesh.Meshlets[i].triangle_offset; j < submesh.Meshlets[i].triangle_offset + submesh.Meshlets[i].triangle_count * 3; j += 3)
						{
							uint32_t packed = 0;
							packed |= uint32_t(MeshletTrianglesUnpacked[j + 0]) << 0;
							packed |= uint32_t(MeshletTrianglesUnpacked[j + 1]) << 8;
							packed |= uint32_t(MeshletTrianglesUnpacked[j + 2]) << 16;
							submesh.MeshletTrianglesPacked.emplace_back(packed);
						}
						submesh.Meshlets[i].triangle_offset = triangle_offset;
					}
				}
			}
#if 0
			RD_CORE_INFO("Mesh: {}", itMesh.name);
			for (const auto& it : vertices)
			{
				RD_CORE_INFO("Pos: {}, Normal: {}, Texcoord: {}", it.position, it.normal, it.texcoord);
			}
			for (const auto& it : indices)
			{
				RD_CORE_INFO("Index: {}", it);
			}
#endif
			AssetManager::GetInstance()->Meshes.emplace_back(mesh);
		}
	}
	//create the buffers
	{
		AssetManager::GetInstance()->UpdateVBOIBO();
	}
	uint32_t textureIndicesOffset = static_cast<uint32_t>(AssetManager::GetInstance()->Textures.size());
	uint32_t imageIndicesOffset = static_cast<uint32_t>(AssetManager::GetInstance()->Images.size());

	//load materials
	{
		RD_SCOPE(Load, Load Materials);
		//load all of the materials
		for (const tinygltf::Material& gltfMat : model.materials)
		{
			Material mat;
			//default gltf data
			mat.Metallic = (float)gltfMat.pbrMetallicRoughness.metallicFactor;
			mat.Roughness = (float)gltfMat.pbrMetallicRoughness.roughnessFactor;
			mat.Color = Vector4(
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[0]),
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[1]),
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[2]),
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[3]));
			if (gltfMat.alphaMode == "MASK")
				mat.AlphaMode = Material::AlphaMode::GLTF_MASK;
			if(gltfMat.alphaMode == "BLEND")
				mat.AlphaMode = Material::AlphaMode::GLTF_BLEND;
			mat.AlphaCutoff = (float)gltfMat.alphaCutoff;
			mat.bIsDoubleSided = gltfMat.doubleSided;
			//no extension textures
			if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0)
				mat.AlbedoTextureIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index + textureIndicesOffset;
			if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
				mat.RoughnessMetallicTextureIndex = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index + textureIndicesOffset;
			if (gltfMat.normalTexture.index >= 0)
				mat.NormalTextureIndex = gltfMat.normalTexture.index + textureIndicesOffset;
			if (gltfMat.extensions.contains("KHR_materials_pbrSpecularGlossiness"))
			{
				const tinygltf::Value& pbrSpecGlossMap = gltfMat.extensions.at("KHR_materials_pbrSpecularGlossiness");
				//get the extension textures
				//diffuse factor
				if (pbrSpecGlossMap.Has("diffuseFactor"))
				{
					mat.Color = Vector4(
						static_cast<float>(pbrSpecGlossMap.Get("diffuseFactor").Get(0).GetNumberAsDouble()),
						static_cast<float>(pbrSpecGlossMap.Get("diffuseFactor").Get(1).GetNumberAsDouble()),
						static_cast<float>(pbrSpecGlossMap.Get("diffuseFactor").Get(2).GetNumberAsDouble()),
						static_cast<float>(pbrSpecGlossMap.Get("diffuseFactor").Get(3).GetNumberAsDouble()));
				}
				if (pbrSpecGlossMap.Has("specularFactor"))				
					mat.Metallic = (float)pbrSpecGlossMap.Get("specularFactor").GetNumberAsDouble();
				if(pbrSpecGlossMap.Has("diffuseTexture"))
					mat.AlbedoTextureIndex = pbrSpecGlossMap.Get("diffuseTexture").Get("index").GetNumberAsInt() + textureIndicesOffset;
				if (pbrSpecGlossMap.Has("specularGlossinessTexture"))
					mat.RoughnessMetallicTextureIndex = pbrSpecGlossMap.Get("specularGlossinessTexture").Get("index").GetNumberAsInt() + textureIndicesOffset;
			}
			AssetManager::GetInstance()->Materials.emplace_back(mat);
		}
	}

	{
		RD_SCOPE(Load, Loading Textures);
		AssetManager::GetInstance()->Textures.resize(model.textures.size() + textureIndicesOffset);
		AssetManager::GetInstance()->Images.resize(model.textures.size() + imageIndicesOffset);
		std::vector<nvrhi::ICommandList*> cmdLists(model.textures.size());
		std::vector<nvrhi::CommandListHandle> cmdListHandles(model.textures.size());
		tf::Taskflow TaskFlow;
		for (uint32_t i = 0; i < model.textures.size(); ++i)
		{
			const tinygltf::Texture& itTex = model.textures[i];
			//textures contain a sampler and an image
			Texture& tex = AssetManager::GetInstance()->Textures[i + textureIndicesOffset];
			SamplerTypes type;
			if (itTex.sampler < 0)
			{
				type = SamplerTypes::Anisotropic_Repeat;
			}
			else
			{
				//create the sampler
				const tinygltf::Sampler gltfSampler = model.samplers[itTex.sampler];
				switch (gltfSampler.wrapS)
				{
				case TINYGLTF_TEXTURE_WRAP_REPEAT:
					switch (gltfSampler.minFilter)
					{
					case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
					case TINYGLTF_TEXTURE_FILTER_LINEAR:
						if (gltfSampler.magFilter == 1)
							type = SamplerTypes::Trilinear_Repeat;
						else
							type = SamplerTypes::Linear_Repeat;
						break;
					case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
					case TINYGLTF_TEXTURE_FILTER_NEAREST:
						type = SamplerTypes::Point_Repeat;
						break;
					case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
					case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
					case -1:
						RD_CORE_WARN("Sampler do not exist, giving trilinear");
						type = SamplerTypes::Trilinear_Repeat;
						break;
					default:
						RD_ASSERT(true, "Unknown min filter");
					}
					break;
				case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
					switch (gltfSampler.minFilter)
					{
					case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
					case TINYGLTF_TEXTURE_FILTER_LINEAR:
						if (gltfSampler.magFilter == 1)
							type = SamplerTypes::Trilinear_Clamp;
						else
							type = SamplerTypes::Linear_Clamp;
						break;
					case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
					case TINYGLTF_TEXTURE_FILTER_NEAREST:
						type = SamplerTypes::Point_Clamp;
						break;
					case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
					case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
					case -1:
						RD_CORE_WARN("Sampler do not exist, giving trilinear");
						type = SamplerTypes::Trilinear_Clamp;
						break;
					default:
						RD_ASSERT(true, "Unknown min filter");
					}
					break;
				case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
					switch (gltfSampler.minFilter)
					{
					case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
					case TINYGLTF_TEXTURE_FILTER_LINEAR:
						if (gltfSampler.magFilter == 1)
							type = SamplerTypes::Trilinear_Repeat;
						else
							type = SamplerTypes::Linear_Repeat;
						break;
					case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
					case TINYGLTF_TEXTURE_FILTER_NEAREST:
						type = SamplerTypes::Point_Repeat;
						break;
					case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
					case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
					case -1:
						RD_CORE_WARN("Sampler do not exist, giving trilinear");
						type = SamplerTypes::Trilinear_Repeat;
						break;
					default:
						RD_ASSERT(true, "Unknown min filter");
					}
					break;
				}
			}
			tex.SamplerIndex = (int)type;
			tex.ImageIndex = i + imageIndicesOffset;

			//enqueue command to load image associated with the texture
			cmdLists[i] = cmdListHandles[i] = DirectXDevice::GetNativeDevice()->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
			nvrhi::CommandListHandle hdl = cmdListHandles[i];

			//check if texture has a dds extension, if it does, load that instead
			uint32_t imageIndex = itTex.source;
			if (itTex.extensions.contains("MSFT_texture_dds"))
			{
				imageIndex = itTex.extensions.at("MSFT_texture_dds").Get("source").GetNumberAsInt();
			}
			Image* img = &AssetManager::GetInstance()->Images[i + imageIndicesOffset];
			tinygltf::Image* itImg = &model.images[imageIndex];
			//push the loading into the taskflow
			TaskFlow.emplace(
				[itImg, path, img, imageIndicesOffset, i, hdl]()
				{
					std::vector<uint8_t> data;
					int32_t size;
					{
						RD_SCOPE(Load, Load File);
						std::filesystem::path modelPath = path.parent_path() / itImg->uri;
						//load raw bytes, do not use stbi load
						std::ifstream file(modelPath, std::ios::binary | std::ios::ate);
						RD_ASSERT(!file, "Unable to open file {}:{}", strerror(errno), modelPath.string());
						size = static_cast<int32_t>(file.tellg());
						file.seekg(0, std::ios::beg);
						data.resize(size);
						RD_ASSERT(!file.read((char*)data.data(), size), "Failed to read file {}", itImg->uri);
					}
					if (itImg->uri.find(".dds") == std::string::npos)
					{
						RD_SCOPE(Load, STB Load);
						//use stbi_info_from_memory to check header on info for how to load
						int w = -1, h = -1, comp = -1, req_comp = 0;
						RD_ASSERT(!stbi_info_from_memory(data.data(), size, &w, &h, &comp), "stb unable to read image {}", itImg->uri);
						int bits = 8;
						int pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
						if (comp == 3)
							req_comp = 4;
						//dont support hdr images
						//use stbi_load_from_memory to load the image data, set up all the desc with that info
						uint8_t* raw = stbi_load_from_memory(data.data(), size, &w, &h, &comp, req_comp);
						RD_ASSERT(!raw, "Issue loading {}", itImg->uri);
						itImg->width = w;
						itImg->height = h;
						itImg->component = comp;
						itImg->bits = bits;
						itImg->pixel_type = pixel_type;
						//do not free the image here, free it when creating textures in gpu
						img->RawData = raw;

						nvrhi::TextureDesc texDesc;
						texDesc.width = itImg->width;
						texDesc.height = itImg->height;
						texDesc.dimension = nvrhi::TextureDimension::Texture2D;
						switch (itImg->component)
						{
						case 1:
							texDesc.format = nvrhi::Format::R8_UNORM;
							break;
						case 2:
							texDesc.format = nvrhi::Format::RG8_UNORM;
							break;
						case 3:
							texDesc.format = nvrhi::Format::RGBA8_UNORM;
							break;
						case 4:
							texDesc.format = nvrhi::Format::RGBA8_UNORM;
							break;
						default:
							RD_ASSERT(true, "Unsupported texture channel count");
						}
						texDesc.debugName = itImg->uri;
						texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
						texDesc.isRenderTarget = false;
						texDesc.keepInitialState = true;

						img->TextureHandle = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
						img->bIsDDS = false;
						RD_ASSERT(img->TextureHandle == nullptr, "Issue creating texture handle: {}", itImg->uri);

						DirectXDevice::GetInstance()->m_NvrhiDevice->writeDescriptorTable(AssetManager::GetInstance()->DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(i + imageIndicesOffset, img->TextureHandle));

						hdl->open();
						//upload the texture data
						hdl->writeTexture(img->TextureHandle, 0, 0, img->RawData, itImg->width* (itImg->component == 3 ? 4 : itImg->component));
						hdl->close();
					}
					else
					{
						RD_SCOPE(Load, DDS Load);

						// Validate DDS header
						const uint8_t* ddsData = reinterpret_cast<const uint8_t*>(data.data());

						if (reinterpret_cast<const uint32_t*>(ddsData)[0] != ' SDD') // DDS Magic Number
							RD_ASSERT(true, "Invalid DDS header for {}", itImg->uri);

						const DDS_HEADER* header = reinterpret_cast<const DDS_HEADER*>(ddsData + sizeof(uint32_t)); // Skip magic number
						bool hasDXT10Header = (header->ddspf.dwFourCC == '01XD'); // DX10 extension
						const DDS_HEADER_DXT10* dxt10Header = hasDXT10Header ? reinterpret_cast<const DDS_HEADER_DXT10*>(ddsData + sizeof(uint32_t) + sizeof(DDS_HEADER)) : nullptr;
						
						nvrhi::Format format{ nvrhi::Format::UNKNOWN };
						if(hasDXT10Header)
							format = SearchForFormat(dxt10Header->dxgiFormat);
						else
						{
							if (header->ddspf.dwFourCC == '1TXD') 
								format = nvrhi::Format::BC1_UNORM;  // DXT1
							if (header->ddspf.dwFourCC == '3TXD') 
								format = nvrhi::Format::BC2_UNORM;  // DXT3
							if (header->ddspf.dwFourCC == '5TXD')
								format = nvrhi::Format::BC3_UNORM;  // DXT5
							if (header->ddspf.dwFourCC == 'U4ET') 
								format = nvrhi::Format::BC4_UNORM;  // ATI1 / BC4
							if (header->ddspf.dwFourCC == '2ITA')
								format = nvrhi::Format::BC5_UNORM;  // ATI2 / BC5
						}

						RD_ASSERT(format == nvrhi::Format::UNKNOWN, "Unsupported DDS format for {}", itImg->uri);

						// Create texture description
						nvrhi::TextureDesc desc;
						desc.width = header->dwWidth;
						desc.height = header->dwHeight;
						desc.mipLevels = header->dwMipMapCount > 0 ? header->dwMipMapCount : 1;
						desc.dimension = (header->dwDepth > 1) ? nvrhi::TextureDimension::Texture3D : nvrhi::TextureDimension::Texture2D;
						desc.format = format;
						desc.isRenderTarget = false;
						desc.initialState = nvrhi::ResourceStates::ShaderResource;
						desc.keepInitialState = true;
						desc.debugName = itImg->uri;

						img->TextureHandle = DirectXDevice::GetNativeDevice()->createTexture(desc);
						RD_ASSERT(img->TextureHandle == nullptr, "Issue creating texture handle: {}", itImg->uri);

						// Bind texture to descriptor table
						DirectXDevice::GetInstance()->m_NvrhiDevice->writeDescriptorTable(
							AssetManager::GetInstance()->DescriptorTable,
							nvrhi::BindingSetItem::Texture_SRV(i + imageIndicesOffset, img->TextureHandle)
						);

						// Determine the start of pixel data in DDS file
						size_t headerSize = sizeof(uint32_t) + sizeof(DDS_HEADER) + (hasDXT10Header ? sizeof(DDS_HEADER_DXT10) : 0);
						const uint8_t* pixelData = ddsData + headerSize;

						hdl->open();

						size_t offset = 0;
						for (uint32_t mipLevel = 0; mipLevel < desc.mipLevels; ++mipLevel) {
							uint32_t mipWidth = std::max(1u, desc.width >> mipLevel);
							uint32_t mipHeight = std::max(1u, desc.height >> mipLevel);

							size_t rowPitch, slicePitch;
							DirectX::ComputePitch(hasDXT10Header ? (DXGI_FORMAT)dxt10Header->dxgiFormat : nvrhi::getDxgiFormatMapping(format).resourceFormat, mipWidth, mipHeight, rowPitch, slicePitch);

							hdl->writeTexture(img->TextureHandle, 0, mipLevel, pixelData + offset, rowPitch, slicePitch);
							offset += slicePitch; // Move to next mip level data
						}

						hdl->close();
					}
				}
			);
		}
		//execute all the task
		SExecutor::Executor.run(TaskFlow).wait();

		DirectXDevice::GetNativeDevice()->executeCommandLists(cmdLists.data(), cmdLists.size());

		//free all raw bytes
		for (auto& it : AssetManager::GetInstance()->Images) {
			if (!it.bIsDDS)
			{
				stbi_image_free(it.RawData);
				it.RawData = nullptr;
			}
		}
	}
	
	{
		RD_SCOPE(Load, Creating Hierarchy);
		//create all the entities and their components
		for (const int& rootIndex : model.scenes[0].nodes) {	//iterating through the all nodes
			TraverseNode(rootIndex, 0, meshIndicesOffset, model, EntityManagerRef, SceneRef);
		}
		SceneRef->UpdateTransforms();
		{
			//iterate through comps and set prev matrix as curr matrix
			auto EcsView = EntityManagerRef->GetRegistry().view<TransformComp>();
			for (const entt::entity& ent : EcsView) {
				TransformComp* comp = EntityManagerRef->GetComponent<TransformComp>(ent);
				comp->m_PrevModelToWorld = comp->m_ModelToWorld;
			}
		}
		{
			Vector3 Min{ FLT_MAX, FLT_MAX, FLT_MAX };
			Vector3 Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
			//get the min and max extends
			auto EcsView = EntityManagerRef->GetRegistry().view<TransformComp, RenderableComp>();
			for (const entt::entity& ent : EcsView)
			{
				const RenderableComp* rComp = EntityManagerRef->GetComponent<RenderableComp>(ent);
				const Mesh& mesh = AssetManager::GetInstance()->Meshes[rComp->meshIndex];
				const TransformComp* tComp = EntityManagerRef->GetComponent<TransformComp>(ent);
				for (const Submesh& submesh : mesh.Submeshes)
				{
					const VertexBufferInfo& Info = AssetManager::GetInstance()->VertexBufferInfos[submesh.VertexBufferIndex];
					DirectX::BoundingBox Box = Info.BestFitBox;
					Box.Transform(Box, tComp->m_ModelToWorld);
					Vector3 Corners[8];
					Box.GetCorners(Corners);
					for (const Vector3& corner : Corners)
					{
						Min = DirectX::XMVectorMin(Min, corner);
						Max = DirectX::XMVectorMax(Max, corner);
					}
				}
			}
			DirectX::BoundingBox::CreateFromPoints(SceneRef->SceneInfo.SceneBounds, Min, Max);
		}
#if 0
		TransformLayer->DebugPrintHierarchy();
#endif
	}
}
