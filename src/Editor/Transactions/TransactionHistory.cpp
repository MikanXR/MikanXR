#include "TransactionHistory.h"
#include "AutomationProtocol.h"
#include "AutomationServer.h"
#include "AutomationVariantText.h"
#include "CommonConfig.h"
#include "InputManager.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanComponent.h"
#include "MikanObject.h"
#include "MikanObjectSystem.h"
#include "MikanPropertyDatabase.h"
#include "ProjectConfig.h"
#include "ProjectManager.h"
#include "PropertyInterface.h"

#include "Project/AppStage_Project.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

namespace
{
// A burst of changes to one target coalesces into one transaction while
// consecutive changes arrive within this window (a gesture bracket overrides)
static const float k_coalesceWindowSeconds= 0.75f;
} // namespace

TransactionHistory::TransactionHistory()= default;

TransactionHistory::~TransactionHistory() { shutdown(); }

bool TransactionHistory::startup(MainWindow* mainWindow)
{
	m_mainWindow= mainWindow;

	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	projectManager->OnProjectLoaded+= MakeDelegate(this, &TransactionHistory::onProjectLoaded);
	projectManager->OnProjectPreUnload+= MakeDelegate(this, &TransactionHistory::onProjectPreUnload);

	m_mainWindow->OnAppStageEntered+= MakeDelegate(this, &TransactionHistory::onAppStageEntered);

	// The initial project loads during ProjectManager startup, before this
	// subscription exists, so bind to it now
	if (projectManager->hasLoadedProject())
	{
		bindToProject();
	}

	return true;
}

void TransactionHistory::update(float deltaSeconds)
{
	m_secondsSinceLastAbsorb+= deltaSeconds;

	// A destroy composite closes at the frame boundary; a quiet property
	// burst seals once its coalescing window lapses. An active gesture
	// holds its transaction open until endGesture.
	if (m_compositeDepth > 0)
		return;

	if (m_bDestroyCompositeOpen
		|| (m_openTransaction && m_activeGestureId.empty() && m_secondsSinceLastAbsorb > k_coalesceWindowSeconds))
	{
		sealOpenTransaction();
	}
}

void TransactionHistory::shutdown()
{
	if (m_mainWindow != nullptr)
	{
		unbindFromProject();

		ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
		if (projectManager)
		{
			projectManager->OnProjectLoaded-= MakeDelegate(this, &TransactionHistory::onProjectLoaded);
			projectManager->OnProjectPreUnload-= MakeDelegate(this, &TransactionHistory::onProjectPreUnload);
		}

		m_mainWindow->OnAppStageEntered-= MakeDelegate(this, &TransactionHistory::onAppStageEntered);

		m_mainWindow= nullptr;
	}
}

// ---- Editor keybindings --------------------------------------------------------

void TransactionHistory::onAppStageEntered(AppStage* oldAppStage, AppStage* newAppStage)
{
	// Undo/redo bind while the project editing stage is up. The stage's
	// input binding set is destroyed on exit, so binding on every entry
	// is the whole lifecycle.
	if (newAppStage != nullptr && newAppStage->getAppStageName() == AppStage_Project::APP_STAGE_NAME)
	{
		InputManager* inputManager= m_mainWindow->getInputManager();

		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_z, MkKeyMod::CTRL)->OnKeyPressed+=
			MakeDelegate(this, &TransactionHistory::onUndoKeyPressed);
		inputManager->fetchOrAddKeyBindings(MkKey::LETTER_z, MkKeyMod::CTRL | MkKeyMod::SHIFT)->OnKeyPressed+=
			MakeDelegate(this, &TransactionHistory::onRedoKeyPressed);
	}
}

void TransactionHistory::onUndoKeyPressed() { undo(); }

void TransactionHistory::onRedoKeyPressed() { redo(); }

// ---- Project lifecycle ---------------------------------------------------------

void TransactionHistory::onProjectLoaded(ProjectManagerPtr projectManager) { bindToProject(); }

void TransactionHistory::onProjectPreUnload(ProjectManagerPtr projectManager) { unbindFromProject(); }

