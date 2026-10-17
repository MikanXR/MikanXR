#include "MikanModelResourceManager.h"
#include "GlbMeshWriter.h"
#include "IMkGraphicsContext.h"
#include "IMkShader.h"
#include "IMkTexture.h"
#include "IMkTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "IMkVertexDefinition.h"
#include "IMkWireframeMesh.h"
#include "Logger.h"
#include "MikanRenderModelResource.h"
#include "MikanTextureCache.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "PathUtils.h"

#include "opencv2/imgcodecs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <system_error>

namespace RenderModelResourceUtils
{
std::string getLowercaseExtension(const std::filesystem::path& path)
{
	std::string extension= path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });
	return extension;
}

MikanRenderModelResource::GeometrySourceFile makeSourceFile(const std::filesystem::path& path)
{
	MikanRenderModelResource::GeometrySourceFile sourceFile;
	sourceFile.path= path;

	// A file that cannot be stat'ed hashes as size and time zero
	std::error_code errorCode;
	const uintmax_t fileSize= std::filesystem::file_size(path, errorCode);
	if (!errorCode)
	{
		sourceFile.fileSize= (uint64_t)fileSize;
	}

	const std::filesystem::file_time_type lastWriteTime= std::filesystem::last_write_time(path, errorCode);
	if (!errorCode)
	{
		sourceFile.lastWriteTime= (int64_t)lastWriteTime.time_since_epoch().count();
	}

	return sourceFile;
}

// Reads the positions, normals and texcoords through the mesh's vertex definition and widens u16 indices
bool extractMeshData(IMkTriangulatedMeshPtr mesh, GlbMeshWriter::MeshData& outMesh)
{
	if (mesh->getIndexPerElementCount() != 3)
	{
		return false;
	}

	MkMaterialInstancePtr materialInstance= mesh->getMaterialInstance();
	MkMaterialConstPtr material= materialInstance ? materialInstance->getMaterial() : nullptr;
	IMkShaderPtr program= material ? material->getProgram() : nullptr;
	IMkVertexDefinitionConstPtr vertexDefinition= program ? program->getVertexDefinition() : nullptr;
	if (!vertexDefinition)
	{
		return false;
	}

	const size_t vertexSize= vertexDefinition->getVertexSize();
	const uint32_t vertexCount= mesh->getVertexCount();
	const uint8_t* vertexData= mesh->getVertexData();
	if (vertexData == nullptr)
	{
		return false;
	}

	outMesh.name= mesh->getName();

	auto readAttribute= [&](eVertexSemantic semantic, auto& outValues)
	{
		using ValueType= typename std::decay_t<decltype(outValues)>::value_type;

		const IMkVertexAttribute* attribute= vertexDefinition->getFirstAttributeBySemantic(semantic);
		if (attribute == nullptr)
		{
			return;
		}

		outValues.resize(vertexCount);
		const uint8_t* attributeData= vertexData + attribute->getOffset();
		for (uint32_t vertexIndex= 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			memcpy(&outValues[vertexIndex], attributeData, sizeof(ValueType));
			attributeData+= vertexSize;
		}
	};
	readAttribute(eVertexSemantic::position, outMesh.vertices);
	readAttribute(eVertexSemantic::normal, outMesh.normals);
	readAttribute(eVertexSemantic::texCoord, outMesh.texCoords);

	const size_t indexCount= mesh->getElementCount() * mesh->getIndexPerElementCount();
	const uint8_t* indexData= mesh->getIndexData();
	const size_t indexSize= mesh->getIndexSize();
	outMesh.indices.resize(indexCount);
	for (size_t index= 0; index < indexCount; ++index)
	{
		if (indexSize == sizeof(uint16_t))
		{
			uint16_t value;
			memcpy(&value, indexData + index * indexSize, sizeof(uint16_t));
			outMesh.indices[index]= value;
		}
		else
		{
			memcpy(&outMesh.indices[index], indexData + index * indexSize, sizeof(uint32_t));
		}
	}

	return !outMesh.vertices.empty();
}

