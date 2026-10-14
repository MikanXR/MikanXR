#include "GltfModelImporter.h"
#include "IMkGraphicsContext.h"
#include "IMkShader.h"
#include "IMkTexture.h"
#include "IMkTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "IMkVertexDefinition.h"
#include "IMkWireframeMesh.h"
#include "Logger.h"
#include "MikanModelResourceManager.h"
#include "MikanRenderModelResource.h"
#include "MikanShaderCache.h"
#include "MikanTextureCache.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "ModelImportUtils.h"

#include "cgltf.h"
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace GltfImportUtils
{
// Frees the parsed glTF document on every exit path
class ScopedGltfData
{
public:
	ScopedGltfData()= default;
	~ScopedGltfData() { cgltf_free(m_data); }

	ScopedGltfData(const ScopedGltfData&)= delete;
	ScopedGltfData& operator=(const ScopedGltfData&)= delete;

	cgltf_data** getOutPtr() { return &m_data; }
	cgltf_data* get() const { return m_data; }

private:
	cgltf_data* m_data= nullptr;
};

const char* getResultString(cgltf_result result)
{
	switch (result)
	{
	case cgltf_result_success:
		return "success";
	case cgltf_result_data_too_short:
		return "data too short";
	case cgltf_result_unknown_format:
		return "unknown format";
	case cgltf_result_invalid_json:
		return "invalid json";
	case cgltf_result_invalid_gltf:
		return "invalid gltf";
	case cgltf_result_invalid_options:
		return "invalid options";
	case cgltf_result_file_not_found:
		return "file not found";
	case cgltf_result_io_error:
		return "io error";
	case cgltf_result_out_of_memory:
		return "out of memory";
	case cgltf_result_legacy_gltf:
		return "legacy gltf (1.0 is not supported)";
	default:
		return "unknown error";
	}
}

struct ImportContext
{
	IMkGraphicsContext* graphicsContext= nullptr;
	IMkTextureCache* textureCache= nullptr;
	MkMaterialConstPtr material;
	const IMkVertexAttribute* positionAttribute= nullptr;
	const IMkVertexAttribute* normalAttribute= nullptr;
	const IMkVertexAttribute* texCoordAttribute= nullptr;
	size_t vertexSize= 0;

	const cgltf_data* data= nullptr;
	std::filesystem::path modelDirectory;
	std::string modelPathString;
	MikanRenderModelResourcePtr modelResource;

	// One texture per glTF image, decoded or loaded the first time a material asks for it
	std::vector<IMkTexturePtr> imageTextures;
	std::vector<bool> imageLoadAttempted;

	std::vector<bool> nodeVisited;
	size_t skippedPrimitiveCount= 0;
};

// Decodes an encoded image (png, jpg, ...) held in memory into an RGBA texture the material owns
IMkTexturePtr createTextureFromEncodedImage(const uint8_t* encodedBytes, size_t encodedSize,
											const std::string& textureName)
{
	int width= 0;
	int height= 0;
	int sourceChannels= 0;
	stbi_uc* pixels= stbi_load_from_memory(encodedBytes, (int)encodedSize, &width, &height, &sourceChannels, 4);
	if (pixels == nullptr)
	{
		MIKAN_LOG_WARNING("GltfModelImporter")
			<< "Failed to decode embedded image " << textureName << ": " << stbi_failure_reason();
		return IMkTexturePtr();
	}

	IMkTexturePtr texture;
	if (width > 0 && height > 0 && width <= UINT16_MAX && height <= UINT16_MAX)
	{
		texture= CreateMkTexture((uint16_t)width, (uint16_t)height, pixels, MK_RGBA, MK_RGBA);
		texture->setName(textureName);
		// glTF samplers repeat by default
		texture->setTextureWrapMode(IMkTexture::TextureWrapMode::Repeat);
		if (texture->createTexture())
		{
			// The pixels are uploaded and freed below, so the texture must not keep pointing at them
			texture->setTextureMapData(nullptr);
		}
		else
		{
			MIKAN_LOG_WARNING("GltfModelImporter") << "Failed to create texture for embedded image " << textureName;
			texture= nullptr;
		}
	}
	else
	{
		MIKAN_LOG_WARNING("GltfModelImporter")
			<< "Embedded image " << textureName << " has unsupported size " << width << "x" << height;
	}

	stbi_image_free(pixels);

	return texture;
}

// Decodes a "data:<mime>;base64,<payload>" uri into raw bytes
bool decodeBase64DataUri(const char* uri, std::vector<uint8_t>& outBytes)
{
	const char* comma= strchr(uri, ',');
	if (comma == nullptr || comma - uri < 7 || strncmp(comma - 7, ";base64", 7) != 0)
	{
		return false;
	}

	const char* base64= comma + 1;
	size_t payloadLength= 0;
	while (base64[payloadLength] != '\0' && base64[payloadLength] != '=')
	{
		payloadLength++;
	}

	const cgltf_size decodedSize= (payloadLength * 6) / 8;
	if (decodedSize == 0)
	{
		return false;
	}

	// Zeroed options select cgltf's default malloc based allocator
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));

	void* decoded= nullptr;
	if (cgltf_load_buffer_base64(&options, decodedSize, base64, &decoded) != cgltf_result_success)
	{
		return false;
	}

	const uint8_t* decodedBytes= (const uint8_t*)decoded;
	outBytes.assign(decodedBytes, decodedBytes + decodedSize);
	free(decoded);

	return true;
}