void TransactionHistory::bindToProject()
{
	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	ProjectConfigPtr projectConfig= projectManager->getProjectConfig();
	if (!projectConfig)
		return;

	projectConfig->OnPropertyChanged+= MakeDelegate(this, &TransactionHistory::onProjectConfigChanged);
	m_projectConfig= projectConfig;

	for (const MikanObjectSystemPtr& system : projectManager->getSystems())
	{
		system->OnNewObjectFinalized+= MakeDelegate(this, &TransactionHistory::onNewObjectFinalized);
		system->OnObjectWillBeDestroyed+= MakeDelegate(this, &TransactionHistory::onObjectWillBeDestroyed);
	}

	seedShadowMap();

	// Old transactions target the previous project's objects
	m_history.clear();
	m_cursor= 0;
	m_openTransaction= nullptr;
	m_activeGestureId.clear();
	m_warnedPropertyKeys.clear();

	m_logWriter.open(projectConfig->getLoadedConfigPath());
}

void TransactionHistory::unbindFromProject()
{
	sealOpenTransaction();

	ProjectConfigPtr projectConfig= m_projectConfig.lock();
	if (projectConfig)
	{
		projectConfig->OnPropertyChanged-= MakeDelegate(this, &TransactionHistory::onProjectConfigChanged);

		for (const MikanObjectSystemPtr& system : m_mainWindow->getProjectManager()->getSystems())
		{
			system->OnNewObjectFinalized-= MakeDelegate(this, &TransactionHistory::onNewObjectFinalized);
			system->OnObjectWillBeDestroyed-= MakeDelegate(this, &TransactionHistory::onObjectWillBeDestroyed);
		}
	}
	m_projectConfig.reset();

	m_logWriter.close();
	m_shadowValues.clear();
	m_history.clear();
	m_cursor= 0;
	m_openTransaction= nullptr;
	m_activeGestureId.clear();
}

std::string TransactionHistory::makeShadowKey(const std::string& systemName, int componentId,
											  const std::string& propertyName) const
{
	// Component ids are globally unique across systems; system-level
	// properties key on the system name instead
	if (componentId != -1)
	{
		return std::to_string(componentId) + "/" + propertyName;
	}
	return systemName + "/" + propertyName;
}

void TransactionHistory::seedShadowMap()
{
	m_shadowValues.clear();

	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	MikanPropertyDatabaseConstPtr propertyDatabase= projectManager->getPropertyDatabaseConst();

	for (const MikanPropertyEntry& entry : propertyDatabase->getAllProperties())
	{
		MikanObjectSystemPtr system= projectManager->getSystemByName(entry.systemName);
		if (!system)
			continue;

		const std::string& propertyName= entry.descriptor->getName();

		if (entry.componentClassName.empty())
		{
			MikanVariant value;
			if (system->getPropertyValue(propertyName, value))
			{
				m_shadowValues[makeShadowKey(entry.systemName, -1, propertyName)]=
					AutomationVariantText::variantToText(value);
			}
		}
		else
		{
			std::vector<int> componentIds;
			if (!system->getComponentIdList(entry.componentClassName, componentIds))
				continue;

			for (int componentId : componentIds)
			{
				if (ProjectConfig::isTransientComponentId(componentId))
					continue;

				MikanComponentPtr component= system->getComponentById(componentId);
				MikanVariant value;
				if (component && component->getPropertyValue(propertyName, value))
				{
					m_shadowValues[makeShadowKey(entry.systemName, componentId, propertyName)]=
						AutomationVariantText::variantToText(value);
				}
			}
		}
	}
}

// ---- Capture -------------------------------------------------------------------