// PNG and JPEG files are embedded as they are. Anything else is decoded and re-encoded as PNG.
bool loadEncodedImage(const std::filesystem::path& path, GlbMeshWriter::ImageData& outImage)
{
	const std::string extension= getLowercaseExtension(path);
	if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return false;
		}

		outImage.bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		outImage.mimeType= extension == ".png" ? "image/png" : "image/jpeg";
		return !outImage.bytes.empty();
	}

	const cv::Mat image= cv::imread(path.string(), cv::IMREAD_UNCHANGED);
	if (image.empty() || !cv::imencode(".png", image, outImage.bytes))
	{
		return false;
	}

	outImage.mimeType= "image/png";
	return true;
}
} // namespace RenderModelResourceUtils

MikanRenderModelResource::MikanRenderModelResource(IMkGraphicsContext* ownerGraphicsContext)
	: m_ownerGraphicsContext(ownerGraphicsContext)
{
}

MikanRenderModelResource::~MikanRenderModelResource() { disposeMeshRenderResources(); }

void MikanRenderModelResource::addTriangulatedMesh(IMkTriangulatedMeshPtr mesh)
{
	if (mesh != nullptr)
	{
		assert(mesh->getOwnerContext() == m_ownerGraphicsContext);
		m_triangulatedMeshes.push_back(mesh);
	}
}

void MikanRenderModelResource::addWireframeMesh(IMkWireframeMeshPtr mesh)
{
	if (mesh != nullptr)
	{
		assert(mesh->getOwnerContext() == m_ownerGraphicsContext);
		m_wireframeMeshes.push_back(mesh);
	}
}

std::string MikanRenderModelResource::computeGeometryETag(const std::vector<GeometrySourceFile>& sourceFiles)
{
	constexpr uint64_t k_fnvOffsetBasis= 0xcbf29ce484222325ull;
	constexpr uint64_t k_fnvPrime= 0x100000001b3ull;

	uint64_t hash= k_fnvOffsetBasis;
	auto hashBytes= [&hash](const void* bytes, size_t byteCount)
	{
		const uint8_t* byteData= (const uint8_t*)bytes;
		for (size_t index= 0; index < byteCount; ++index)
		{
			hash^= byteData[index];
			hash*= k_fnvPrime;
		}
	};

	for (const GeometrySourceFile& sourceFile : sourceFiles)
	{
		// The terminator keeps one file's path from running into the next file's fields
		const std::string pathString= PathUtils::pathToUtf8(sourceFile.path);
		hashBytes(pathString.c_str(), pathString.size() + 1);
		hashBytes(&sourceFile.fileSize, sizeof(sourceFile.fileSize));
		hashBytes(&sourceFile.lastWriteTime, sizeof(sourceFile.lastWriteTime));
	}

	char hexString[17];
	snprintf(hexString, sizeof(hexString), "%016llx", (unsigned long long)hash);
	return hexString;
}

