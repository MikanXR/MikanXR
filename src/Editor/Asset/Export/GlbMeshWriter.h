#pragma once

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Writes one triangle mesh as a self-contained binary glTF (.glb) file
class GlbMeshWriter
{
public:
	// Positions and winding are written as given, since glTF shares the editor's right-handed Y-up frame.
	// Normals and texcoords are written only when there is one per vertex. Texcoords are taken V-up
	// (the OBJ convention) and stored V-down as glTF requires. A non-empty textureFileName adds a
	// material whose base colour texture references that file, relative to the written model.
	static bool write(const std::filesystem::path& path, const std::string& meshName,
					  const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals,
					  const std::vector<glm::vec2>& texCoords, const std::vector<uint32_t>& indices,
					  const std::string& textureFileName= std::string());
};