IMkTexturePtr loadImageTexture(ImportContext& context, const cgltf_image* image)
{
	const size_t imageIndex= cgltf_image_index(context.data, image);
	if (imageIndex >= context.imageTextures.size())
	{
		return IMkTexturePtr();
	}

	if (context.imageLoadAttempted[imageIndex])
	{
		return context.imageTextures[imageIndex];
	}
	context.imageLoadAttempted[imageIndex]= true;

	const std::string textureName= context.modelPathString + "#image" + std::to_string(imageIndex);

	IMkTexturePtr texture;
	if (image->buffer_view != nullptr)
	{
		// Image embedded in a buffer (the usual case for .glb)
		const uint8_t* encodedBytes= cgltf_buffer_view_data(image->buffer_view);
		if (encodedBytes != nullptr && image->buffer_view->size > 0)
		{
			texture= createTextureFromEncodedImage(encodedBytes, image->buffer_view->size, textureName);
		}
	}
	else if (image->uri != nullptr && image->uri[0] != '\0')
	{
		if (strncmp(image->uri, "data:", 5) == 0)
		{
			// Image embedded in the json as a base64 data uri
			std::vector<uint8_t> encodedBytes;
			if (decodeBase64DataUri(image->uri, encodedBytes))
			{
				texture= createTextureFromEncodedImage(encodedBytes.data(), encodedBytes.size(), textureName);
			}
			else
			{
				MIKAN_LOG_WARNING("GltfModelImporter") << "Unsupported image data uri in " << context.modelPathString;
			}
		}
		else
		{
			// External image file, relative to the model file. Uris are percent-encoded utf-8.
			std::string decodedUri= image->uri;
			cgltf_decode_uri(&decodedUri[0]);
			decodedUri.resize(strlen(decodedUri.c_str()));

			const std::u8string utf8Uri(decodedUri.begin(), decodedUri.end());
			const std::filesystem::path texturePath= context.modelDirectory / std::filesystem::path(utf8Uri);
			texture= context.textureCache->loadTexturePath(texturePath);
			if (texture == nullptr)
			{
				MIKAN_LOG_WARNING("GltfModelImporter") << "Failed to load texture " << texturePath.string();
			}
		}
	}

	context.imageTextures[imageIndex]= texture;

	return texture;
}

void bindTextureBySemantic(ImportContext& context, MkMaterialInstancePtr materialInstance, eUniformSemantic semantic,
						   IMkTexturePtr texture)
{
	IMkShaderPtr program= materialInstance->getMaterial()->getProgram();

	// Only bind when the shader has a texture uniform with the given semantic
	std::string uniformName;
	if (program->getFirstUniformNameOfSemantic(semantic, uniformName)
		&& getUniformSemanticDataType(semantic) == eUniformDataType::datatype_texture)
	{
		if (texture == nullptr)
		{
			texture= context.textureCache->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGB);
		}

		if (texture != nullptr)
		{
			materialInstance->setTextureByUniformName(uniformName, texture);
		}
	}
}

