#include "ScriptObjectSystem.h"
#include "Logger.h"
#include "MikanServer.h"
#include "PathUtils.h"
#include "ProjectScriptContext.h"
#include "ScriptRequestHandler.h"

#include <fstream>

// -- ScriptObjectSystemDefinition -----
ScriptObjectSystemDefinition::ScriptObjectSystemDefinition(const std::string& configName,
														   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

// -- ScriptObjectSystem ----
ScriptObjectSystem::ScriptObjectSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
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
		return;

	m_scriptContext= std::make_shared<ProjectScriptContext>(getOwnerProjectManager());
	if (!m_scriptContext->createScriptState())
	{
		m_scriptContext= nullptr;
		return;
	}

	for (ScriptComponentPtr script : scripts)
	{
		const std::filesystem::path scriptPath= script->getResolvedScriptPath();
		if (!m_scriptContext->runScriptFile(scriptPath, script->getComponentId(), script->getScriptDefinition().get()))
		{
			// The failing chunk disposed the whole state
			MIKAN_LOG_ERROR("ScriptObjectSystem::reloadAllScripts")
				<< "Script " << script->getComponentId() << " (" << scriptPath << ") failed; project scripts unloaded";
			m_scriptContext= nullptr;
			return;
		}
	}

	MikanServer::getInstance()->getScriptRequestHandler()->bindScriptContext(m_scriptContext);
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
	return addNewObjectByTypedDefinition(
		[](ScriptDefinitionPtr definition)
		{
			return true;
		});
}
