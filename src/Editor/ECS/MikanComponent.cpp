#include "AnchorObjectSystem.h"
#include "AssetReference.h"
#include "CameraObjectSystem.h"
#include "CompositorObjectSystem.h"
#include "DMXObjectSystem.h"
#include "IEditorWindow.h"
#include "Logger.h"
#include "SceneObjectSystem.h"
#include "MikanComponent.h"
#include "MikanComponentTypes.h"
#include "MikanObject.h"
#include "MikanObjectSystem.h"
#include "MikanPropertyTypes.h"
#include "ProjectManager.h"
#include "StringUtils.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- MikanComponentConfig -----
const std::string MikanComponentDefinition::k_componentIdPropertyId= "component_id";
const std::string MikanComponentDefinition::k_componentNamePropertyId= "component_name";

// Scripts were once attached per component under this key. They are project
// objects now, so the key is reported and dropped.
static const std::string k_legacyComponentScriptKey= "component_script";

MikanComponentDefinition::MikanComponentDefinition()
	: m_componentId(-1)
	, m_componentName()
{
}

MikanComponentDefinition::MikanComponentDefinition(int componentId, const std::string& componentName)
	: m_componentId(componentId)
	, m_componentName(componentName)
{
}

configuru::Config MikanComponentDefinition::writeToJSON()
{
	configuru::Config pt= CommonConfig::writeToJSON();

	pt[k_componentIdPropertyId]= m_componentId;
	pt[k_componentNamePropertyId]= m_componentName;

	return pt;
}

void MikanComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	CommonConfig::readFromJSON(pt);

	m_componentId= pt.get_or<int>(k_componentIdPropertyId, -1);
	m_componentName= pt.get_or<std::string>(k_componentNamePropertyId, "");

	if (pt.has_key(k_legacyComponentScriptKey))
	{
		AssetReferenceConfig legacyScript;
		legacyScript.readFromJSON(pt[k_legacyComponentScriptKey]);
		MIKAN_LOG_WARNING("MikanComponentDefinition::readFromJSON")
			<< "Ignoring legacy component script '" << legacyScript.assetPath << "' on " << m_componentName << " ("
			<< m_componentId << "); scripts are project-level now";
	}
}

bool MikanComponentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												  const Serialization::PolymorphicObjectPtr& initParams)
{
	const auto* componentValues= initParams.getTypedPointer<MikanComponentValues>();
	if (componentValues)
	{
		// Don't read the component ID since we already have an assigned component ID

		// Assign a component ID if we don't already have one
		const std::string desiredName= componentValues->component_name.getUtf8Value();
		if (!desiredName.empty())
		{
			m_componentName= componentValues->component_name.getUtf8Value();
		}
	}

	return true;
}

void MikanComponentDefinition::setComponentName(const std::string& name)
{
	if (name != m_componentName)
	{
		m_componentName= name;
		notifyPropertyChanged(
			ConfigPropertyChangeSet().addPropertyName(MikanComponentDefinition::k_componentNamePropertyId));
	}
}

// -- MikanComponent -----
MikanComponent::MikanComponent(MikanObjectWeakPtr owner)
	: m_ownerObject(owner)
{
}

void MikanComponent::init()
{
	if (m_bWasInitialized)
		return;

	MikanObjectSystemPtr objectSystemPtr= getOwnerObject()->getOwnerSystem();

	if (m_bWantsUpdate)
	{
		objectSystemPtr->onUpdate+= MakeDelegate(this, &MikanComponent::update);
	}

	m_bWasInitialized= true;

	if (objectSystemPtr->OnComponentInitialized)
	{
		objectSystemPtr->OnComponentInitialized(objectSystemPtr, shared_from_this());
	}

	if (m_definition)
	{
		// Listen for definition changes
		m_definition->OnPropertyChanged+= MakeDelegate(this, &MikanComponent::onDefinitionMarkedDirty);
	}
}

void MikanComponent::postInit() {}

void MikanComponent::dispose()
{
	if (m_bWasDisposed)
		return;

	m_bWasDisposed= true;

	MikanObjectSystemPtr objectSystemPtr= getOwnerObject()->getOwnerSystem();

	if (m_definition)
	{
		m_definition->OnPropertyChanged-= MakeDelegate(this, &MikanComponent::onDefinitionMarkedDirty);
	}

	if (objectSystemPtr->OnComponentDisposed)
	{
		objectSystemPtr->OnComponentDisposed(objectSystemPtr, shared_from_this());
	}

	if (m_bWantsUpdate)
	{
		objectSystemPtr->onUpdate-= MakeDelegate(this, &MikanComponent::update);
	}

	if (onDisposed)
	{
		onDisposed(this);
	}
}

