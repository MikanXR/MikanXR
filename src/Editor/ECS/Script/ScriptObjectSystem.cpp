#include "ScriptObjectSystem.h"
#include "Logger.h"
#include "MikanPropertyDatabase.h"
#include "MikanServer.h"
#include "PathUtils.h"
#include "ProjectScriptContext.h"
#include "ScriptRequestHandler.h"

#include <fstream>

// -- ScriptObjectSystemDefinition -----
const std::string ScriptObjectSystemDefinition::k_httpRoutesPropertyId= "http_routes";

ScriptObjectSystemDefinition::ScriptObjectSystemDefinition(const std::string& configName,
														   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config ScriptObjectSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	if (!m_httpRoutes.empty())
	{
		pt[k_httpRoutesPropertyId]= m_httpRoutes.writeToJSON();
	}

	return pt;
}

void ScriptObjectSystemDefinition::readFromJSON(const configuru::Config& pt)
{
	Super::readFromJSON(pt);

	m_httpRoutes.clear();
	if (pt.has_key(k_httpRoutesPropertyId))
	{
		m_httpRoutes.readFromJSON(pt[k_httpRoutesPropertyId]);
	}
}

void ScriptObjectSystemDefinition::setHttpRoutes(const ScriptHttpRouteTable& routes)
{
	m_httpRoutes= routes;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_httpRoutesPropertyId));
}

// -- ScriptObjectSystem ----
ScriptObjectSystem::ScriptObjectSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

bool ScriptObjectSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	if (!Super::init(definitionPtr))
		return false;

	// Route edits from the panel, undo, and automation all land on the
	// definition, and the installed routes follow from here
	if (ScriptObjectSystemDefinitionPtr definition= getTypedDefinition())
	{
		definition->OnPropertyChanged+= MakeDelegate(this, &ScriptObjectSystem::onDefinitionPropertyChanged);
		m_bDefinitionEventsBound= true;
	}

	return true;
}

void ScriptObjectSystem::postInit()
{
	Super::postInit();

	// Every other system has loaded by now, so scripts can resolve objects by name
	bindObjectLifecycleEvents();
	reloadAllScripts();
}

void ScriptObjectSystem::dispose()
{
	// Systems dispose in reverse registration order, so the others are still alive
	unbindObjectLifecycleEvents();
	disposeScriptContext();

	if (m_bDefinitionEventsBound)
	{
		if (ScriptObjectSystemDefinitionPtr definition= getTypedDefinition())
		{
			definition->OnPropertyChanged-= MakeDelegate(this, &ScriptObjectSystem::onDefinitionPropertyChanged);
		}
		m_bDefinitionEventsBound= false;
	}

	Super::dispose();

	m_bReloadPending= false;
}

void ScriptObjectSystem::bindObjectLifecycleEvents()
{
	ProjectManagerPtr projectManager= getOwnerProjectManager();
	if (!projectManager || m_bLifecycleEventsBound)
		return;

	for (const MikanObjectSystemPtr& system : projectManager->getSystems())
	{
		system->OnNewObjectFinalized+= MakeDelegate(this, &ScriptObjectSystem::onObjectFinalized);
		system->OnObjectWillBeDestroyed+= MakeDelegate(this, &ScriptObjectSystem::onObjectWillBeDestroyed);
	}
	m_bLifecycleEventsBound= true;
}

void ScriptObjectSystem::unbindObjectLifecycleEvents()
{
	ProjectManagerPtr projectManager= getOwnerProjectManager();
	if (!projectManager || !m_bLifecycleEventsBound)
		return;

	for (const MikanObjectSystemPtr& system : projectManager->getSystems())
	{
		system->OnNewObjectFinalized-= MakeDelegate(this, &ScriptObjectSystem::onObjectFinalized);
		system->OnObjectWillBeDestroyed-= MakeDelegate(this, &ScriptObjectSystem::onObjectWillBeDestroyed);
	}
	m_bLifecycleEventsBound= false;
}

void ScriptObjectSystem::onObjectFinalized(MikanObjectSystemPtr objectSystem, MikanObjectPtr object)
{
	// A recreated object (undo of a destroy) may be one a reference still names
	if (m_scriptContext)
	{
		m_scriptContext->refreshComponentVariables();
	}
}

void ScriptObjectSystem::onObjectWillBeDestroyed(MikanObjectSystemPtr objectSystem, MikanComponentPtr primaryComponent)
{
	// The object is still alive here, so its id is excluded explicitly
	if (m_scriptContext && primaryComponent)
	{
		m_scriptContext->refreshComponentVariables(primaryComponent->getComponentId());
	}
}

void ScriptObjectSystem::onDefinitionPropertyChanged(CommonConfigPtr configPtr,
													 const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(ScriptObjectSystemDefinition::k_httpRoutesPropertyId))
	{
		applyHttpRoutes();
	}
}

void ScriptObjectSystem::update(float deltaSeconds)
{
	if (m_bReloadPending)
	{
		reloadAllScripts();
	}

	Super::update(deltaSeconds);

	if (m_scriptContext)
	{
		m_scriptContext->updateScript(deltaSeconds);
	}
}

