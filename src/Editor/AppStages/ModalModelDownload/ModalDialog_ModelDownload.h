#pragma once

//-- includes -----
#include "ModelCatalog.h"
#include "ModelDownloadTask.h"
#include "Shared/ModalDialog.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class AppStage;

//-- definitions -----

/// Asks whether to download the models a capture needs, then shows the
/// download running.
///
/// The prompt is where a license with use restrictions gets presented and
/// accepted, which is a condition of redistributing these weights rather than
/// a nicety, so the Download button stays disabled until the box is ticked.
class ModalDialog_ModelDownload : public ModalDialog
{
public:
	ModalDialog_ModelDownload(AppStage* ownerAppStage);
	virtual ~ModalDialog_ModelDownload();

	using CompletionCallback= std::function<void()>;

	/// Pushes the dialog for every listed model that is not installed.
	/// onInstalled fires once they all are, which is where the caller retries
	/// whatever needed them. Returns false when there was nothing to do, in
	/// which case no dialog is pushed and no callback fires.
	static bool requestModels(AppStage* appStage, const std::vector<eModelId>& models,
							  CompletionCallback onInstalled= {}, CompletionCallback onCancelled= {});

	virtual void onGui() override;

private:
	void drawPrompt();
	void drawProgress();
	void onStartDownload();
	void onCancel();
	void onFinished();

	std::vector<eModelId> m_models;
	CompletionCallback m_onInstalled;
	CompletionCallback m_onCancelled;

	std::unique_ptr<ModelDownloadTask> m_task;
	bool m_bLicenseAccepted= false;
	bool m_bDownloadStarted= false;
	std::string m_errorDetail;
};
