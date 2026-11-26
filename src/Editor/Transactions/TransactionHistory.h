#pragma once

#include "CommonConfigFwd.h"
#include "ComponentFwd.h"
#include "ObjectFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "TransactionLogWriter.h"
#include "TransactionTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

/// The editor transaction system: records every persistent definition
/// mutation as an undoable transaction, keeps an undo/redo stack, appends a
/// JSONL session log for post-session diagnosis, and serves the automation
/// server's history namespace.
///
/// Capture attaches to ProjectConfig::OnPropertyChanged (every definition
/// change bubbles there with the leaf config pointer), so no write site
/// needs to know about recording. Old values come from a shadow value map
/// maintained alongside the recording.
class TransactionHistory
{
public:
	static const int k_maxHistoryDepth= 256;

	TransactionHistory();
	~TransactionHistory();

	// Non-copyable
	TransactionHistory(const TransactionHistory&)= delete;
	TransactionHistory& operator=(const TransactionHistory&)= delete;

	bool startup(class MainWindow* mainWindow);
	void update(float deltaSeconds);
	void shutdown();

	/// Register the history command namespace on the automation server.
	void registerAutomationCommands(class AutomationServer* automationServer);

	/// Group all changes captured between the brackets into one transaction
	/// (a gizmo drag, a slider drag). Nesting is not supported; a second
	/// begin replaces the active gesture id.
	void beginGesture(const std::string& gestureId);
	void endGesture();

	/// Group every create, destroy, and property change captured between the
	/// brackets into one transaction, in the order they happen, so an object
	/// and the objects it owns are created or destroyed as one undo step.
	/// Brackets nest; the transaction seals when the outermost one ends.
	void beginComposite();
	void endComposite();

	bool canUndo() const { return m_cursor > 0; }
	bool canRedo() const { return m_cursor < m_history.size(); }
	bool undo();
	bool redo();
	void clearHistory();

private:
	// Editor keybindings (bound per Project stage entry; the stage's input
	// binding set is destroyed on exit, so no unbind is needed)
	void onAppStageEntered(class AppStage* oldAppStage, class AppStage* newAppStage);
	void onUndoKeyPressed();
	void onRedoKeyPressed();

	// Project lifecycle
	void onProjectLoaded(ProjectManagerPtr projectManager);
	void onProjectPreUnload(ProjectManagerPtr projectManager);
	void bindToProject();
	void unbindFromProject();
	void seedShadowMap();

	// Capture
	void onProjectConfigChanged(CommonConfigPtr configPtr, const class ConfigPropertyChangeSet& changedPropertySet);
	void capturePropertyChange(const std::string& systemName, const std::string& componentClassName, int componentId,
							   const std::string& propertyName, class IPropertyInterface* propertyInterface);
	void onNewObjectFinalized(MikanObjectSystemPtr system, MikanObjectPtr object);
	void onObjectWillBeDestroyed(MikanObjectSystemPtr system, MikanComponentPtr component);
	void openNewTransaction();
	void absorbOp(TransactionOp&& op);
	void flushPendingDestroyOp();
	void sealOpenTransaction();

	// Application
	bool applyTransaction(const Transaction& transaction, bool bUndo);
	bool applyOp(const TransactionOp& op, bool bUndo);
	bool applySetPropertyOp(const TransactionOp& op, bool bUndo);
	bool recreateObjectForOp(const TransactionOp& op);
	bool destroyObjectForOp(const TransactionOp& op);

	// Shadow maintenance for created/destroyed components
	void seedComponentShadow(const std::string& systemName, const std::string& componentClassName, int componentId);
	void eraseComponentShadow(int componentId);

	// Shadow values, keyed by component id (globally unique) or system name
	std::string makeShadowKey(const std::string& systemName, int componentId, const std::string& propertyName) const;

	// Automation
	bool handleHistoryCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
							  std::string& outError);

	class MainWindow* m_mainWindow= nullptr;
	ProjectConfigWeakPtr m_projectConfig;
	TransactionLogWriter m_logWriter;

	std::map<std::string, std::string> m_shadowValues; // shadow key -> value text

	// The open (unsealed) transaction absorbing this gesture/burst
	TransactionPtr m_openTransaction;
	float m_secondsSinceLastAbsorb= 0.f;
	std::string m_activeGestureId;

	// Open beginComposite brackets. While any is open, nothing seals.
	int m_compositeDepth= 0;

	// A destroy in flight: the pre-destroy snapshot appended LAST at seal so
	// reverse-order undo recreates the object before restoring its children
	bool m_bDestroyCompositeOpen= false;
	TransactionOp m_pendingDestroyOp;

	// Sealed history: m_history[0..m_cursor) are applied
	std::vector<TransactionPtr> m_history;
	size_t m_cursor= 0;
	int64_t m_nextSequenceNumber= 1;

	// Suppresses capture while undo/redo re-applies values
	bool m_bApplyingTransaction= false;

	// Once-per-name warnings for change-set names with no property descriptor
	std::set<std::string> m_warnedPropertyKeys;
};

/// Brackets a composite for the life of the scope. A null history (the
/// headless command line has none) makes it a no-op.
class TransactionCompositeScope
{
public:
	explicit TransactionCompositeScope(TransactionHistory* transactionHistory)
		: m_transactionHistory(transactionHistory)
	{
		if (m_transactionHistory)
			m_transactionHistory->beginComposite();
	}
	~TransactionCompositeScope()
	{
		if (m_transactionHistory)
			m_transactionHistory->endComposite();
	}

	TransactionCompositeScope(const TransactionCompositeScope&)= delete;
	TransactionCompositeScope& operator=(const TransactionCompositeScope&)= delete;

private:
	TransactionHistory* m_transactionHistory;
};