void ScriptObjectSystem::reloadAllScripts()
{
	m_bReloadPending= false;
	m_lastLoadErrors.clear();
	disposeScriptContext();

	// Gather the scripts with a path, in pool order
	std::vector<ScriptComponentPtr> scripts;
	for (ScriptDefinitionPtr definition : getTypedDefinitionConst()->getAllDefinitions())
	{
		ScriptComponentPtr script= getTypedComponentById(definition->getScriptId());
		if (script && definition->hasScriptPath())
		{
			scripts.push_back(script);
		}
	}

	// No state at all without scripts
	if (scripts.empty())
	{
		if (OnScriptsReloaded)
			OnScriptsReloaded(true);
		return;
	}

	m_scriptContext= std::make_shared<ProjectScriptContext>(getOwnerProjectManager());
	m_scriptContext->OnScriptError+= MakeDelegate(this, &ScriptObjectSystem::onScriptError);
	if (!m_scriptContext->createScriptState())
	{
		m_scriptContext= nullptr;
		if (OnScriptsReloaded)
			OnScriptsReloaded(false);
		return;
	}

	for (ScriptComponentPtr script : scripts)
	{
		const std::filesystem::path scriptPath= script->getResolvedScriptPath();
		if (!m_scriptContext->loadBehavior(scriptPath, script->getComponentId(), script->getScriptDefinition().get()))
		{
			// The failing script disposed the whole state
			MIKAN_LOG_ERROR("ScriptObjectSystem::reloadAllScripts")
				<< "Script " << script->getComponentId() << " (" << scriptPath << ") failed; project scripts unloaded";
			m_scriptContext= nullptr;
			if (OnScriptsReloaded)
				OnScriptsReloaded(false);
			return;
		}
	}

	MikanServer::getInstance()->getScriptRequestHandler()->bindScriptContext(m_scriptContext);
	applyHttpRoutes();

	if (OnScriptsReloaded)
		OnScriptsReloaded(true);
}

void ScriptObjectSystem::onScriptError(const ScriptError& error)
{
	if (error.kind == eScriptErrorKind::load)
	{
		m_lastLoadErrors.push_back(error);
	}

	if (OnScriptError)
		OnScriptError(error);
}

void ScriptObjectSystem::disposeScriptContext()
{
	if (!m_scriptContext)
		return;

	// Drop the HTTP routes and message subscription before the context goes away
	MikanServer* mikanServer= MikanServer::getInstance();
	if (mikanServer && mikanServer->getScriptRequestHandler())
	{
		mikanServer->getScriptRequestHandler()->unbindScriptContext(m_scriptContext);
	}

	m_scriptContext= nullptr;
}

ScriptComponentPtr ScriptObjectSystem::addNewScript()
{
	return addNewObjectByTypedDefinition([](ScriptDefinitionPtr definition) { return true; });
}

// -- HTTP routes -----
void ScriptObjectSystem::applyHttpRoutes()
{
	MikanServer* mikanServer= MikanServer::getInstance();
	if (mikanServer && mikanServer->getScriptRequestHandler())
	{
		mikanServer->getScriptRequestHandler()->setHttpRoutes(getTypedDefinitionConst()->getHttpRoutes());
	}
}

bool ScriptObjectSystem::addHttpRoute(const ScriptHttpRoute& route)
{
	ScriptHttpRouteTable routes= getTypedDefinitionConst()->getHttpRoutes();
	if (!routes.addRoute(route))
		return false;

	getTypedDefinition()->setHttpRoutes(routes);
	return true;
}

bool ScriptObjectSystem::setHttpRoute(size_t index, const ScriptHttpRoute& route)
{
	ScriptHttpRouteTable routes= getTypedDefinitionConst()->getHttpRoutes();
	if (!routes.setRoute(index, route))
		return false;

	getTypedDefinition()->setHttpRoutes(routes);
	return true;
}

bool ScriptObjectSystem::removeHttpRoute(size_t index)
{
	ScriptHttpRouteTable routes= getTypedDefinitionConst()->getHttpRoutes();
	if (!routes.removeRoute(index))
		return false;

	getTypedDefinition()->setHttpRoutes(routes);
	return true;
}

bool ScriptObjectSystem::isHttpRouteResolved(const ScriptHttpRoute& route) const
{
	if (!m_scriptContext || route.scriptId == INVALID_MIKAN_ID || route.functionName.empty())
		return false;

	return m_scriptContext->behaviorHasMethod(route.scriptId,
											  CommonScriptContext::k_httpTriggerMethodPrefix + route.functionName);
}

// -- IPropertyInterface ----
void ScriptObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);

	// The route table as one JSON string: the HTTP Triggers panel draws the
	// routes itself, and the string form is what undo re-applies verbatim
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(ScriptObjectSystemDefinition::k_httpRoutesPropertyId,
																  MikanVariantType::STRING)
								 ->setUIHidden()
								 ->setClientAPIHidden());
}

bool ScriptObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == ScriptObjectSystemDefinition::k_httpRoutesPropertyId)
	{
		outValue= getTypedDefinitionConst()->getHttpRoutes().toJsonString();
		return true;
	}

	return Super::getPropertyValue(propertyName, outValue);
}

bool ScriptObjectSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == ScriptObjectSystemDefinition::k_httpRoutesPropertyId)
	{
		if (inValue.value_type != MikanVariantType::STRING)
			return false;

		// Malformed text is rejected without touching the table
		ScriptHttpRouteTable routes;
		if (!routes.fromJsonString(inValue.getUtf8Value()))
			return false;

		getTypedDefinition()->setHttpRoutes(routes);
		return true;
	}

	return Super::setPropertyValue(propertyName, inValue);
}