void TransactionHistory::onProjectConfigChanged(CommonConfigPtr configPtr,
												const ConfigPropertyChangeSet& changedPropertySet)
{
	// Ignore the changes undo/redo application itself produces
	if (m_bApplyingTransaction)
		return;

	// The serialization vetoes gate only the autosave timer upstream;
	// apply them here to skip runtime-driven state (mount-driven camera
	// transforms, transient VR devices)
	if (!configPtr->wantsConfigSerialization() || !configPtr->wantsSaveForPropertyChange(changedPropertySet))
		return;

	// Resolve the mutated definition to its command target
	std::string systemName;
	std::string componentClassName;
	int componentId= -1;
	IPropertyInterface* propertyInterface= nullptr;
	MikanObjectSystemPtr ownerSystem;
	MikanComponentPtr ownerComponent;

	if (auto systemDefinition= std::dynamic_pointer_cast<MikanObjectSystemDefinition>(configPtr))
	{
		ownerSystem= systemDefinition->getOwnerSystem();
		if (!ownerSystem)
			return;

		systemName= ownerSystem->getObjectSystemClassName();
		propertyInterface= ownerSystem.get();
	}
	else if (auto componentDefinition= std::dynamic_pointer_cast<MikanComponentDefinition>(configPtr))
	{
		ownerComponent= componentDefinition->getOwnerComponent();
		if (!ownerComponent)
			return;

		MikanObjectPtr ownerObject= ownerComponent->getOwnerObject();
		if (!ownerObject)
			return;

		ownerSystem= ownerObject->getOwnerSystem();
		if (!ownerSystem)
			return;

		componentId= ownerComponent->getComponentId();
		if (ProjectConfig::isTransientComponentId(componentId))
			return;

		systemName= ownerSystem->getObjectSystemClassName();
		componentClassName= ownerComponent->getComponentClassName();
		propertyInterface= ownerComponent.get();
	}
	else
	{
		// Pool definitions and other intermediate configs carry no
		// addressable properties
		return;
	}

	for (const std::string& propertyName : changedPropertySet.getSet())
	{
		capturePropertyChange(systemName, componentClassName, componentId, propertyName, propertyInterface);
	}
}

void TransactionHistory::capturePropertyChange(const std::string& systemName, const std::string& componentClassName,
											   int componentId, const std::string& propertyName,
											   IPropertyInterface* propertyInterface)
{
	MikanPropertyDatabaseConstPtr propertyDatabase= m_mainWindow->getProjectManager()->getPropertyDatabaseConst();
	PropertyDescriptorConstPtr descriptor=
		propertyDatabase->findPropertyDescriptor(systemName, componentClassName, propertyName);
	if (!descriptor)
	{
		// A notified name with no descriptor cannot be recorded or undone.
		// Warn once per name; the notification guard test tracks these.
		const std::string warnKey= systemName + "/" + componentClassName + "/" + propertyName;
		if (m_warnedPropertyKeys.insert(warnKey).second)
		{
			MIKAN_LOG_WARNING("TransactionHistory") << "Unrecordable property change (no descriptor): " << warnKey;
		}
		return;
	}

	// A read-only property cannot be re-applied through setPropertyValue, so
	// recording it would only produce a failing op on undo (the typed systems'
	// component id lists notify on every create and destroy)
	if (descriptor->isReadOnly())
		return;

	MikanVariant newValue;
	if (!propertyInterface->getPropertyValue(propertyName, newValue))
		return;

	const std::string newValueText= AutomationVariantText::variantToText(newValue);
	const std::string shadowKey= makeShadowKey(systemName, componentId, propertyName);

	auto shadowIter= m_shadowValues.find(shadowKey);
	if (shadowIter == m_shadowValues.end())
	{
		// First observation of this property (e.g. a component created after
		// the seed): adopt the value without recording a transaction
		m_shadowValues[shadowKey]= newValueText;
		return;
	}

	if (shadowIter->second == newValueText)
		return;

	TransactionOp op;
	op.kind= eTransactionOpKind::setProperty;
	op.systemName= systemName;
	op.componentClassName= componentClassName;
	op.componentId= componentId;
	op.propertyName= propertyName;
	op.valueType= descriptor->getDataType();
	op.oldValueText= shadowIter->second;
	op.newValueText= newValueText;

	shadowIter->second= newValueText;

	absorbOp(std::move(op));
}

