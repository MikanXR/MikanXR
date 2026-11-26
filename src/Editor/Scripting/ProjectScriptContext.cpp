#include "AnchorObjectSystem.h"
#include "AnchorComponent.h"
#include "BoxShapeComponent.h"
#include "BoxShapeSystem.h"
#include "BoxStencilComponent.h"
#include "BoxStencilSystem.h"
#include "CameraObjectSystem.h"
#include "CompositorObjectSystem.h"
#include "CameraComponent.h"
#include "CompositorComponent.h"
#include "OutputComponent.h"
#include "SpoutOutputComponent.h"
#include "SpoutOutputSystem.h"
#include "DMXFixtureComponent.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXPresetComponent.h"
#include "DMXPresetSystem.h"
#include "DMXSequenceComponent.h"
#include "DMXSequenceSystem.h"
#include "DMXObjectSystem.h"
#include "LuaMath.h"
#include "MarkerComponent.h"
#include "MikanComponent.h"
#include "ModelShapeComponent.h"
#include "ModelShapeSystem.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "ProjectManager.h"
#include "ProjectScriptContext.h"
#include "QuadShapeComponent.h"
#include "QuadShapeSystem.h"
#include "QuadStencilComponent.h"
#include "QuadStencilSystem.h"
#include "ShapeComponent.h"
#include "RGBPixelGridComponent.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"
#include "SceneObjectSystem.h"
#include "SceneComponent.h"
#include "StageComponent.h"
#include "StageObjectSystem.h"
#include "TransformComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

namespace
{
// LuaBridge pushes a pointer by its static type, so a component reaches Lua
// through a thunk that casts to the concrete class first. A null pointer, or
// one that is not of this class, pushes nil.
template <class t_component_type>
void registerComponentPushThunk(CommonScriptContext* context)
{
	context->registerComponentClass(t_component_type::k_componentClassName,
									[](lua_State* L, MikanComponentPtr component)
									{
										t_component_type* typedComponent=
											std::dynamic_pointer_cast<t_component_type>(component).get();
										return static_cast<bool>(luabridge::push(L, typedComponent));
									});
}
} // namespace

ProjectScriptContext::ProjectScriptContext(ProjectManagerPtr projectManager)
	: CommonScriptContext()
	, m_projectManager(projectManager)
{
}

MikanComponentPtr ProjectScriptContext::resolveComponent(const std::string& componentClass,
														 MikanComponentID componentId) const
{
	ProjectManagerPtr projectManager= m_projectManager.lock();
	if (!projectManager)
		return nullptr;

	MikanComponentPtr component= projectManager->getComponentById(componentId);
	if (!component || component->getComponentClassName() != componentClass)
		return nullptr;

	return component;
}

