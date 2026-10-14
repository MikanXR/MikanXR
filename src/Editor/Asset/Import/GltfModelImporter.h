#pragma once

#include "IModelImporter.h"

// Imports glTF 2.0 models, both binary (.glb) and JSON (.gltf) with external or embedded buffers
class GltfModelImporter : public IModelImporter
{
public:
	GltfModelImporter(class MikanModelResourceManager* ownerManager)
		: IModelImporter(ownerManager)
	{
	}

	virtual MikanRenderModelResourcePtr importModelFromFile(const std::filesystem::path& modelPath,
															MkMaterialConstPtr overrideMaterial) override;
};
