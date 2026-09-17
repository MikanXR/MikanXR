//-- includes -----
#include "AppStage.h"
#include "LocText.h"
#include "ModalDialog_ModelDownload.h"
#include "ModelRepository.h"
#include "OSUtils.h"

#include "imgui.h"

#include <assert.h>

namespace
{
constexpr float k_gigabyte= 1.0e9f;

float toGigabytes(uint64_t bytes) { return (float)bytes / k_gigabyte; }

const char* getPhaseLabel(eModelDownloadPhase phase)
{
	switch (phase)
	{
	case eModelDownloadPhase::checkingDiskSpace:
		return locText("modelDownload.phaseCheckingDisk");
	case eModelDownloadPhase::fetchingManifest:
		return locText("modelDownload.phaseFetchingManifest");
	case eModelDownloadPhase::verifying:
		return locText("modelDownload.phaseVerifying");
	case eModelDownloadPhase::assembling:
		return locText("modelDownload.phaseAssembling");
	default:
		return locText("modelDownload.phaseDownloading");
	}
}
} // namespace

//-- public methods -----
ModalDialog_ModelDownload::ModalDialog_ModelDownload(AppStage* appStage)
	: ModalDialog(appStage)
{
}

ModalDialog_ModelDownload::~ModalDialog_ModelDownload()
{
	// Cancels and joins whatever is in flight, so tearing the stage down
	// while a download runs is safe.
	m_task.reset();
}

bool ModalDialog_ModelDownload::requestModels(AppStage* appStage, const std::vector<eModelId>& models,
											  CompletionCallback onInstalled, CompletionCallback onCancelled)
{
	std::vector<eModelId> missing;
	for (const eModelId id : models)
	{
		if (!ModelRepository::isModelInstalled(id))
			missing.push_back(id);
	}

	if (missing.empty())
		return false;

	ModalDialog_ModelDownload* dialog= appStage->pushModalDialog<ModalDialog_ModelDownload>();
	dialog->m_models= missing;
	dialog->m_onInstalled= onInstalled;
	dialog->m_onCancelled= onCancelled;

	return true;
}

