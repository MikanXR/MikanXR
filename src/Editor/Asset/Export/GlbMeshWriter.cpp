#include "GlbMeshWriter.h"
#include "Logger.h"

#include "cgltf.h"
#include "cgltf_write.h"

#include <glm/common.hpp>

#include <cstring>
#include <fstream>
#include <system_error>

namespace GlbMeshWriterUtils
{
constexpr uint32_t k_glbMagic= 0x46546C67; // "glTF"
constexpr uint32_t k_glbVersion= 2;
constexpr uint32_t k_glbJsonChunkType= 0x4E4F534A; // "JSON"
constexpr uint32_t k_glbBinChunkType= 0x004E4942;  // "BIN\0"

size_t alignTo4(size_t offset) { return (offset + 3) & ~(size_t)3; }

// cgltf_write copies strings into the json verbatim, so anything that would break a json string is replaced
std::string makeJsonSafeName(const std::string& name)
{
	std::string safeName= name;
	for (char& c : safeName)
	{
		if (c == '"' || c == '\\' || (unsigned char)c < 0x20)
		{
			c= '_';
		}
	}
	return safeName;
}

// glTF uris are percent-encoded; unreserved characters and path separators pass through
std::string encodeUri(const std::string& text)
{
	static const char* k_hexDigits= "0123456789ABCDEF";

	std::string encoded;
	for (const char c : text)
	{
		const unsigned char byte= (unsigned char)c;
		const bool bUnreserved= (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
								|| (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' || byte == '_'
								|| byte == '~' || byte == '/';
		if (bUnreserved)
		{
			encoded.push_back(c);
		}
		else
		{
			encoded.push_back('%');
			encoded.push_back(k_hexDigits[byte >> 4]);
			encoded.push_back(k_hexDigits[byte & 0x0F]);
		}
	}
	return encoded;
}

// Appends raw bytes to the binary chunk at a 4-byte aligned offset and returns that offset
size_t appendToBuffer(std::vector<uint8_t>& buffer, const void* bytes, size_t byteCount)
{
	const size_t offset= alignTo4(buffer.size());
	buffer.resize(offset + byteCount, 0);
	memcpy(buffer.data() + offset, bytes, byteCount);
	return offset;
}

void appendUint32(std::vector<uint8_t>& bytes, uint32_t value)
{
	const uint8_t littleEndian[4]= {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF),
									(uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 24) & 0xFF)};
	bytes.insert(bytes.end(), littleEndian, littleEndian + 4);
}

// cgltf_write emits the JSON document only, so the GLB container is assembled here: the 12-byte
// header, the JSON chunk padded with spaces, then the BIN chunk padded with zeros
void assembleGlb(const std::string& json, const std::vector<uint8_t>& bin, std::vector<uint8_t>& outBytes)
{
	const size_t jsonChunkSize= alignTo4(json.size());
	const size_t binChunkSize= alignTo4(bin.size());
	const size_t totalSize= 12 + 8 + jsonChunkSize + (bin.empty() ? 0 : 8 + binChunkSize);

	outBytes.clear();
	outBytes.reserve(totalSize);

	appendUint32(outBytes, k_glbMagic);
	appendUint32(outBytes, k_glbVersion);
	appendUint32(outBytes, (uint32_t)totalSize);

	appendUint32(outBytes, (uint32_t)jsonChunkSize);
	appendUint32(outBytes, k_glbJsonChunkType);
	outBytes.insert(outBytes.end(), json.begin(), json.end());
	outBytes.resize(outBytes.size() + (jsonChunkSize - json.size()), 0x20);

	if (!bin.empty())
	{
		appendUint32(outBytes, (uint32_t)binChunkSize);
		appendUint32(outBytes, k_glbBinChunkType);
		outBytes.insert(outBytes.end(), bin.begin(), bin.end());
		outBytes.resize(outBytes.size() + (binChunkSize - bin.size()), 0x00);
	}
}

bool validateMesh(const GlbMeshWriter::MeshData& mesh, size_t imageCount)
{
	if (mesh.vertices.empty() || mesh.indices.empty() || (mesh.indices.size() % 3) != 0)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Mesh '" << mesh.name << "' has no triangle list";
		return false;
	}

	for (const uint32_t index : mesh.indices)
	{
		if (index >= mesh.vertices.size())
		{
			MIKAN_LOG_ERROR("GlbMeshWriter") << "Index " << index << " out of range in mesh '" << mesh.name << "'";
			return false;
		}
	}