bool ProjectScriptContext::bindContextFunctions()
{
	if (!CommonScriptContext::bindContextFunctions())
		return false;

	ProjectManagerPtr projectManager= m_projectManager.lock();
	if (!projectManager)
		return false;

	// Register object system classes before component classes
	CameraObjectSystem::bindLuaFunctions(m_luaState);
	SceneObjectSystem::bindLuaFunctions(m_luaState);
	StageObjectSystem::bindLuaFunctions(m_luaState);
	DMXObjectSystem::bindLuaFunctions(m_luaState);
	RGBSpotLightSystem::bindLuaFunctions(m_luaState);
	DMXFixtureGroupSystem::bindLuaFunctions(m_luaState);
	DMXPresetSystem::bindLuaFunctions(m_luaState);
	DMXSequenceSystem::bindLuaFunctions(m_luaState);
	AnchorObjectSystem::bindLuaFunctions(m_luaState);
	CompositorObjectSystem::bindLuaFunctions(m_luaState);
	SpoutOutputSystem::bindLuaFunctions(m_luaState);
	ModelStencilSystem::bindLuaFunctions(m_luaState);
	BoxStencilSystem::bindLuaFunctions(m_luaState);
	QuadStencilSystem::bindLuaFunctions(m_luaState);
	ModelShapeSystem::bindLuaFunctions(m_luaState);
	BoxShapeSystem::bindLuaFunctions(m_luaState);
	QuadShapeSystem::bindLuaFunctions(m_luaState);

	// Bind in dependency order: parent classes before derived classes
	MikanComponent::bindLuaFunctions(m_luaState);
	CompositorComponent::bindLuaFunctions(m_luaState);
	OutputComponent::bindLuaFunctions(m_luaState);
	SpoutOutputComponent::bindLuaFunctions(m_luaState);
	TransformComponent::bindLuaFunctions(m_luaState);
	SceneComponent::bindLuaFunctions(m_luaState);
	StageComponent::bindLuaFunctions(m_luaState);
	StencilComponent::bindLuaFunctions(m_luaState);
	QuadStencilComponent::bindLuaFunctions(m_luaState);
	BoxStencilComponent::bindLuaFunctions(m_luaState);
	ModelStencilComponent::bindLuaFunctions(m_luaState);
	ShapeComponent::bindLuaFunctions(m_luaState);
	QuadShapeComponent::bindLuaFunctions(m_luaState);
	BoxShapeComponent::bindLuaFunctions(m_luaState);
	ModelShapeComponent::bindLuaFunctions(m_luaState);
	DMXFixtureComponent::bindLuaFunctions(m_luaState);
	RGBSpotLightComponent::bindLuaFunctions(m_luaState);
	RGBPixelGridComponent::bindLuaFunctions(m_luaState);
	DMXFixtureGroupComponent::bindLuaFunctions(m_luaState);
	DMXPresetComponent::bindLuaFunctions(m_luaState);
	DMXSequenceComponent::bindLuaFunctions(m_luaState);
	CameraComponent::bindLuaFunctions(m_luaState);
	AnchorComponent::bindLuaFunctions(m_luaState);
	MarkerComponent::bindLuaFunctions(m_luaState);

	// The classes ScriptContext.registerComponent accepts
	registerComponentPushThunk<MikanComponent>(this);
	registerComponentPushThunk<CompositorComponent>(this);
	registerComponentPushThunk<TransformComponent>(this);
	registerComponentPushThunk<SceneComponent>(this);
	registerComponentPushThunk<StageComponent>(this);
	registerComponentPushThunk<StencilComponent>(this);
	registerComponentPushThunk<QuadStencilComponent>(this);
	registerComponentPushThunk<BoxStencilComponent>(this);
	registerComponentPushThunk<ModelStencilComponent>(this);
	registerComponentPushThunk<ShapeComponent>(this);
	registerComponentPushThunk<QuadShapeComponent>(this);
	registerComponentPushThunk<BoxShapeComponent>(this);
	registerComponentPushThunk<ModelShapeComponent>(this);
	registerComponentPushThunk<DMXFixtureComponent>(this);
	registerComponentPushThunk<RGBSpotLightComponent>(this);
	registerComponentPushThunk<RGBPixelGridComponent>(this);
	registerComponentPushThunk<DMXFixtureGroupComponent>(this);
	registerComponentPushThunk<DMXPresetComponent>(this);
	registerComponentPushThunk<DMXSequenceComponent>(this);
	registerComponentPushThunk<CameraComponent>(this);
	registerComponentPushThunk<AnchorComponent>(this);
	registerComponentPushThunk<MarkerComponent>(this);

	// Expose every scriptable object system as a global so scripts can look up
	// objects by name or id
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<CameraObjectSystem>().get(), "CameraSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<SceneObjectSystem>().get(), "SceneSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<AnchorObjectSystem>().get(), "AnchorSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<CompositorObjectSystem>().get(),
						 "CompositorSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<SpoutOutputSystem>().get(), "SpoutOutputSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<StageObjectSystem>().get(), "StageSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<DMXObjectSystem>().get(), "DMXSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<RGBSpotLightSystem>().get(), "RGBSpotLightSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<DMXFixtureGroupSystem>().get(),
						 "DMXFixtureGroupSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<DMXPresetSystem>().get(), "DMXPresetSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<DMXSequenceSystem>().get(), "DMXSequenceSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<ModelStencilSystem>().get(), "ModelStencilSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<BoxStencilSystem>().get(), "BoxStencilSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<QuadStencilSystem>().get(), "QuadStencilSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<ModelShapeSystem>().get(), "ModelShapeSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<BoxShapeSystem>().get(), "BoxShapeSystem");
	luabridge::setGlobal(m_luaState, projectManager->getSystemOfType<QuadShapeSystem>().get(), "QuadShapeSystem");

	return true;
}