MkMaterialInstancePtr createPrimitiveMaterialInstance(ImportContext& context, const cgltf_material* gltfMaterial)
{
	MkMaterialInstancePtr materialInstance= createMkMaterialInstance(context.material);

	// A primitive without a material, or a material without pbrMetallicRoughness, takes the glTF defaults
	glm::vec4 baseColor(1.f);
	IMkTexturePtr baseColorTexture;
	if (gltfMaterial != nullptr && gltfMaterial->has_pbr_metallic_roughness)
	{
		const cgltf_pbr_metallic_roughness& pbr= gltfMaterial->pbr_metallic_roughness;

		baseColor= glm::make_vec4(pbr.base_color_factor);
		if (pbr.base_color_texture.texture != nullptr && pbr.base_color_texture.texture->image != nullptr)
		{
			baseColorTexture= loadImageTexture(context, pbr.base_color_texture.texture->image);
		}
	}

	// glTF only carries the diffuse term the lit shader uses, so the remaining lighting terms
	// take the same defaults as an OBJ file without a material library
	materialInstance->setVec3BySemantic(eUniformSemantic::ambientColorRGB, glm::vec3(1.f));
	materialInstance->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA, baseColor);
	materialInstance->setVec3BySemantic(eUniformSemantic::specularColorRGB, glm::vec3(0.5f));
	materialInstance->setFloatBySemantic(eUniformSemantic::specularHighlights, 250.f);
	materialInstance->setFloatBySemantic(eUniformSemantic::opticalDensity, 1.45f);
	materialInstance->setFloatBySemantic(eUniformSemantic::dissolve, baseColor.a);
	materialInstance->setFloatBySemantic(eUniformSemantic::ambientStrength, 0.3f);

	bindTextureBySemantic(context, materialInstance, eUniformSemantic::diffuseTexture, baseColorTexture);
	bindTextureBySemantic(context, materialInstance, eUniformSemantic::ambientTexture, IMkTexturePtr());
	bindTextureBySemantic(context, materialInstance, eUniformSemantic::specularTexture, IMkTexturePtr());
	bindTextureBySemantic(context, materialInstance, eUniformSemantic::specularHightlightTexture, IMkTexturePtr());
	bindTextureBySemantic(context, materialInstance, eUniformSemantic::alphaTexture, IMkTexturePtr());
	bindTextureBySemantic(context, materialInstance, eUniformSemantic::bumpTexture, IMkTexturePtr());

	return materialInstance;
}

bool readVec3Accessor(const cgltf_accessor* accessor, std::vector<glm::vec3>& outValues)
{
	outValues.resize(accessor->count);
	for (cgltf_size index= 0; index < accessor->count; ++index)
	{
		if (!cgltf_accessor_read_float(accessor, index, glm::value_ptr(outValues[index]), 3))
		{
			return false;
		}
	}

	return true;
}

bool readVec2Accessor(const cgltf_accessor* accessor, std::vector<glm::vec2>& outValues)
{
	outValues.resize(accessor->count);
	for (cgltf_size index= 0; index < accessor->count; ++index)
	{
		if (!cgltf_accessor_read_float(accessor, index, glm::value_ptr(outValues[index]), 2))
		{
			return false;
		}
	}

	return true;
}