void MikanRenderModelResource::finalizeGeometryETag()
{
	m_clientTexturePaths.clear();
	m_clientTextureIndexPerMesh.assign(m_triangulatedMeshes.size(), -1);

	// The importers bind a white texture to a material with no image, which carries nothing to show
	IMkTexturePtr whiteTexture=
		m_ownerGraphicsContext != nullptr
			? m_ownerGraphicsContext->getTextureCache()->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGB)
			: nullptr;

	for (size_t meshIndex= 0; meshIndex < m_triangulatedMeshes.size(); ++meshIndex)
	{
		MkMaterialInstancePtr materialInstance= m_triangulatedMeshes[meshIndex]->getMaterialInstance();

		IMkTexturePtr diffuseTexture;
		if (!materialInstance
			|| !materialInstance->getMutableTextureBySemantic(eUniformSemantic::diffuseTexture, diffuseTexture)
			|| !diffuseTexture || diffuseTexture == whiteTexture)
		{
			continue;
		}

		const std::filesystem::path imagePath= diffuseTexture->getImagePath();
		if (imagePath.empty())
		{
			MIKAN_LOG_WARNING("MikanRenderModelResource::finalizeGeometryETag")
				<< "Mesh " << m_triangulatedMeshes[meshIndex]->getName() << " in " << m_renderModelFilepath.string()
				<< " has an embedded texture, which client geometry does not carry";
			continue;
		}

		auto pathIt= std::find(m_clientTexturePaths.begin(), m_clientTexturePaths.end(), imagePath);
		if (pathIt == m_clientTexturePaths.end())
		{
			m_clientTexturePaths.push_back(imagePath);
			pathIt= m_clientTexturePaths.end() - 1;
		}
		m_clientTextureIndexPerMesh[meshIndex]= (int)(pathIt - m_clientTexturePaths.begin());
	}

	std::vector<GeometrySourceFile> sourceFiles;
	sourceFiles.push_back(RenderModelResourceUtils::makeSourceFile(m_renderModelFilepath));
	for (const std::filesystem::path& texturePath : m_clientTexturePaths)
	{
		sourceFiles.push_back(RenderModelResourceUtils::makeSourceFile(texturePath));
	}

	m_geometryETag= computeGeometryETag(sourceFiles);
}

std::shared_ptr<const std::vector<uint8_t>> MikanRenderModelResource::getOrBuildGlbPayload()
{
	if (m_glbPayload)
	{
		return m_glbPayload;
	}

	// An image that fails to load drops its meshes' material rather than the whole model
	std::vector<GlbMeshWriter::ImageData> images;
	std::vector<int> glbImageIndexPerTexture(m_clientTexturePaths.size(), -1);
	for (size_t textureIndex= 0; textureIndex < m_clientTexturePaths.size(); ++textureIndex)
	{
		GlbMeshWriter::ImageData image;
		if (RenderModelResourceUtils::loadEncodedImage(m_clientTexturePaths[textureIndex], image))
		{
			glbImageIndexPerTexture[textureIndex]= (int)images.size();
			images.push_back(std::move(image));
		}
		else
		{
			MIKAN_LOG_WARNING("MikanRenderModelResource::getOrBuildGlbPayload")
				<< "Failed to read texture " << m_clientTexturePaths[textureIndex].string();
		}
	}

	std::vector<GlbMeshWriter::MeshData> meshes;
	for (size_t meshIndex= 0; meshIndex < m_triangulatedMeshes.size(); ++meshIndex)
	{
		GlbMeshWriter::MeshData meshData;
		if (!RenderModelResourceUtils::extractMeshData(m_triangulatedMeshes[meshIndex], meshData))
		{
			continue;
		}

		const int textureIndex=
			meshIndex < m_clientTextureIndexPerMesh.size() ? m_clientTextureIndexPerMesh[meshIndex] : -1;
		meshData.imageIndex= textureIndex >= 0 ? glbImageIndexPerTexture[textureIndex] : -1;
		meshes.push_back(std::move(meshData));
	}

	auto payload= std::make_shared<std::vector<uint8_t>>();
	if (!GlbMeshWriter::writeToBytes(meshes, images, *payload))
	{
		MIKAN_LOG_ERROR("MikanRenderModelResource::getOrBuildGlbPayload")
			<< "Failed to build client geometry for " << m_renderModelFilepath.string();
		return nullptr;
	}

	m_glbPayload= payload;
	return m_glbPayload;
}

void MikanRenderModelResource::disposeMeshRenderResources()
{
	for (IMkTriangulatedMeshPtr triMesh : m_triangulatedMeshes)
	{
		triMesh->deleteResources();
	}
	m_triangulatedMeshes.clear();

	for (IMkWireframeMeshPtr wireframeMesh : m_wireframeMeshes)
	{
		wireframeMesh->deleteResources();
	}
	m_wireframeMeshes.clear();
}
