//-- includes -----
#include "AppStage.h"
#include "LocText.h"
#include "ModalDialog_ScriptErrors.h"
#include "ScriptAssetReference.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

#include <algorithm>
#include <assert.h>

namespace
{
const char* kindLocKey(eScriptErrorKind kind)
{
	switch (kind)
	{
	case eScriptErrorKind::load:
		return "scriptErrors.kindLoad";
	case eScriptErrorKind::trigger:
		return "scriptErrors.kindTrigger";
	case eScriptErrorKind::httpTrigger:
		return "scriptErrors.kindHttpTrigger";
	case eScriptErrorKind::message:
		return "scriptErrors.kindMessage";
	case eScriptErrorKind::sequence:
		return "scriptErrors.kindSequence";
	case eScriptErrorKind::coroutine:
		return "scriptErrors.kindCoroutine";
	default:
		return "scriptErrors.kindLoad";
	}
}

bool sameError(const ScriptError& a, const ScriptError& b)
{
	return a.kind == b.kind && a.chunkName == b.chunkName && a.line == b.line && a.message == b.message
		   && a.context == b.context;
}
} // namespace

//-- public methods -----
ModalDialog_ScriptErrors::ModalDialog_ScriptErrors(AppStage* appStage)
	: ModalDialog(appStage)
{
}

ModalDialog_ScriptErrors* ModalDialog_ScriptErrors::show(AppStage* appStage, const ScriptError& error,
														 DismissCallback dismissCallback)
{
	ModalDialog_ScriptErrors* dialog= appStage->pushModalDialog<ModalDialog_ScriptErrors>();
	dialog->m_dismissCallback= dismissCallback;
	dialog->addError(error);

	return dialog;
}

void ModalDialog_ScriptErrors::addError(const ScriptError& error)
{
	const bool bListed= std::any_of(m_errors.begin(), m_errors.end(),
									[&error](const ScriptError& listed) { return sameError(listed, error); });
	if (!bListed)
	{
		m_errors.push_back(error);
	}
}

void ModalDialog_ScriptErrors::replaceErrors(const std::vector<ScriptError>& errors)
{
	m_errors.clear();
	for (const ScriptError& error : errors)
	{
		addError(error);
	}
}

bool ModalDialog_ScriptErrors::holdsOnlyLoadErrors() const
{
	return !m_errors.empty()
		   && std::all_of(m_errors.begin(), m_errors.end(),
						  [](const ScriptError& error) { return error.kind == eScriptErrorKind::load; });
}

void ModalDialog_ScriptErrors::onGui()
{
	static const char* k_popupId= "##ScriptErrorsModal";
	if (m_bNeedsOpen)
	{
		ImGui::OpenPopup(k_popupId);
		m_bNeedsOpen= false;
	}

	// A click on a location is applied once the popup has finished drawing,
	// so the editor launch never happens mid-frame
	if (m_pendingOpenIndex >= 0 && m_pendingOpenIndex < static_cast<int>(m_errors.size()))
	{
		const ScriptError& error= m_errors[m_pendingOpenIndex];
		ScriptAssetReference::openScriptInEditor(error.resolvedPath, error.line);
	}
	m_pendingOpenIndex= -1;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(480.f * MkGui::getUiScale(), 0.f),
										ImVec2(900.f * MkGui::getUiScale(), 600.f * MkGui::getUiScale()));
	if (ImGui::BeginPopupModal(k_popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted(locText("scriptErrors.title"));
		ImGui::Separator();

		for (size_t index= 0; index < m_errors.size(); ++index)
		{
			drawError(index, m_errors[index]);
		}

		ImGui::Spacing();
		bool bDismissed= false;
		if (ImGui::Button(locLabel("scriptErrors.close")))
		{
			ImGui::CloseCurrentPopup();
			bDismissed= true;
		}
		ImGui::EndPopup();

		if (bDismissed)
			onDismiss();
	}
}

void ModalDialog_ScriptErrors::drawError(size_t index, const ScriptError& error)
{
	ImGui::PushID(static_cast<int>(index));

	std::string heading= locText(kindLocKey(error.kind));
	if (!error.context.empty())
		heading+= " (" + error.context + ")";
	ImGui::TextUnformatted(heading.c_str());

	ImGui::Indent();
	ImGui::TextWrapped("%s", error.message.c_str());

	if (!error.chunkName.empty())
	{
		const std::string location= error.chunkName + ":" + std::to_string(error.line);
		if (error.hasLocation())
		{
			if (ImGui::SmallButton(location.c_str()))
			{
				m_pendingOpenIndex= static_cast<int>(index);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", locText("scriptErrors.openInEditor"));
			}
		}
		else
		{
			ImGui::TextDisabled("%s", location.c_str());
		}
	}

	if (!error.traceback.empty())
	{
		if (ImGui::TreeNode(locText("scriptErrors.traceback")))
		{
			ImGui::TextUnformatted(error.traceback.c_str());
			ImGui::TreePop();
		}
	}
	ImGui::Unindent();
	ImGui::Spacing();

	ImGui::PopID();
}

void ModalDialog_ScriptErrors::onDismiss()
{
	DismissCallback callback= std::move(m_dismissCallback);
	AppStage* ownerAppStage= m_ownerAppStage;

	assert(ownerAppStage->getCurrentModalDialog() == this);
	ownerAppStage->popModalDialog(); // deletes 'this'

	if (callback)
		callback();
}