	if (mesh.imageIndex >= (int)imageCount)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Image index " << mesh.imageIndex << " out of range in mesh '" << mesh.name
										 << "'";
		return false;
	}

	return true;
}
}; // namespace GlbMeshWriterUtils

bool GlbMeshWriter::writeToBytes(const std::vector<MeshData>& meshes, const std::vector<ImageData>& images,
								 std::vector<uint8_t>& outBytes)
{
	outBytes.clear();

	if (meshes.empty())
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "No meshes to write";
		return false;
	}

	for (const MeshData& mesh : meshes)
	{
		if (!GlbMeshWriterUtils::validateMesh(mesh, images.size()))
		{
			return false;
		}
	}

	for (const ImageData& image : images)
	{
		if (image.uri.empty() && (image.bytes.empty() || image.mimeType.empty()))
		{
			MIKAN_LOG_ERROR("GlbMeshWriter") << "Image has neither a uri nor encoded bytes with a mime type";
			return false;
		}
	}

	// Every cgltf array is sized up front: the document links its parts by pointer, so no array may
	// move once a pointer into it is taken. Each mesh adds up to four accessors and three attributes,
	// and each embedded image one buffer view.
	const size_t meshCount= meshes.size();
	const size_t imageCount= images.size();

	// cgltf_write takes mutable C strings and does not own them, so they live here until the write returns
	std::string versionString= "2.0";
	std::string generatorString= "MikanXR";
	std::string positionName= "POSITION";
	std::string normalName= "NORMAL";
	std::string texCoordName= "TEXCOORD_0";
	std::vector<std::string> meshNames(meshCount);
	std::vector<std::string> imageUris(imageCount);
	std::vector<std::string> imageMimeTypes(imageCount);

	std::vector<uint8_t> binBuffer;

	cgltf_buffer buffer;
	memset(&buffer, 0, sizeof(cgltf_buffer));

	std::vector<cgltf_buffer_view> bufferViews(meshCount * 4 + imageCount);
	std::vector<cgltf_accessor> accessors(meshCount * 4);
	std::vector<cgltf_attribute> attributes(meshCount * 3);
	std::vector<cgltf_primitive> primitives(meshCount);
	std::vector<cgltf_mesh> cgltfMeshes(meshCount);
	std::vector<cgltf_node> nodes(meshCount);
	std::vector<cgltf_node*> sceneNodes(meshCount);
	std::vector<cgltf_image> cgltfImages(imageCount);
	std::vector<cgltf_texture> textures(imageCount);
	std::vector<cgltf_material> materials(imageCount);
	memset(bufferViews.data(), 0, bufferViews.size() * sizeof(cgltf_buffer_view));
	memset(accessors.data(), 0, accessors.size() * sizeof(cgltf_accessor));
	memset(attributes.data(), 0, attributes.size() * sizeof(cgltf_attribute));
	memset(primitives.data(), 0, primitives.size() * sizeof(cgltf_primitive));
	memset(cgltfMeshes.data(), 0, cgltfMeshes.size() * sizeof(cgltf_mesh));
	memset(nodes.data(), 0, nodes.size() * sizeof(cgltf_node));
	if (imageCount > 0)
	{
		memset(cgltfImages.data(), 0, cgltfImages.size() * sizeof(cgltf_image));
		memset(textures.data(), 0, textures.size() * sizeof(cgltf_texture));
		memset(materials.data(), 0, materials.size() * sizeof(cgltf_material));
	}

	size_t bufferViewCount= 0;
	size_t accessorCount= 0;
	size_t attributeCount= 0;

	auto addBufferView= [&](const void* bytes, size_t byteSize, cgltf_buffer_view_type viewType) -> cgltf_buffer_view*
	{
		cgltf_buffer_view& bufferView= bufferViews[bufferViewCount++];
		bufferView.buffer= &buffer;
		bufferView.offset= GlbMeshWriterUtils::appendToBuffer(binBuffer, bytes, byteSize);
		bufferView.size= byteSize;
		bufferView.type= viewType;
		return &bufferView;
	};

	auto addAccessor= [&](const void* bytes, size_t byteSize, cgltf_buffer_view_type viewType, cgltf_type type,
						  cgltf_component_type componentType, size_t count) -> cgltf_accessor*
	{
		cgltf_accessor& accessor= accessors[accessorCount++];
		accessor.buffer_view= addBufferView(bytes, byteSize, viewType);
		accessor.component_type= componentType;
		accessor.type= type;
		accessor.count= count;
		accessor.stride= cgltf_calc_size(type, componentType);
		return &accessor;
	};

	auto addAttribute= [&](std::string& name, cgltf_attribute_type type, cgltf_accessor* accessor)
	{
		cgltf_attribute& attribute= attributes[attributeCount++];
		attribute.name= &name[0];
		attribute.type= type;
		attribute.index= 0;
		attribute.data= accessor;
	};

	for (size_t imageIndex= 0; imageIndex < imageCount; ++imageIndex)
	{
		const ImageData& imageData= images[imageIndex];
		cgltf_image& image= cgltfImages[imageIndex];

		if (!imageData.uri.empty())
		{
			imageUris[imageIndex]= GlbMeshWriterUtils::encodeUri(imageData.uri);
			image.uri= &imageUris[imageIndex][0];
		}
		else
		{
			imageMimeTypes[imageIndex]= GlbMeshWriterUtils::makeJsonSafeName(imageData.mimeType);
			image.mime_type= &imageMimeTypes[imageIndex][0];
			image.buffer_view=
				addBufferView(imageData.bytes.data(), imageData.bytes.size(), cgltf_buffer_view_type_invalid);
		}

		textures[imageIndex].image= &image;

		// The textures are captured colour, which already carries the scene's lighting
		cgltf_material& material= materials[imageIndex];
		material.has_pbr_metallic_roughness= 1;
		material.pbr_metallic_roughness.base_color_texture.texture= &textures[imageIndex];
		material.pbr_metallic_roughness.base_color_texture.texcoord= 0;
		material.pbr_metallic_roughness.base_color_texture.scale= 1.f;
		for (int channel= 0; channel < 4; ++channel)
		{
			material.pbr_metallic_roughness.base_color_factor[channel]= 1.f;
		}
		material.pbr_metallic_roughness.metallic_factor= 0.f;
		material.pbr_metallic_roughness.roughness_factor= 1.f;
		material.alpha_mode= cgltf_alpha_mode_opaque;
		material.unlit= 1;
	}

	for (size_t meshIndex= 0; meshIndex < meshCount; ++meshIndex)
	{
		const MeshData& meshData= meshes[meshIndex];
		const bool bHasNormals= meshData.normals.size() == meshData.vertices.size();
		const bool bHasTexCoords= meshData.texCoords.size() == meshData.vertices.size();

		cgltf_attribute* firstAttribute= &attributes[attributeCount];

		cgltf_accessor* positionAccessor= addAccessor(
			meshData.vertices.data(), meshData.vertices.size() * sizeof(glm::vec3), cgltf_buffer_view_type_vertices,
			cgltf_type_vec3, cgltf_component_type_r_32f, meshData.vertices.size());
		glm::vec3 positionMin= meshData.vertices[0];
		glm::vec3 positionMax= meshData.vertices[0];
		for (const glm::vec3& vertex : meshData.vertices)
		{
			positionMin= glm::min(positionMin, vertex);
			positionMax= glm::max(positionMax, vertex);
		}
		positionAccessor->has_min= 1;
		positionAccessor->has_max= 1;
		for (int axis= 0; axis < 3; ++axis)
		{
			positionAccessor->min[axis]= positionMin[axis];
			positionAccessor->max[axis]= positionMax[axis];
		}
		addAttribute(positionName, cgltf_attribute_type_position, positionAccessor);

		if (bHasNormals)
		{
			cgltf_accessor* normalAccessor= addAccessor(
				meshData.normals.data(), meshData.normals.size() * sizeof(glm::vec3), cgltf_buffer_view_type_vertices,
				cgltf_type_vec3, cgltf_component_type_r_32f, meshData.normals.size());
			addAttribute(normalName, cgltf_attribute_type_normal, normalAccessor);
		}

		if (bHasTexCoords)
		{
			// The editor's texcoords are V-up, glTF stores V-down
			std::vector<glm::vec2> flippedTexCoords(meshData.texCoords.size());
			for (size_t index= 0; index < meshData.texCoords.size(); ++index)
			{
				flippedTexCoords[index]= glm::vec2(meshData.texCoords[index].x, 1.f - meshData.texCoords[index].y);
			}
			cgltf_accessor* texCoordAccessor= addAccessor(
				flippedTexCoords.data(), flippedTexCoords.size() * sizeof(glm::vec2), cgltf_buffer_view_type_vertices,
				cgltf_type_vec2, cgltf_component_type_r_32f, flippedTexCoords.size());
			addAttribute(texCoordName, cgltf_attribute_type_texcoord, texCoordAccessor);
		}

		cgltf_accessor* indexAccessor= addAccessor(meshData.indices.data(), meshData.indices.size() * sizeof(uint32_t),
												   cgltf_buffer_view_type_indices, cgltf_type_scalar,
												   cgltf_component_type_r_32u, meshData.indices.size());

		cgltf_primitive& primitive= primitives[meshIndex];
		primitive.type= cgltf_primitive_type_triangles;
		primitive.indices= indexAccessor;
		primitive.material= meshData.imageIndex >= 0 ? &materials[meshData.imageIndex] : nullptr;
		primitive.attributes= firstAttribute;
		primitive.attributes_count= &attributes[0] + attributeCount - firstAttribute;

		meshNames[meshIndex]= GlbMeshWriterUtils::makeJsonSafeName(meshData.name);

		cgltf_mesh& mesh= cgltfMeshes[meshIndex];
		mesh.name= &meshNames[meshIndex][0];
		mesh.primitives= &primitive;
		mesh.primitives_count= 1;

		cgltf_node& node= nodes[meshIndex];
		node.name= &meshNames[meshIndex][0];
		node.mesh= &mesh;
		sceneNodes[meshIndex]= &node;
	}

	buffer.size= binBuffer.size();

	cgltf_scene scene;
	memset(&scene, 0, sizeof(cgltf_scene));
	scene.nodes= sceneNodes.data();
	scene.nodes_count= meshCount;

	cgltf_data data;
	memset(&data, 0, sizeof(cgltf_data));
	data.file_type= cgltf_file_type_glb;
	data.asset.version= &versionString[0];
	data.asset.generator= &generatorString[0];
	data.meshes= cgltfMeshes.data();
	data.meshes_count= meshCount;
	data.accessors= accessors.data();
	data.accessors_count= accessorCount;
	data.buffer_views= bufferViews.data();
	data.buffer_views_count= bufferViewCount;
	data.buffers= &buffer;
	data.buffers_count= 1;
	if (imageCount > 0)
	{
		data.materials= materials.data();
		data.materials_count= imageCount;
		data.images= cgltfImages.data();
		data.images_count= imageCount;
		data.textures= textures.data();
		data.textures_count= imageCount;
	}
	data.nodes= nodes.data();
	data.nodes_count= meshCount;
	data.scenes= &scene;
	data.scenes_count= 1;
	data.scene= &scene;

	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));
	options.type= cgltf_file_type_glb;

	// The first call sizes the JSON, including its null terminator, and the second writes it
	const cgltf_size jsonCapacity= cgltf_write(&options, nullptr, 0, &data);
	std::string json(jsonCapacity, '\0');
	const cgltf_size jsonWritten= cgltf_write(&options, &json[0], jsonCapacity, &data);
	if (jsonWritten == 0 || jsonWritten != jsonCapacity)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Failed to write the glTF JSON";
		return false;
	}
	json.resize(jsonWritten - 1);

	GlbMeshWriterUtils::assembleGlb(json, binBuffer, outBytes);
	return true;
}

