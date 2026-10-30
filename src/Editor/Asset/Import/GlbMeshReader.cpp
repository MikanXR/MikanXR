#include "GlbMeshReader.h"

#include "cgltf.h"

#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/matrix.hpp"

#include <cstring>

namespace GlbMeshReaderInternal
{
struct Document
{
	cgltf_data* data= nullptr;
	~Document()
	{
		if (data)
			cgltf_free(data);
	}
};

bool appendPrimitive(const cgltf_primitive& primitive, const glm::mat4& nodeToWorld, TriangleMesh& mesh,
					 bool& bAllHaveNormals)
{
	if (primitive.type != cgltf_primitive_type_triangles || primitive.has_draco_mesh_compression)
		return true;

	const cgltf_accessor* positions= cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
	if (positions == nullptr || positions->count == 0)
		return true;
	const cgltf_accessor* normals= cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
	if (normals != nullptr && normals->count != positions->count)
		normals= nullptr;
	if (normals == nullptr)
		bAllHaveNormals= false;

	const uint32_t firstVertex= (uint32_t)mesh.vertices.size();
	const glm::mat3 normalMatrix= glm::transpose(glm::inverse(glm::mat3(nodeToWorld)));
	for (cgltf_size i= 0; i < positions->count; ++i)
	{
		glm::vec3 position(0.f);
		if (!cgltf_accessor_read_float(positions, i, glm::value_ptr(position), 3))
			return false;
		mesh.vertices.push_back(glm::vec3(nodeToWorld * glm::vec4(position, 1.f)));
		if (normals != nullptr)
		{
			glm::vec3 normal(0.f);
			if (!cgltf_accessor_read_float(normals, i, glm::value_ptr(normal), 3))
				return false;
			mesh.normals.push_back(glm::normalize(normalMatrix * normal));
		}
	}

	if (primitive.indices != nullptr)
	{
		for (cgltf_size i= 0; i < primitive.indices->count; ++i)
		{
			mesh.indices.push_back(firstVertex + (uint32_t)cgltf_accessor_read_index(primitive.indices, i));
		}
	}
	else
	{
		for (cgltf_size i= 0; i < positions->count; ++i)
		{
			mesh.indices.push_back(firstVertex + (uint32_t)i);
		}
	}
	return true;
}
} // namespace GlbMeshReaderInternal

bool GlbMeshReader::read(const std::filesystem::path& path, TriangleMesh& outMesh, std::string& outError)
{
	outMesh.clear();
	outError.clear();

	const std::string pathString= path.string();
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));
	GlbMeshReaderInternal::Document document;
	if (cgltf_parse_file(&options, pathString.c_str(), &document.data) != cgltf_result_success)
	{
		outError= "could not parse " + pathString;
		return false;
	}
	if (cgltf_load_buffers(&options, document.data, pathString.c_str()) != cgltf_result_success)
	{
		outError= "could not load the buffers of " + pathString;
		return false;
	}

	bool bAllHaveNormals= true;
	for (cgltf_size n= 0; n < document.data->nodes_count; ++n)
	{
		const cgltf_node& node= document.data->nodes[n];
		if (node.mesh == nullptr)
			continue;

		float world[16];
		cgltf_node_transform_world(&node, world);
		const glm::mat4 nodeToWorld= glm::make_mat4(world);
		for (cgltf_size p= 0; p < node.mesh->primitives_count; ++p)
		{
			if (!GlbMeshReaderInternal::appendPrimitive(node.mesh->primitives[p], nodeToWorld, outMesh,
														bAllHaveNormals))
			{
				outError= "could not read a primitive of " + pathString;
				return false;
			}
		}
	}
	if (!bAllHaveNormals)
		outMesh.normals.clear();
	if (outMesh.indices.empty())
	{
		outError= pathString + " holds no triangles";
		return false;
	}
	return true;
}