void TransactionHistory::onNewObjectFinalized(MikanObjectSystemPtr system, MikanObjectPtr object)
{
	if (m_bApplyingTransaction)
		return;

	// The primary component is the one bound to a definition; secondary
	// components (selection, colliders) have none
	MikanComponentPtr primaryComponent;
	for (const MikanComponentPtr& component : object->getComponentsConst())
	{
		if (component->getDefinition())
		{
			primaryComponent= component;
			break;
		}
	}
	if (!primaryComponent)
		return;

	const int componentId= primaryComponent->getComponentId();
	if (ProjectConfig::isTransientComponentId(componentId))
		return;

	TransactionOp op;
	op.kind= eTransactionOpKind::createObject;
	op.systemName= system->getObjectSystemClassName();
	op.componentClassName= primaryComponent->getComponentClassName();
	op.componentId= componentId;
	op.definitionConfig= primaryComponent->getDefinition()->writeToJSON();

	if (m_compositeDepth > 0)
	{
		// Joins the composite in order, after any destroy still pending
		flushPendingDestroyOp();
		if (!m_openTransaction)
			openNewTransaction();
		m_openTransaction->ops.push_back(std::move(op));
	}
	else
	{
		sealOpenTransaction();
		openNewTransaction();
		m_openTransaction->ops.push_back(std::move(op));
		sealOpenTransaction();
	}

	seedComponentShadow(system->getObjectSystemClassName(), primaryComponent->getComponentClassName(), componentId);
}

void TransactionHistory::onObjectWillBeDestroyed(MikanObjectSystemPtr system, MikanComponentPtr component)
{
	if (m_bApplyingTransaction)
		return;

	const int componentId= component->getComponentId();
	if (ProjectConfig::isTransientComponentId(componentId))
		return;

	// Snapshot the definition before dispose rewrites parenting on it.
	// The child reparent property changes dispose fires fold into this
	// composite; the destroy op is appended last at seal time. Inside a
	// composite bracket the previous destroy lands in the same transaction.
	if (m_compositeDepth > 0)
		flushPendingDestroyOp();
	else
		sealOpenTransaction();

	m_pendingDestroyOp= TransactionOp();
	m_pendingDestroyOp.kind= eTransactionOpKind::destroyObject;
	m_pendingDestroyOp.systemName= system->getObjectSystemClassName();
	m_pendingDestroyOp.componentClassName= component->getComponentClassName();
	m_pendingDestroyOp.componentId= componentId;
	m_pendingDestroyOp.definitionConfig= component->getDefinition()->writeToJSON();
	m_bDestroyCompositeOpen= true;

	eraseComponentShadow(componentId);
}

void TransactionHistory::openNewTransaction()
{
	m_openTransaction= std::make_shared<Transaction>();
	m_openTransaction->sequenceNumber= m_nextSequenceNumber++;
	m_openTransaction->timestampMs= TransactionLogWriter::nowEpochMs();
	m_openTransaction->gestureId= m_activeGestureId;
	m_secondsSinceLastAbsorb= 0.f;
}

void TransactionHistory::absorbOp(TransactionOp&& op)
{
	// While a destroy composite is open, everything the teardown cascade
	// touches (children reparenting to the grandparent) joins it. A composite
	// bracket takes every op the same way.
	if (m_bDestroyCompositeOpen || m_compositeDepth > 0)
	{
		if (!m_openTransaction)
			openNewTransaction();

		m_openTransaction->ops.push_back(std::move(op));
		m_secondsSinceLastAbsorb= 0.f;
		return;
	}

	if (m_openTransaction)
	{
		// Merge only while the burst stays on the same target and either the
		// gesture bracket is still open or the coalescing window has not lapsed
		const TransactionOp& firstOp= m_openTransaction->ops[0];
		const bool bSameTarget= firstOp.systemName == op.systemName && firstOp.componentId == op.componentId;
		const bool bGestureActive= !m_activeGestureId.empty() && m_openTransaction->gestureId == m_activeGestureId;
		const bool bInWindow= m_secondsSinceLastAbsorb <= k_coalesceWindowSeconds;

		if (bSameTarget && (bGestureActive || bInWindow))
		{
			// Same property: keep the first old value, take the latest new value
			for (TransactionOp& existingOp : m_openTransaction->ops)
			{
				if (existingOp.kind == eTransactionOpKind::setProperty && existingOp.propertyName == op.propertyName)
				{
					existingOp.newValueText= op.newValueText;
					m_secondsSinceLastAbsorb= 0.f;
					return;
				}
			}

			m_openTransaction->ops.push_back(std::move(op));
			m_secondsSinceLastAbsorb= 0.f;
			return;
		}

		sealOpenTransaction();
	}

	openNewTransaction();
	m_openTransaction->ops.push_back(std::move(op));
}

