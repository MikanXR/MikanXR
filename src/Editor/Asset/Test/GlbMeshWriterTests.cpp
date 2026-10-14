#include "GlbMeshWriterTests.h"
#include "GlbMeshWriter.h"
#include "unit_test.h"

#include "cgltf.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

// ---- Helpers ----

static std::filesystem::path glb_test_get_output_directory()
{
	return std::filesystem::temp_directory_path() / "MikanGlbMeshWriterTests";
}

// Owns a parsed glTF document and frees it when the test is done with it
class GlbTestDocument
{
public:
	GlbTestDocument()= default;
	~GlbTestDocument() { cgltf_free(m_data); }

	GlbTestDocument(const GlbTestDocument&)= delete;
	GlbTestDocument& operator=(const GlbTestDocument&)= delete;

	// Parses, loads the buffers, and validates the way the importer does
	bool load(const std::filesystem::path& path)
	{
		const std::string pathString= path.string();

		cgltf_options options;
		memset(&options, 0, sizeof(cgltf_options));

		cgltf_result result= cgltf_parse_file(&options, pathString.c_str(), &m_data);
		if (result != cgltf_result_success)
		{
			fprintf(stdout, "    cgltf_parse_file failed (%d)\n", (int)result);
			return false;
		}

		result= cgltf_load_buffers(&options, m_data, pathString.c_str());
		if (result != cgltf_result_success)
		{
			fprintf(stdout, "    cgltf_load_buffers failed (%d)\n", (int)result);
			return false;
		}

		result= cgltf_validate(m_data);
		if (result != cgltf_result_success)
		{
			fprintf(stdout, "    cgltf_validate failed (%d)\n", (int)result);
			return false;
		}

		return true;
	}

	const cgltf_data* get() const { return m_data; }

private:
	cgltf_data* m_data= nullptr;
};

struct GlbTestQuad
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;
};

// Two triangles over a tilted quad, with texcoords that are not symmetric under a V flip
static GlbTestQuad glb_test_make_quad()
{
	GlbTestQuad quad;
	quad.vertices= {
		glm::vec3(-0.5f, -0.25f, 1.f),
		glm::vec3(0.5f, -0.25f, 1.f),
		glm::vec3(0.5f, 0.75f, 1.5f),
		glm::vec3(-0.5f, 0.75f, 1.5f),
	};
	quad.normals= {
		glm::vec3(0.f, -0.447214f, 0.894427f),
		glm::vec3(0.f, -0.447214f, 0.894427f),
		glm::vec3(0.f, -0.447214f, 0.894427f),
		glm::vec3(0.f, -0.447214f, 0.894427f),
	};
	quad.texCoords= {
		glm::vec2(0.1f, 0.2f),
		glm::vec2(0.9f, 0.2f),
		glm::vec2(0.9f, 0.7f),
		glm::vec2(0.1f, 0.7f),
	};
	quad.indices= {0, 1, 2, 0, 2, 3};
	return quad;
}

static bool glb_test_nearly_equal(float a, float b) { return std::fabs(a - b) <= 1e-6f; }

// ---- Test cases ----

