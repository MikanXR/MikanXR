#pragma once

#include "ComponentFwd.h"
#include "MulticastDelegate.h"
#include "IMkMesh.h"
#include "ObjectSystemFwd.h"
#include "MikanRendererFwd.h"
#include "TransformComponent.h"

class StaticMeshComponent : public TransformComponent
{
public:
	StaticMeshComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "StaticMeshComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	IMkStaticMeshInstancePtr getStaticMesh() const;
	void setStaticMesh(IMkStaticMeshInstancePtr meshInstance);

	MulticastDelegate<void(StaticMeshComponentWeakPtr meshComponent)> OnMeshChanged;
};
