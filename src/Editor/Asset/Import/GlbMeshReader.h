#pragma once

#include "ReconstructionTypes.h"

#include <filesystem>
#include <string>

// Reads a glTF model's triangle primitives into one TriangleMesh with the node
// transforms applied, with no GL in sight, for the headless tools. Normals are
// kept when every primitive carries them and left empty otherwise.
class GlbMeshReader
{
public:
	static bool read(const std::filesystem::path& path, TriangleMesh& outMesh, std::string& outError);
};
