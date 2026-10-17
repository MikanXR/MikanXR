#include "IMkStaticMeshInstance.h"
#include "StaticMeshComponent.h"
#include "MikanObject.h"

StaticMeshComponent::StaticMeshComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

IMkStaticMeshInstancePtr StaticMeshComponent::getStaticMesh() const
{
	return std::dynamic_pointer_cast<IMkStaticMeshInstance>(m_sceneRenderable);
}

void StaticMeshComponent::setStaticMesh(IMkStaticMeshInstancePtr meshInstance)
{
	m_sceneRenderable= meshInstance;
	if (OnMeshChanged)
	{
		StaticMeshComponentWeakPtr meshComponent= getSelfWeakPtr<StaticMeshComponent>();

		OnMeshChanged(meshComponent);
	}
}