void importPrimitive(ImportContext& context, const cgltf_primitive& primitive, const glm::mat4& worldTransform,
					 const std::string& meshName)
{
	if (primitive.type != cgltf_primitive_type_triangles || primitive.has_draco_mesh_compression)
	{
		context.skippedPrimitiveCount++;
		return;
	}

	const cgltf_accessor* positionAccessor= cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
	const cgltf_accessor* normalAccessor= cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
	const cgltf_accessor* texCoordAccessor= cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
	if (positionAccessor == nullptr || positionAccessor->count == 0)
	{
		MIKAN_LOG_WARNING("GltfModelImporter") << "Skipping primitive " << meshName << " without positions";
		return;
	}

	// Read the source attributes
	std::vector<glm::vec3> positions;
	if (!readVec3Accessor(positionAccessor, positions))
	{
		MIKAN_LOG_ERROR("GltfModelImporter") << "Failed to read positions of " << meshName;
		return;
	}
	const size_t sourceVertexCount= positions.size();

	std::vector<glm::vec3> normals;
	bool bHasNormals= false;
	if (normalAccessor != nullptr && normalAccessor->count == sourceVertexCount)
	{
		bHasNormals= readVec3Accessor(normalAccessor, normals);
	}

	std::vector<glm::vec2> texCoords;
	bool bHasTexCoords= false;
	if (texCoordAccessor != nullptr && texCoordAccessor->count == sourceVertexCount)
	{
		bHasTexCoords= readVec2Accessor(texCoordAccessor, texCoords);
	}

	// Read the triangle list, or synthesize one for non-indexed geometry
	std::vector<uint32_t> indices;
	if (primitive.indices != nullptr)
	{
		indices.resize(primitive.indices->count);
		for (cgltf_size index= 0; index < primitive.indices->count; ++index)
		{
			indices[index]= (uint32_t)cgltf_accessor_read_index(primitive.indices, index);
		}
	}
	else
	{
		indices.resize(sourceVertexCount);
		for (size_t index= 0; index < sourceVertexCount; ++index)
		{
			indices[index]= (uint32_t)index;
		}
	}
	indices.resize(indices.size() - (indices.size() % 3));

	for (const uint32_t index : indices)
	{
		if (index >= sourceVertexCount)
		{
			MIKAN_LOG_ERROR("GltfModelImporter") << "Primitive " << meshName << " has an out of range index";
			return;
		}
	}

	if (indices.empty())
	{
		return;
	}

	// Bake the node's world transform into the vertices.
	// Normals take the inverse-transpose so non-uniform scale keeps them perpendicular to the surface.
	const glm::mat3 normalTransform= glm::transpose(glm::inverse(glm::mat3(worldTransform)));
	for (glm::vec3& position : positions)
	{
		position= glm::vec3(worldTransform * glm::vec4(position, 1.f));
	}
	if (bHasNormals)
	{
		for (glm::vec3& normal : normals)
		{
			const glm::vec3 transformedNormal= normalTransform * normal;
			const float normalLength= glm::length(transformedNormal);
			normal= normalLength > 0.f ? transformedNormal / normalLength : glm::vec3(0.f);
		}
	}

	// A mirroring transform turns counter-clockwise triangles clockwise, so restore the front-face winding
	if (glm::determinant(glm::mat3(worldTransform)) < 0.f)
	{
		for (size_t triIndex= 0; triIndex + 2 < indices.size(); triIndex+= 3)
		{
			std::swap(indices[triIndex + 1], indices[triIndex + 2]);
		}
	}

	// glTF requires flat normals when a primitive has none, which needs a vertex per triangle corner
	if (!bHasNormals && context.normalAttribute != nullptr)
	{
		std::vector<glm::vec3> flatPositions;
		std::vector<glm::vec3> flatNormals;
		std::vector<glm::vec2> flatTexCoords;
		flatPositions.reserve(indices.size());
		flatNormals.reserve(indices.size());
		if (bHasTexCoords)
		{
			flatTexCoords.reserve(indices.size());
		}

		for (size_t triIndex= 0; triIndex + 2 < indices.size(); triIndex+= 3)
		{
			const glm::vec3& p0= positions[indices[triIndex]];
			const glm::vec3& p1= positions[indices[triIndex + 1]];
			const glm::vec3& p2= positions[indices[triIndex + 2]];
			const glm::vec3 faceCross= glm::cross(p1 - p0, p2 - p0);
			const float crossLength= glm::length(faceCross);
			const glm::vec3 faceNormal= crossLength > 0.f ? faceCross / crossLength : glm::vec3(0.f);

			for (size_t corner= 0; corner < 3; ++corner)
			{
				const uint32_t sourceIndex= indices[triIndex + corner];
				flatPositions.push_back(positions[sourceIndex]);
				flatNormals.push_back(faceNormal);
				if (bHasTexCoords)
				{
					flatTexCoords.push_back(texCoords[sourceIndex]);
				}
			}
		}

		positions= std::move(flatPositions);
		normals= std::move(flatNormals);
		texCoords= std::move(flatTexCoords);

		for (size_t index= 0; index < indices.size(); ++index)
		{
			indices[index]= (uint32_t)index;
		}
	}

	// Pack the vertices into the material's vertex layout
	const size_t vertexCount= positions.size();
	std::vector<uint8_t> vertexData(vertexCount * context.vertexSize, 0);
	for (size_t vertexIndex= 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		uint8_t* vertexWritePtr= &vertexData[vertexIndex * context.vertexSize];

		memcpy(vertexWritePtr + context.positionAttribute->getOffset(), &positions[vertexIndex], sizeof(glm::vec3));

		if (context.normalAttribute != nullptr)
		{
			memcpy(vertexWritePtr + context.normalAttribute->getOffset(), &normals[vertexIndex], sizeof(glm::vec3));
		}

		if (context.texCoordAttribute != nullptr && bHasTexCoords)
		{
			// glTF puts v=0 at the top of the image while the editor's meshes carry V-up texcoords
			// (the OBJ convention), so this is the one place a glTF texcoord is flipped on import
			const glm::vec2 texCoord(texCoords[vertexIndex].x, 1.f - texCoords[vertexIndex].y);
			memcpy(vertexWritePtr + context.texCoordAttribute->getOffset(), &texCoord, sizeof(glm::vec2));
		}
	}

	ModelImportUtils::TriMeshSourceData sourceData;
	sourceData.meshName= meshName;
	sourceData.materialInstance= createPrimitiveMaterialInstance(context, primitive.material);
	sourceData.positionAttribute= context.positionAttribute;
	sourceData.vertexData= vertexData.data();
	sourceData.vertexSize= context.vertexSize;
	sourceData.vertexCount= (uint32_t)vertexCount;
	sourceData.indexData= indices.data();
	sourceData.indexCount= (uint32_t)indices.size();

	IMkTriangulatedMeshPtr triMesh=
		ModelImportUtils::createTriangulatedMeshResource(context.graphicsContext, sourceData);
	IMkWireframeMeshPtr wireMesh= ModelImportUtils::createWireframeMeshResource(context.graphicsContext, sourceData);

	context.modelResource->addTriangulatedMesh(triMesh);
	context.modelResource->addWireframeMesh(wireMesh);
}