void TransactionHistory::flushPendingDestroyOp()
{
	// A destroy composite closes by appending its snapshot op LAST, so a
	// reverse-order undo recreates the object before restoring its children
	if (m_bDestroyCompositeOpen)
	{
		if (!m_openTransaction)
			openNewTransaction();

		m_openTransaction->ops.push_back(m_pendingDestroyOp);
		m_pendingDestroyOp= TransactionOp();
		m_bDestroyCompositeOpen= false;
	}
}

void TransactionHistory::sealOpenTransaction()
{
	flushPendingDestroyOp();

	if (!m_openTransaction)
		return;

	// Describe from the ops. Property bursts share one target; a composite
	// takes its name from the create/destroy op.
	const TransactionOp& firstOp= m_openTransaction->ops[0];
	const TransactionOp& lastOp= m_openTransaction->ops.back();
	if (lastOp.kind == eTransactionOpKind::destroyObject)
	{
		m_openTransaction->description=
			"destroy " + lastOp.componentClassName + "#" + std::to_string(lastOp.componentId);
		if (m_openTransaction->ops.size() > 1)
		{
			m_openTransaction->description+=
				" (+" + std::to_string(m_openTransaction->ops.size() - 1) + " child updates)";
		}
	}
	else if (firstOp.kind == eTransactionOpKind::createObject)
	{
		m_openTransaction->description=
			"create " + firstOp.componentClassName + "#" + std::to_string(firstOp.componentId);
	}
	else
	{
		std::string target= !firstOp.componentClassName.empty()
								? firstOp.componentClassName + "#" + std::to_string(firstOp.componentId)
								: firstOp.systemName;
		std::string propertyNames;
		for (const TransactionOp& op : m_openTransaction->ops)
		{
			if (!propertyNames.empty())
				propertyNames+= ",";
			propertyNames+= op.propertyName;
		}
		m_openTransaction->description= "set " + target + " " + propertyNames;
	}

	// Committing a new transaction discards the redo tail
	m_history.erase(m_history.begin() + m_cursor, m_history.end());
	m_history.push_back(m_openTransaction);
	if ((int)m_history.size() > k_maxHistoryDepth)
	{
		m_history.erase(m_history.begin());
	}
	m_cursor= m_history.size();

	m_logWriter.writeTransaction(*m_openTransaction);
	m_openTransaction= nullptr;
}

// ---- Gestures ------------------------------------------------------------------

void TransactionHistory::beginGesture(const std::string& gestureId)
{
	// Idempotent while the same gesture stays active, so per-frame callers
	// (an active ImGui widget) don't split their own transaction
	if (m_activeGestureId == gestureId)
		return;

	// A new gesture never merges into a previous burst
	sealOpenTransaction();
	m_activeGestureId= gestureId;
}

void TransactionHistory::endGesture()
{
	sealOpenTransaction();
	m_activeGestureId.clear();
}

void TransactionHistory::beginComposite()
{
	// A composite never merges into the burst before it
	if (m_compositeDepth == 0)
		sealOpenTransaction();

	++m_compositeDepth;
}

void TransactionHistory::endComposite()
{
	if (m_compositeDepth == 0)
		return;

	if (--m_compositeDepth == 0)
		sealOpenTransaction();
}

// ---- Undo / redo ---------------------------------------------------------------

bool TransactionHistory::undo()
{
	sealOpenTransaction();

	if (!canUndo())
		return false;

	const TransactionPtr transaction= m_history[m_cursor - 1];
	applyTransaction(*transaction, true);
	--m_cursor;

	m_logWriter.writeEvent("undo", transaction->sequenceNumber);
	return true;
}

bool TransactionHistory::redo()
{
	sealOpenTransaction();

	if (!canRedo())
		return false;

	const TransactionPtr transaction= m_history[m_cursor];
	applyTransaction(*transaction, false);
	++m_cursor;

	m_logWriter.writeEvent("redo", transaction->sequenceNumber);
	return true;
}

