#pragma once

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Writes triangle meshes as a self-contained binary glTF (.glb), to a file or to memory
class GlbMeshWriter
{
public:
	// One triangle list. Positions and winding are written as given, since glTF shares the editor's
	// right-handed Y-up frame. Normals and texcoords are written only when there is one per vertex.
	// Texcoords are taken V-up (the OBJ convention) and stored V-down as glTF requires.
	struct MeshData
	{
		std::string name;
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> texCoords;
		std::vector<uint32_t> indices;
		// Index into the image list used as this mesh's base colour texture, or -1 for no material
		int imageIndex= -1;
	};

	// A base colour image. A non-empty uri references a file relative to the written model.
	// Otherwise bytes (an encoded PNG or JPEG named by mimeType) are embedded in the binary chunk.
	struct ImageData
	{
		std::string uri;
		std::string mimeType;
		std::vector<uint8_t> bytes;
	};

	// One mesh and one node per input mesh under one scene, and one unlit material per image
	static bool writeToBytes(const std::vector<MeshData>& meshes, const std::vector<ImageData>& images,
							 std::vector<uint8_t>& outBytes);

	// A single mesh written to a file. A non-empty textureFileName adds a material whose base colour
	// texture references that file, relative to the written model.
	static bool write(const std::filesystem::path& path, const std::string& meshName,
					  const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals,
					  const std::vector<glm::vec2>& texCoords, const std::vector<uint32_t>& indices,
					  const std::string& textureFileName= std::string());
};
