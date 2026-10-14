#pragma once

#include "MikanRendererFwd.h"

#include <vector>

// Implemented by a component whose meshes outline its object while that object is selected or
// hovered. The renderables are drawn into the outline mask whether or not they are visible, so
// an object shown only as a wireframe still outlines its solid silhouette.
class ISelectionOutlineSource
{
public:
	virtual ~ISelectionOutlineSource() {}

	virtual void getSelectionOutlineRenderables(std::vector<IMkSceneRenderableConstPtr>& outRenderables) const= 0;
};