void MikanComponent::setDefinition(MikanComponentDefinitionPtr config)
{
	assert(!m_bWasInitialized);
	m_definition= config;
	m_definition->setOwnerComponent(getSelfPtr<MikanComponent>());

	// Make the component name match the config name
	m_name= config->getComponentName();
}

void MikanComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
											 const ConfigPropertyChangeSet& changedPropertySet)
{
}

int MikanComponent::getComponentId() const
{
	if (m_definition)
	{
		return m_definition->getComponentId();
	}

	return INVALID_MIKAN_ID;
}

IEditorWindow* MikanComponent::getOwnerEditorWindow() const
{
	MikanObjectPtr ownerObject= m_ownerObject.lock();
	if (ownerObject)
	{
		return ownerObject->getOwnerSystem()->getOwnerProjectManager()->getOwnerWindow();
	}

	return nullptr;
}

IMkGraphicsContext* MikanComponent::getGraphicsContext() const
{
	IEditorWindow* ownerEditorWindow= getOwnerEditorWindow();
	if (ownerEditorWindow)
	{
		return ownerEditorWindow->getGraphicsContext().get();
	}

	return nullptr;
}

void MikanComponent::setName(const std::string& name)
{
	m_name= name;

	if (m_bWasInitialized)
		m_definition->setComponentName(name);
}

ProjectManagerPtr MikanComponent::getOwnerProjectManager() const
{
	return getOwnerObject()->getOwnerSystem()->getOwnerProjectManager();
}

bool MikanComponent::destroyOwnerObject()
{
	MikanObjectPtr ownerObject= getOwnerObject();
	if (ownerObject)
	{
		auto ownerSystem= ownerObject->getOwnerSystem();
		if (ownerSystem)
		{
			return ownerSystem->deleteObject(ownerObject);
		}
	}

	return false;
}

// -- Lua Binding ----
void MikanComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<MikanComponent>("MikanComponent")
		.addProperty("name", &MikanComponent::getName, &MikanComponent::setName)
		.addProperty("className", &MikanComponent::getComponentClassName)
		.addProperty("componentId", [](MikanComponent* c) -> int { return c->getComponentId(); })
		.addFunction("getCameraSystem", [](MikanComponent* c) -> CameraObjectSystem*
					 { return c->getObjectSystemOfType<CameraObjectSystem>().get(); })
		.addFunction("getSceneSystem", [](MikanComponent* c) -> SceneObjectSystem*
					 { return c->getObjectSystemOfType<SceneObjectSystem>().get(); })
		.addFunction("getDMXSystem", [](MikanComponent* c) -> DMXObjectSystem*
					 { return c->getObjectSystemOfType<DMXObjectSystem>().get(); })
		.addFunction("getAnchorSystem", [](MikanComponent* c) -> AnchorObjectSystem*
					 { return c->getObjectSystemOfType<AnchorObjectSystem>().get(); })
		.addFunction("getCompositorSystem", [](MikanComponent* c) -> CompositorObjectSystem*
					 { return c->getObjectSystemOfType<CompositorObjectSystem>().get(); })
		.endClass();
}

// -- IEntityAccessor ----
std::string MikanComponent::makePropertyUIIdentifier(const std::string& propName) const
{
	return StringUtils::stringify(getComponentClassName(), getComponentId(), "_", propName);
}

Serialization::StructTypeHandle MikanComponent::getClientAPIValuesStructType() const
{
	return &MikanComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void MikanComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(MikanComponentDefinition::k_componentIdPropertyId, MikanVariantType::INT)
			->setReadOnly()
			->setDefaultValue(-1)
			->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(MikanComponentDefinition::k_componentNamePropertyId,
																  MikanVariantType::STRING));
}

bool MikanComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == MikanComponentDefinition::k_componentIdPropertyId)
	{
		outValue= m_definition->getComponentId();
		return true;
	}
	else if (propertyName == MikanComponentDefinition::k_componentNamePropertyId)
	{
		outValue= getName();
		return true;
	}

	return false;
}

bool MikanComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == MikanComponentDefinition::k_componentNamePropertyId)
	{
		setName(inValue.getUtf8Value());
		return true;
	}

	return false;
}

// -- IFunctionInterface ----
void MikanComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors) {}

bool MikanComponent::invokeFunction(const std::string& functionName) { return false; }