void ModalDialog_ModelDownload::onGui()
{
	static const char* k_popupId= "##ModelDownloadModal";
	if (m_bNeedsOpen)
	{
		ImGui::OpenPopup(k_popupId);
		m_bNeedsOpen= false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(k_popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted(locText("modelDownload.title"));
		ImGui::Separator();

		if (m_bDownloadStarted)
			drawProgress();
		else
			drawPrompt();

		ImGui::EndPopup();
	}
}

void ModalDialog_ModelDownload::drawPrompt()
{
	ImGui::TextWrapped("%s", locText("modelDownload.intro"));
	ImGui::Spacing();

	bool bNeedsAcceptance= false;
	std::string acceptanceLicenseName;

	for (const eModelId id : m_models)
	{
		const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
		if (entry == nullptr)
			continue;

		ImGui::Bullet();
		ImGui::TextUnformatted(locText(entry->displayNameLocKey.c_str()));
		ImGui::Indent();
		ImGui::TextWrapped("%s", locText(entry->descriptionLocKey.c_str()));
		ImGui::TextUnformatted(locFormat("modelDownload.sizeFmt", toGigabytes(entry->approxDownloadBytes)).c_str());
		ImGui::TextUnformatted(locFormat("modelDownload.licenseFmt", entry->licenseName.c_str()).c_str());

		if (entry->bRequiresLicenseAcceptance)
		{
			bNeedsAcceptance= true;
			acceptanceLicenseName= entry->licenseName;

			ImGui::TextWrapped("%s", locText("modelDownload.restrictionsNotice"));
			if (!entry->licenseUrl.empty())
			{
				ImGui::PushID(entry->name.c_str());
				if (ImGui::Button(locLabel("modelDownload.viewLicense")))
					OSUtils::openUrl(entry->licenseUrl);
				ImGui::PopID();
			}
		}
		ImGui::Unindent();
		ImGui::Spacing();
	}

	ImGui::TextUnformatted(
		locFormat("modelDownload.destinationFmt", ModelRepository::getUserModelsRoot().string().c_str()).c_str());
	ImGui::Spacing();

	if (bNeedsAcceptance)
	{
		// The label carries the license name, so it is assembled the way the
		// localization manager assembles one: visible text, then "##key" to
		// keep the ImGui ID off the translation.
		const std::string acceptLabel= locFormat("modelDownload.acceptLicenseFmt", acceptanceLicenseName.c_str())
									   + "##modelDownload.acceptLicense";
		ImGui::Checkbox(acceptLabel.c_str(), &m_bLicenseAccepted);
	}

	ImGui::Separator();

	const bool bCanDownload= !bNeedsAcceptance || m_bLicenseAccepted;
	ImGui::BeginDisabled(!bCanDownload);
	const bool bDownloadPressed= ImGui::Button(locLabel("modelDownload.download"));
	ImGui::EndDisabled();

	ImGui::SameLine();
	const bool bCancelPressed= ImGui::Button(locLabel("modelDownload.cancel"));

	if (bDownloadPressed && bCanDownload)
		onStartDownload();
	else if (bCancelPressed)
		onCancel();
}

void ModalDialog_ModelDownload::drawProgress()
{
	assert(m_task);
	const ModelDownloadTask::Status status= m_task->getStatus();

	if (status.bFinished)
	{
		if (status.bSucceeded)
		{
			// Nothing to show: the capture the operator asked for runs now.
			onFinished();
			return;
		}

		ImGui::TextWrapped("%s", locText(status.bCancelled ? "modelDownload.cancelled" : "modelDownload.failed"));
		if (!status.errorDetail.empty())
			ImGui::TextWrapped("%s", status.errorDetail.c_str());

		ImGui::Separator();
		if (ImGui::Button(locLabel("modelDownload.close")))
			onCancel();

		return;
	}

	const ModelCatalogEntry* entry= ModelCatalog::findEntry(status.currentModel);
	if (entry != nullptr)
		ImGui::TextUnformatted(locText(entry->displayNameLocKey.c_str()));

	ImGui::TextUnformatted(getPhaseLabel(status.phase));

	if (!status.currentFile.empty())
	{
		ImGui::TextUnformatted(locFormat("modelDownload.currentFileFmt", status.currentFile.c_str(),
										 toGigabytes(status.currentFileReceivedBytes),
										 toGigabytes(status.currentFileTotalBytes))
								   .c_str());
	}

	const float fraction= status.overallTotalBytes > 0
							  ? (float)((double)status.overallReceivedBytes / (double)status.overallTotalBytes)
							  : 0.f;
	ImGui::ProgressBar(fraction, ImVec2(360.f, 0.f));
	ImGui::TextUnformatted(locFormat("modelDownload.overallFmt", toGigabytes(status.overallReceivedBytes),
									 toGigabytes(status.overallTotalBytes))
							   .c_str());

	ImGui::Separator();
	if (ImGui::Button(locLabel("modelDownload.cancel")))
	{
		// Only signals: the dialog stays up until the worker unwinds, which
		// is also what keeps the stage from being torn down underneath it.
		m_task->cancelAndJoin();
	}
}

void ModalDialog_ModelDownload::onStartDownload()
{
	m_task= std::make_unique<ModelDownloadTask>();
	m_task->start(m_models);
	m_bDownloadStarted= true;
}

void ModalDialog_ModelDownload::onCancel()
{
	CompletionCallback callback= std::move(m_onCancelled);
	AppStage* ownerAppStage= m_ownerAppStage;

	ImGui::CloseCurrentPopup();

	assert(ownerAppStage->getCurrentModalDialog() == this);
	ownerAppStage->popModalDialog(); // deletes 'this', which joins the task

	if (callback)
		callback();
}

void ModalDialog_ModelDownload::onFinished()
{
	CompletionCallback callback= std::move(m_onInstalled);
	AppStage* ownerAppStage= m_ownerAppStage;

	ImGui::CloseCurrentPopup();

	assert(ownerAppStage->getCurrentModalDialog() == this);
	ownerAppStage->popModalDialog(); // deletes 'this'

	if (callback)
		callback();
}
