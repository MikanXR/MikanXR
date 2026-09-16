#pragma once

//-- includes -----
#include "ScriptError.h"
#include "Shared/ModalDialog.h"

#include <functional>
#include <string>
#include <vector>

class AppStage;

//-- definitions -----
// Lists the Lua errors the project scripts raised. An entry whose file and
// line resolved opens the code editor there. One dialog holds every error
// reported while it is open, so a burst of errors does not stack dialogs.
class ModalDialog_ScriptErrors : public ModalDialog
{
public:
	ModalDialog_ScriptErrors(AppStage* ownerAppStage);
	virtual ~ModalDialog_ScriptErrors()= default;

	using DismissCallback= std::function<void()>;
	// Pushes the dialog with one entry; the instance is returned so later
	// errors can be appended while it stays open
	static ModalDialog_ScriptErrors* show(AppStage* appStage, const ScriptError& error,
										  DismissCallback dismissCallback= {});

	// Appends, unless an identical message at the same location is already listed
	void addError(const ScriptError& error);
	void replaceErrors(const std::vector<ScriptError>& errors);
	bool holdsOnlyLoadErrors() const;
	inline size_t getErrorCount() const { return m_errors.size(); }

	virtual void onGui() override;

protected:
	void drawError(size_t index, const ScriptError& error);
	void onDismiss();

	std::vector<ScriptError> m_errors;
	DismissCallback m_dismissCallback;
	// The entry whose location was clicked, opened after the popup has drawn
	int m_pendingOpenIndex= -1;
};