// A full quad survives a write and read back through cgltf: attribute counts, position bounds,
// index order, and positions all match, and texcoords come back V-down
bool glb_mesh_writer_test_quad_round_trip()
{
	UNIT_TEST_BEGIN("quad round trips through a glb file")

	const GlbTestQuad quad= glb_test_make_quad();
	const std::filesystem::path glbPath= glb_test_get_output_directory() / "quad.glb";

	if (!GlbMeshWriter::write(glbPath, "quad", quad.vertices, quad.normals, quad.texCoords, quad.indices))
	{
		fprintf(stdout, "    write failed\n");
		success= false;
	}

	GlbTestDocument document;
	if (success && !document.load(glbPath))
	{
		success= false;
	}

	const cgltf_data* data= document.get();
	if (success && (data->meshes_count != 1 || data->meshes[0].primitives_count != 1))
	{
		fprintf(stdout, "    expected one mesh with one primitive\n");
		success= false;
	}

	const cgltf_primitive* primitive= success ? &data->meshes[0].primitives[0] : nullptr;
	if (success && primitive->type != cgltf_primitive_type_triangles)
	{
		fprintf(stdout, "    primitive is not a triangle list\n");
		success= false;
	}

	if (success && (data->materials_count != 0 || primitive->material != nullptr))
	{
		fprintf(stdout, "    untextured mesh should carry no material\n");
		success= false;
	}

	const cgltf_accessor* positions=
		success ? cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0) : nullptr;
	const cgltf_accessor* normals= success ? cgltf_find_accessor(primitive, cgltf_attribute_type_normal, 0) : nullptr;
	const cgltf_accessor* texCoords=
		success ? cgltf_find_accessor(primitive, cgltf_attribute_type_texcoord, 0) : nullptr;
	if (success
		&& (positions == nullptr || normals == nullptr || texCoords == nullptr || primitive->indices == nullptr))
	{
		fprintf(stdout, "    missing POSITION, NORMAL, TEXCOORD_0, or indices\n");
		success= false;
	}

	if (success && (positions->count != 4 || normals->count != 4 || texCoords->count != 4))
	{
		fprintf(stdout, "    expected 4 of each vertex attribute\n");
		success= false;
	}

	if (success)
	{
		const glm::vec3 expectedMin(-0.5f, -0.25f, 1.f);
		const glm::vec3 expectedMax(0.5f, 0.75f, 1.5f);
		if (!positions->has_min || !positions->has_max)
		{
			fprintf(stdout, "    POSITION has no min/max\n");
			success= false;
		}
		for (int axis= 0; success && axis < 3; ++axis)
		{
			if (positions->min[axis] != expectedMin[axis] || positions->max[axis] != expectedMax[axis])
			{
				fprintf(stdout, "    POSITION bounds axis %d: [%f, %f], expected [%f, %f]\n", axis,
						positions->min[axis], positions->max[axis], expectedMin[axis], expectedMax[axis]);
				success= false;
			}
		}
	}

	if (success)
	{
		for (cgltf_size vertexIndex= 0; vertexIndex < 4; ++vertexIndex)
		{
			float position[3]= {0.f, 0.f, 0.f};
			float normal[3]= {0.f, 0.f, 0.f};
			float texCoord[2]= {0.f, 0.f};
			cgltf_accessor_read_float(positions, vertexIndex, position, 3);
			cgltf_accessor_read_float(normals, vertexIndex, normal, 3);
			cgltf_accessor_read_float(texCoords, vertexIndex, texCoord, 2);

			const glm::vec3& expectedPosition= quad.vertices[vertexIndex];
			const glm::vec3& expectedNormal= quad.normals[vertexIndex];
			const glm::vec2& expectedTexCoord= quad.texCoords[vertexIndex];
			for (int axis= 0; axis < 3; ++axis)
			{
				if (!glb_test_nearly_equal(position[axis], expectedPosition[axis]))
				{
					fprintf(stdout, "    vertex %d position mismatch\n", (int)vertexIndex);
					success= false;
				}
				if (!glb_test_nearly_equal(normal[axis], expectedNormal[axis]))
				{
					fprintf(stdout, "    vertex %d normal mismatch\n", (int)vertexIndex);
					success= false;
				}
			}

			if (!glb_test_nearly_equal(texCoord[0], expectedTexCoord.x)
				|| !glb_test_nearly_equal(texCoord[1], 1.f - expectedTexCoord.y))
			{
				fprintf(stdout, "    vertex %d texcoord (%f, %f), expected (%f, %f)\n", (int)vertexIndex, texCoord[0],
						texCoord[1], expectedTexCoord.x, 1.f - expectedTexCoord.y);
				success= false;
			}
		}
	}

	if (success)
	{
		if (primitive->indices->count != quad.indices.size())
		{
			fprintf(stdout, "    expected %d indices, got %d\n", (int)quad.indices.size(),
					(int)primitive->indices->count);
			success= false;
		}

		for (cgltf_size index= 0; success && index < primitive->indices->count; ++index)
		{
			const cgltf_size readIndex= cgltf_accessor_read_index(primitive->indices, index);
			if (readIndex != quad.indices[index])
			{
				fprintf(stdout, "    index %d is %d, expected %d\n", (int)index, (int)readIndex,
						(int)quad.indices[index]);
				success= false;
			}
		}
	}

	UNIT_TEST_COMPLETE()
}