void importNode(ImportContext& context, const cgltf_node* node)
{
	// Guard against a node reached twice through a malformed hierarchy
	const size_t nodeIndex= cgltf_node_index(context.data, node);
	if (nodeIndex >= context.nodeVisited.size() || context.nodeVisited[nodeIndex])
	{
		return;
	}
	context.nodeVisited[nodeIndex]= true;

	if (node->mesh != nullptr)
	{
		// cgltf writes the world transform column-major, the same layout glm uses
		cgltf_float worldMatrix[16];
		cgltf_node_transform_world(node, worldMatrix);
		const glm::mat4 worldTransform= glm::make_mat4(worldMatrix);

		const cgltf_mesh* mesh= node->mesh;
		std::string baseName;
		if (node->name != nullptr && node->name[0] != '\0')
		{
			baseName= node->name;
		}
		else if (mesh->name != nullptr && mesh->name[0] != '\0')
		{
			baseName= mesh->name;
		}
		else
		{
			baseName= "mesh" + std::to_string(cgltf_mesh_index(context.data, mesh));
		}

		for (cgltf_size primitiveIndex= 0; primitiveIndex < mesh->primitives_count; ++primitiveIndex)
		{
			const std::string meshName= baseName + "_" + std::to_string(primitiveIndex);

			importPrimitive(context, mesh->primitives[primitiveIndex], worldTransform, meshName);
		}
	}

	for (cgltf_size childIndex= 0; childIndex < node->children_count; ++childIndex)
	{
		importNode(context, node->children[childIndex]);
	}
}
}; // namespace GltfImportUtils