void TransactionHistory::clearHistory()
{
	sealOpenTransaction();
	m_history.clear();
	m_cursor= 0;
	m_logWriter.writeEvent("clear");
}

bool TransactionHistory::applyTransaction(const Transaction& transaction, bool bUndo)
{
	m_bApplyingTransaction= true;

	bool bSuccess= true;
	if (bUndo)
	{
		for (auto opIter= transaction.ops.rbegin(); opIter != transaction.ops.rend(); ++opIter)
		{
			bSuccess&= applyOp(*opIter, bUndo);
		}
	}
	else
	{
		for (const TransactionOp& op : transaction.ops)
		{
			bSuccess&= applyOp(op, bUndo);
		}
	}

	m_bApplyingTransaction= false;

	if (!bSuccess)
	{
		MIKAN_LOG_WARNING("TransactionHistory")
			<< "Transaction " << transaction.sequenceNumber << (bUndo ? " undo" : " redo") << " partially failed";
	}

	return bSuccess;
}

bool TransactionHistory::applyOp(const TransactionOp& op, bool bUndo)
{
	switch (op.kind)
	{
	case eTransactionOpKind::setProperty:
		return applySetPropertyOp(op, bUndo);
	case eTransactionOpKind::createObject:
		// Undoing a create destroys; redoing recreates
		return bUndo ? destroyObjectForOp(op) : recreateObjectForOp(op);
	case eTransactionOpKind::destroyObject:
		return bUndo ? recreateObjectForOp(op) : destroyObjectForOp(op);
	default:
		return false;
	}
}

bool TransactionHistory::recreateObjectForOp(const TransactionOp& op)
{
	MikanObjectSystemPtr system= m_mainWindow->getProjectManager()->getSystemByName(op.systemName);
	if (!system)
		return false;

	MikanComponentPtr component= system->recreateObjectFromDefinitionJson(op.definitionConfig);
	if (!component)
	{
		MIKAN_LOG_WARNING("TransactionHistory")
			<< "Failed to recreate " << op.componentClassName << "#" << op.componentId;
		return false;
	}

	seedComponentShadow(op.systemName, op.componentClassName, op.componentId);
	return true;
}

bool TransactionHistory::destroyObjectForOp(const TransactionOp& op)
{
	MikanObjectSystemPtr system= m_mainWindow->getProjectManager()->getSystemByName(op.systemName);
	if (!system)
		return false;

	MikanComponentPtr component= system->getComponentById(op.componentId);
	if (!component)
		return false;

	if (!component->destroyOwnerObject())
		return false;

	eraseComponentShadow(op.componentId);
	return true;
}

void TransactionHistory::seedComponentShadow(const std::string& systemName, const std::string& componentClassName,
											 int componentId)
{
	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	MikanObjectSystemPtr system= projectManager->getSystemByName(systemName);
	if (!system)
		return;

	MikanComponentPtr component= system->getComponentById(componentId);
	if (!component)
		return;

	MikanPropertyDatabaseConstPtr propertyDatabase= projectManager->getPropertyDatabaseConst();
	for (const MikanPropertyEntry& entry : propertyDatabase->getAllProperties())
	{
		if (entry.systemName != systemName || entry.componentClassName != componentClassName)
			continue;

		const std::string& propertyName= entry.descriptor->getName();
		MikanVariant value;
		if (component->getPropertyValue(propertyName, value))
		{
			m_shadowValues[makeShadowKey(systemName, componentId, propertyName)]=
				AutomationVariantText::variantToText(value);
		}
	}
}

void TransactionHistory::eraseComponentShadow(int componentId)
{
	const std::string keyPrefix= std::to_string(componentId) + "/";
	for (auto it= m_shadowValues.begin(); it != m_shadowValues.end();)
	{
		if (it->first.rfind(keyPrefix, 0) == 0)
			it= m_shadowValues.erase(it);
		else
			++it;
	}
}