// Without normals, texcoords, or a texture only POSITION and the index list are written
bool glb_mesh_writer_test_positions_only()
{
	UNIT_TEST_BEGIN("positions-only mesh writes no optional attributes")

	const GlbTestQuad quad= glb_test_make_quad();
	const std::filesystem::path glbPath= glb_test_get_output_directory() / "quad_positions_only.glb";

	if (!GlbMeshWriter::write(glbPath, "quad", quad.vertices, std::vector<glm::vec3>(), std::vector<glm::vec2>(),
							  quad.indices))
	{
		fprintf(stdout, "    write failed\n");
		success= false;
	}

	GlbTestDocument document;
	if (success && !document.load(glbPath))
	{
		success= false;
	}

	const cgltf_data* data= document.get();
	if (success && (data->meshes_count != 1 || data->meshes[0].primitives_count != 1))
	{
		fprintf(stdout, "    expected one mesh with one primitive\n");
		success= false;
	}

	if (success)
	{
		const cgltf_primitive& primitive= data->meshes[0].primitives[0];
		if (primitive.attributes_count != 1 || primitive.attributes[0].type != cgltf_attribute_type_position)
		{
			fprintf(stdout, "    expected POSITION as the only attribute, got %d attributes\n",
					(int)primitive.attributes_count);
			success= false;
		}

		if (primitive.indices == nullptr || primitive.indices->count != quad.indices.size())
		{
			fprintf(stdout, "    expected %d indices\n", (int)quad.indices.size());
			success= false;
		}

		if (primitive.material != nullptr || data->materials_count != 0 || data->images_count != 0)
		{
			fprintf(stdout, "    expected no material or image\n");
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// A texture file name becomes one image uri referenced by the primitive's material
bool glb_mesh_writer_test_texture_reference()
{
	UNIT_TEST_BEGIN("texture file name becomes a material image reference")

	const GlbTestQuad quad= glb_test_make_quad();
	const std::filesystem::path glbPath= glb_test_get_output_directory() / "quad_textured.glb";

	if (!GlbMeshWriter::write(glbPath, "quad", quad.vertices, quad.normals, quad.texCoords, quad.indices, "quad.png"))
	{
		fprintf(stdout, "    write failed\n");
		success= false;
	}

	GlbTestDocument document;
	if (success && !document.load(glbPath))
	{
		success= false;
	}

	const cgltf_data* data= document.get();
	if (success
		&& (data->images_count != 1 || data->images[0].uri == nullptr || strcmp(data->images[0].uri, "quad.png") != 0))
	{
		fprintf(stdout, "    expected one image with uri quad.png\n");
		success= false;
	}

	if (success && (data->materials_count != 1 || data->textures_count != 1))
	{
		fprintf(stdout, "    expected one material and one texture\n");
		success= false;
	}

	if (success)
	{
		const cgltf_material& material= data->materials[0];
		const cgltf_texture* baseColorTexture= material.pbr_metallic_roughness.base_color_texture.texture;
		if (!material.has_pbr_metallic_roughness || baseColorTexture == nullptr
			|| baseColorTexture->image != &data->images[0])
		{
			fprintf(stdout, "    material base colour texture does not reference the image\n");
			success= false;
		}

		if (data->meshes_count != 1 || data->meshes[0].primitives_count != 1
			|| data->meshes[0].primitives[0].material != &data->materials[0])
		{
			fprintf(stdout, "    primitive does not reference the material\n");
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// Writing into a directory that does not exist fails cleanly
bool glb_mesh_writer_test_missing_directory_fails()
{
	UNIT_TEST_BEGIN("missing output directory returns false")

	const GlbTestQuad quad= glb_test_make_quad();
	const std::filesystem::path glbPath= glb_test_get_output_directory() / "does_not_exist" / "quad.glb";

	if (GlbMeshWriter::write(glbPath, "quad", quad.vertices, quad.normals, quad.texCoords, quad.indices))
	{
		fprintf(stdout, "    write into a missing directory reported success\n");
		success= false;
	}

	std::error_code errorCode;
	if (std::filesystem::exists(glbPath, errorCode))
	{
		fprintf(stdout, "    a file was created in the missing directory\n");
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// ---- Test Suite ----

bool run_glb_mesh_writer_tests()
{
	std::error_code errorCode;
	const std::filesystem::path outputDirectory= glb_test_get_output_directory();
	std::filesystem::remove_all(outputDirectory, errorCode);
	std::filesystem::create_directories(outputDirectory, errorCode);

	UNIT_TEST_MODULE_BEGIN("glb_mesh_writer")
	UNIT_TEST_MODULE_CALL_TEST(glb_mesh_writer_test_quad_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(glb_mesh_writer_test_positions_only);
	UNIT_TEST_MODULE_CALL_TEST(glb_mesh_writer_test_texture_reference);
	UNIT_TEST_MODULE_CALL_TEST(glb_mesh_writer_test_missing_directory_fails);
	std::filesystem::remove_all(outputDirectory, errorCode);
	UNIT_TEST_MODULE_END()
}