MikanRenderModelResourcePtr GltfModelImporter::importModelFromFile(const std::filesystem::path& modelPath,
																   MkMaterialConstPtr overrideMaterial)
{
	if (modelPath.empty())
		return MikanRenderModelResourcePtr();

	IMkGraphicsContext* graphicsContext= m_ownerManager->getGraphicsContext();
	IMkShaderCache* shaderCache= graphicsContext->getShaderCache();

	MkMaterialConstPtr triMeshMaterial= overrideMaterial;
	if (!triMeshMaterial)
	{
		triMeshMaterial= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PNT_TEXTURED_LIT_COLORED);
	}

	if (!triMeshMaterial)
		return MikanRenderModelResourcePtr();

	const std::string modelPathString= modelPath.string();

	// The material's vertex layout decides which attributes get packed
	IMkVertexDefinitionConstPtr vertexDefinition= triMeshMaterial->getProgram()->getVertexDefinition();

	GltfImportUtils::ImportContext context;
	context.graphicsContext= graphicsContext;
	context.textureCache= graphicsContext->getTextureCache();
	context.material= triMeshMaterial;
	context.positionAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::position);
	context.normalAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::normal);
	context.texCoordAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::texCoord);
	context.vertexSize= vertexDefinition->getVertexSize();

	if (context.positionAttribute == nullptr
		|| context.positionAttribute->getDataType() != eVertexDataType::datatype_vec3)
	{
		MIKAN_LOG_ERROR("GltfModelImporter") << "Material " << triMeshMaterial->getName()
											 << " has no vec3 position attribute, can't import " << modelPathString;
		return MikanRenderModelResourcePtr();
	}
	if (context.normalAttribute != nullptr && context.normalAttribute->getDataType() != eVertexDataType::datatype_vec3)
	{
		context.normalAttribute= nullptr;
	}
	if (context.texCoordAttribute != nullptr
		&& context.texCoordAttribute->getDataType() != eVertexDataType::datatype_vec2)
	{
		context.texCoordAttribute= nullptr;
	}

	// Parse the document, then load its buffers relative to the model path so external .bin files
	// and data uris resolve
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));

	GltfImportUtils::ScopedGltfData gltfData;
	cgltf_result result= cgltf_parse_file(&options, modelPathString.c_str(), gltfData.getOutPtr());
	if (result != cgltf_result_success)
	{
		MIKAN_LOG_ERROR("GltfModelImporter")
			<< "Failed to parse " << modelPathString << ": " << GltfImportUtils::getResultString(result);
		return MikanRenderModelResourcePtr();
	}

	cgltf_data* data= gltfData.get();
	result= cgltf_load_buffers(&options, data, modelPathString.c_str());
	if (result != cgltf_result_success)
	{
		MIKAN_LOG_ERROR("GltfModelImporter")
			<< "Failed to load buffers of " << modelPathString << ": " << GltfImportUtils::getResultString(result);
		return MikanRenderModelResourcePtr();
	}

	result= cgltf_validate(data);
	if (result != cgltf_result_success)
	{
		MIKAN_LOG_ERROR("GltfModelImporter")
			<< "Failed to validate " << modelPathString << ": " << GltfImportUtils::getResultString(result);
		return MikanRenderModelResourcePtr();
	}

	MikanRenderModelResourcePtr modelResource= std::make_shared<MikanRenderModelResource>(graphicsContext);
	modelResource->setName(modelPath.stem().string());
	modelResource->setModelFilePath(modelPath);

	context.data= data;
	context.modelDirectory= modelPath.parent_path();
	context.modelPathString= modelPathString;
	context.modelResource= modelResource;
	context.imageTextures.resize(data->images_count);
	context.imageLoadAttempted.resize(data->images_count, false);
	context.nodeVisited.resize(data->nodes_count, false);

	// Walk the default scene, else the first scene, else every root node
	const cgltf_scene* scene= data->scene != nullptr ? data->scene : (data->scenes_count > 0 ? data->scenes : nullptr);
	if (scene != nullptr)
	{
		for (cgltf_size nodeIndex= 0; nodeIndex < scene->nodes_count; ++nodeIndex)
		{
			GltfImportUtils::importNode(context, scene->nodes[nodeIndex]);
		}
	}
	else
	{
		for (cgltf_size nodeIndex= 0; nodeIndex < data->nodes_count; ++nodeIndex)
		{
			if (data->nodes[nodeIndex].parent == nullptr)
			{
				GltfImportUtils::importNode(context, &data->nodes[nodeIndex]);
			}
		}
	}

	if (context.skippedPrimitiveCount > 0)
	{
		MIKAN_LOG_WARNING("GltfModelImporter")
			<< "Omitted " << context.skippedPrimitiveCount << " primitive(s) while importing " << modelPathString
			<< ". Only uncompressed triangle lists are supported.";
	}

	if (modelResource->getTriangulatedMeshCount() == 0)
	{
		MIKAN_LOG_ERROR("GltfModelImporter") << "No triangle geometry imported from " << modelPathString;
		return MikanRenderModelResourcePtr();
	}

	return modelResource;
}