bool TransactionHistory::applySetPropertyOp(const TransactionOp& op, bool bUndo)
{
	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	MikanObjectSystemPtr system= projectManager->getSystemByName(op.systemName);
	if (!system)
		return false;

	IPropertyInterface* propertyInterface= system.get();
	MikanComponentPtr component;
	if (op.componentId != -1)
	{
		component= system->getComponentById(op.componentId);
		if (!component)
			return false;
		propertyInterface= component.get();
	}

	const std::string& valueText= bUndo ? op.oldValueText : op.newValueText;

	// Strings apply verbatim (they may contain spaces); everything else
	// tokenizes on whitespace for the multi-component math types
	std::vector<std::string> valueTokens;
	if (op.valueType == MikanVariantType::STRING)
	{
		valueTokens.push_back(valueText);
	}
	else
	{
		std::string parseError;
		if (!AutomationProtocol::tokenizeCommandLine(valueText, valueTokens, parseError))
			return false;
	}

	MikanVariant value;
	std::string coerceError;
	if (!AutomationVariantText::textToVariant(op.valueType, valueTokens, value, coerceError))
	{
		MIKAN_LOG_WARNING("TransactionHistory")
			<< "Failed to parse stored value for " << op.propertyName << ": " << coerceError;
		return false;
	}

	if (!propertyInterface->setPropertyValue(op.propertyName, value))
	{
		MIKAN_LOG_WARNING("TransactionHistory")
			<< "Failed to apply " << (bUndo ? "undo" : "redo") << " of " << op.propertyName;
		return false;
	}

	m_shadowValues[makeShadowKey(op.systemName, op.componentId, op.propertyName)]= valueText;
	return true;
}

// ---- Automation ----------------------------------------------------------------

void TransactionHistory::registerAutomationCommands(AutomationServer* automationServer)
{
	using namespace std::placeholders;

	automationServer->registerCommandNamespace(
		"history", {"history list [n]", "history info", "history undo [n]", "history redo [n]", "history clear"},
		std::bind(&TransactionHistory::handleHistoryCommand, this, _1, _2, _3));
}

bool TransactionHistory::handleHistoryCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											  std::string& outError)
{
	// Any history command is a coalescing boundary
	sealOpenTransaction();

	if (args.empty())
	{
		outError= "usage: history list|info|undo|redo|clear";
		return false;
	}

	const std::string& verb= args[0];

	if (verb == "list")
	{
		int lineCount= 10;
		if (args.size() >= 2)
			lineCount= atoi(args[1].c_str());

		const int startIndex= std::max(0, (int)m_history.size() - lineCount);
		for (int i= startIndex; i < (int)m_history.size(); ++i)
		{
			const TransactionPtr& transaction= m_history[i];
			outLines.push_back(std::to_string(transaction->sequenceNumber) + " "
							   + ((size_t)i < m_cursor ? "applied" : "undone") + " " + transaction->description);
		}
		return true;
	}
	else if (verb == "info")
	{
		outLines.push_back("depth " + std::to_string(m_history.size()));
		outLines.push_back("cursor " + std::to_string(m_cursor));
		outLines.push_back(std::string("can_undo ") + (canUndo() ? "true" : "false"));
		outLines.push_back(std::string("can_redo ") + (canRedo() ? "true" : "false"));
		outLines.push_back(std::string("gesture ") + (m_activeGestureId.empty() ? "none" : m_activeGestureId));
		outLines.push_back("log " + m_logWriter.getLogFilePath().string());
		return true;
	}
	else if (verb == "undo" || verb == "redo")
	{
		int stepCount= 1;
		if (args.size() >= 2)
			stepCount= atoi(args[1].c_str());
		if (stepCount <= 0)
		{
			outError= "invalid step count";
			return false;
		}

		int stepsTaken= 0;
		for (int i= 0; i < stepCount; ++i)
		{
			if (!(verb == "undo" ? undo() : redo()))
				break;
			++stepsTaken;
		}

		if (stepsTaken == 0)
		{
			outError= verb == "undo" ? "nothing to undo" : "nothing to redo";
			return false;
		}

		outLines.push_back("cursor " + std::to_string(m_cursor));
		return true;
	}
	else if (verb == "clear")
	{
		clearHistory();
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}
