#include "GlbMeshWriter.h"
#include "Logger.h"

#include "cgltf.h"
#include "cgltf_write.h"

#include <glm/common.hpp>

#include <cstring>
#include <system_error>

namespace GlbMeshWriterUtils
{
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
}; // namespace GlbMeshWriterUtils

bool GlbMeshWriter::write(const std::filesystem::path& path, const std::string& meshName,
						  const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals,
						  const std::vector<glm::vec2>& texCoords, const std::vector<uint32_t>& indices,
						  const std::string& textureFileName)
{
	const std::string pathString= path.string();

	if (vertices.empty() || indices.empty() || (indices.size() % 3) != 0)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "No triangle list to write to " << pathString;
		return false;
	}

	for (const uint32_t index : indices)
	{
		if (index >= vertices.size())
		{
			MIKAN_LOG_ERROR("GlbMeshWriter") << "Index " << index << " out of range writing " << pathString;
			return false;
		}
	}

	std::error_code errorCode;
	const std::filesystem::path parentDirectory= path.parent_path();
	if (!parentDirectory.empty() && !std::filesystem::is_directory(parentDirectory, errorCode))
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Directory does not exist for " << pathString;
		return false;
	}

	const bool bHasNormals= normals.size() == vertices.size();
	const bool bHasTexCoords= texCoords.size() == vertices.size();
	const bool bHasTexture= !textureFileName.empty();

	// Pack the binary chunk: positions, normals, texcoords, then indices, each 4-byte aligned
	std::vector<uint8_t> binBuffer;

	const size_t positionOffset=
		GlbMeshWriterUtils::appendToBuffer(binBuffer, vertices.data(), vertices.size() * sizeof(glm::vec3));
	const size_t positionSize= vertices.size() * sizeof(glm::vec3);

	size_t normalOffset= 0;
	const size_t normalSize= normals.size() * sizeof(glm::vec3);
	if (bHasNormals)
	{
		normalOffset= GlbMeshWriterUtils::appendToBuffer(binBuffer, normals.data(), normalSize);
	}

	size_t texCoordOffset= 0;
	const size_t texCoordSize= texCoords.size() * sizeof(glm::vec2);
	if (bHasTexCoords)
	{
		// The editor's texcoords are V-up, glTF stores V-down
		std::vector<glm::vec2> flippedTexCoords(texCoords.size());
		for (size_t index= 0; index < texCoords.size(); ++index)
		{
			flippedTexCoords[index]= glm::vec2(texCoords[index].x, 1.f - texCoords[index].y);
		}
		texCoordOffset= GlbMeshWriterUtils::appendToBuffer(binBuffer, flippedTexCoords.data(), texCoordSize);
	}

	const size_t indexSize= indices.size() * sizeof(uint32_t);
	const size_t indexOffset= GlbMeshWriterUtils::appendToBuffer(binBuffer, indices.data(), indexSize);

	glm::vec3 positionMin= vertices[0];
	glm::vec3 positionMax= vertices[0];
	for (const glm::vec3& vertex : vertices)
	{
		positionMin= glm::min(positionMin, vertex);
		positionMax= glm::max(positionMax, vertex);
	}

	// cgltf_write takes mutable C strings and does not own them, so they live here until the write returns
	std::string versionString= "2.0";
	std::string generatorString= "MikanXR";
	std::string meshNameString= GlbMeshWriterUtils::makeJsonSafeName(meshName);
	std::string positionName= "POSITION";
	std::string normalName= "NORMAL";
	std::string texCoordName= "TEXCOORD_0";
	std::string textureUri= GlbMeshWriterUtils::encodeUri(textureFileName);

	// The document's arrays, zero-initialized like the C structs cgltf expects
	cgltf_buffer buffer;
	memset(&buffer, 0, sizeof(cgltf_buffer));
	buffer.size= binBuffer.size();

	std::vector<cgltf_buffer_view> bufferViews;
	std::vector<cgltf_accessor> accessors;
	std::vector<cgltf_attribute> attributes;
	bufferViews.reserve(4);
	accessors.reserve(4);
	attributes.reserve(3);

	auto addAccessor= [&](size_t byteOffset, size_t byteSize, cgltf_buffer_view_type viewType, cgltf_type type,
						  cgltf_component_type componentType, size_t count) -> size_t
	{
		cgltf_buffer_view bufferView;
		memset(&bufferView, 0, sizeof(cgltf_buffer_view));
		bufferView.buffer= &buffer;
		bufferView.offset= byteOffset;
		bufferView.size= byteSize;
		bufferView.type= viewType;
		bufferViews.push_back(bufferView);

		cgltf_accessor accessor;
		memset(&accessor, 0, sizeof(cgltf_accessor));
		accessor.component_type= componentType;
		accessor.type= type;
		accessor.count= count;
		accessor.stride= cgltf_calc_size(type, componentType);
		accessors.push_back(accessor);

		return accessors.size() - 1;
	};

	const size_t positionAccessorIndex= addAccessor(positionOffset, positionSize, cgltf_buffer_view_type_vertices,
													cgltf_type_vec3, cgltf_component_type_r_32f, vertices.size());
	accessors[positionAccessorIndex].has_min= 1;
	accessors[positionAccessorIndex].has_max= 1;
	for (int axis= 0; axis < 3; ++axis)
	{
		accessors[positionAccessorIndex].min[axis]= positionMin[axis];
		accessors[positionAccessorIndex].max[axis]= positionMax[axis];
	}

	size_t normalAccessorIndex= 0;
	if (bHasNormals)
	{
		normalAccessorIndex= addAccessor(normalOffset, normalSize, cgltf_buffer_view_type_vertices, cgltf_type_vec3,
										 cgltf_component_type_r_32f, normals.size());
	}

	size_t texCoordAccessorIndex= 0;
	if (bHasTexCoords)
	{
		texCoordAccessorIndex= addAccessor(texCoordOffset, texCoordSize, cgltf_buffer_view_type_vertices,
										   cgltf_type_vec2, cgltf_component_type_r_32f, texCoords.size());
	}

	const size_t indexAccessorIndex= addAccessor(indexOffset, indexSize, cgltf_buffer_view_type_indices,
												 cgltf_type_scalar, cgltf_component_type_r_32u, indices.size());

	// Accessors reference their buffer views by pointer, so link them once both arrays stop growing
	for (size_t index= 0; index < accessors.size(); ++index)
	{
		accessors[index].buffer_view= &bufferViews[index];
	}

	auto addAttribute= [&](std::string& name, cgltf_attribute_type type, size_t accessorIndex)
	{
		cgltf_attribute attribute;
		memset(&attribute, 0, sizeof(cgltf_attribute));
		attribute.name= &name[0];
		attribute.type= type;
		attribute.index= 0;
		attribute.data= &accessors[accessorIndex];
		attributes.push_back(attribute);
	};

	addAttribute(positionName, cgltf_attribute_type_position, positionAccessorIndex);
	if (bHasNormals)
	{
		addAttribute(normalName, cgltf_attribute_type_normal, normalAccessorIndex);
	}
	if (bHasTexCoords)
	{
		addAttribute(texCoordName, cgltf_attribute_type_texcoord, texCoordAccessorIndex);
	}

	cgltf_image image;
	memset(&image, 0, sizeof(cgltf_image));
	cgltf_texture texture;
	memset(&texture, 0, sizeof(cgltf_texture));
	cgltf_material material;
	memset(&material, 0, sizeof(cgltf_material));
	if (bHasTexture)
	{
		image.uri= &textureUri[0];
		texture.image= &image;

		material.has_pbr_metallic_roughness= 1;
		material.pbr_metallic_roughness.base_color_texture.texture= &texture;
		material.pbr_metallic_roughness.base_color_texture.texcoord= 0;
		material.pbr_metallic_roughness.base_color_texture.scale= 1.f;
		for (int channel= 0; channel < 4; ++channel)
		{
			material.pbr_metallic_roughness.base_color_factor[channel]= 1.f;
		}
		material.pbr_metallic_roughness.metallic_factor= 0.f;
		material.pbr_metallic_roughness.roughness_factor= 1.f;
		material.alpha_mode= cgltf_alpha_mode_opaque;
	}

	cgltf_primitive primitive;
	memset(&primitive, 0, sizeof(cgltf_primitive));
	primitive.type= cgltf_primitive_type_triangles;
	primitive.indices= &accessors[indexAccessorIndex];
	primitive.material= bHasTexture ? &material : nullptr;
	primitive.attributes= attributes.data();
	primitive.attributes_count= attributes.size();

	cgltf_mesh mesh;
	memset(&mesh, 0, sizeof(cgltf_mesh));
	mesh.name= &meshNameString[0];
	mesh.primitives= &primitive;
	mesh.primitives_count= 1;

	cgltf_node node;
	memset(&node, 0, sizeof(cgltf_node));
	node.name= &meshNameString[0];
	node.mesh= &mesh;

	cgltf_node* sceneNodes[1]= {&node};
	cgltf_scene scene;
	memset(&scene, 0, sizeof(cgltf_scene));
	scene.nodes= sceneNodes;
	scene.nodes_count= 1;

	cgltf_data data;
	memset(&data, 0, sizeof(cgltf_data));
	data.file_type= cgltf_file_type_glb;
	data.asset.version= &versionString[0];
	data.asset.generator= &generatorString[0];
	data.meshes= &mesh;
	data.meshes_count= 1;
	data.accessors= accessors.data();
	data.accessors_count= accessors.size();
	data.buffer_views= bufferViews.data();
	data.buffer_views_count= bufferViews.size();
	data.buffers= &buffer;
	data.buffers_count= 1;
	if (bHasTexture)
	{
		data.materials= &material;
		data.materials_count= 1;
		data.images= &image;
		data.images_count= 1;
		data.textures= &texture;
		data.textures_count= 1;
	}
	data.nodes= &node;
	data.nodes_count= 1;
	data.scenes= &scene;
	data.scenes_count= 1;
	data.scene= &scene;
	data.bin= binBuffer.data();
	data.bin_size= binBuffer.size();

	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));
	options.type= cgltf_file_type_glb;

	const cgltf_result result= cgltf_write_file(&options, pathString.c_str(), &data);
	if (result != cgltf_result_success)
	{
		MIKAN_LOG_ERROR("GlbMeshWriter") << "Failed to write " << pathString;
		return false;
	}

	return true;
}