bool GlbMeshWriter::write(const std::filesystem::path& path, const std::string& meshName,
						  const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals,
						  const std::vector<glm::vec2>& texCoords, const std::vector<uint32_t>& indices,
						  const std::string& textureFileName)
{
	const std::string pathString= path.string();

	std::error_code errorCode;
	const std::filesystem::path parentDirectory= path.parent_path();
	if (!parentDirectory.empty() && !std::filesystem::is_directory(parentDirectory, errorCode))
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Directory does not exist for " << pathString;
		return false;
	}

	std::vector<MeshData> meshes(1);
	meshes[0].name= meshName;
	meshes[0].vertices= vertices;
	meshes[0].normals= normals;
	meshes[0].texCoords= texCoords;
	meshes[0].indices= indices;

	std::vector<ImageData> images;
	if (!textureFileName.empty())
	{
		ImageData image;
		image.uri= textureFileName;
		images.push_back(image);
		meshes[0].imageIndex= 0;
	}

	std::vector<uint8_t> glbBytes;
	if (!writeToBytes(meshes, images, glbBytes))
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Failed to build " << pathString;
		return false;
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Failed to open " << pathString;
		return false;
	}
	file.write((const char*)glbBytes.data(), (std::streamsize)glbBytes.size());
	if (!file)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Failed to write " << pathString;
		return false;
	}

	return true;
}
