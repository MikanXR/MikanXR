#include "LogPanel.h"
#include "AutomationLogBuffer.h"
#include "LocText.h"
#include "Logger.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

#include <vector>

LogPanel& LogPanel::getInstance()
{
	static LogPanel instance;
	return instance;
}

void LogPanel::draw(bool* pOpen)
{
	if (!ImGui::Begin(locWindowTitle("windows.log"), pOpen))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(locLabel("logPanel.clear")))
	{
		AutomationLogBuffer::clear();
	}
	ImGui::SameLine();
	ImGui::Checkbox(locLabel("logPanel.autoScroll"), &m_bAutoScroll);
	ImGui::SameLine();

	const char* levelLabels[]= {locText("logPanel.levelTrace"), locText("logPanel.levelDebug"),
								locText("logPanel.levelInfo"),  locText("logPanel.levelWarning"),
								locText("logPanel.levelError"), locText("logPanel.levelFatal")};
	ImGui::SetNextItemWidth(140.f * MkGui::getUiScale());
	ImGui::Combo(locLabel("logPanel.minLevel"), &m_minLevel, levelLabels, IM_ARRAYSIZE(levelLabels));

	ImGui::Separator();
	ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

	std::vector<AutomationLogBuffer::LeveledLine> lines;
	AutomationLogBuffer::getLines(m_minLevel, lines);

	for (const AutomationLogBuffer::LeveledLine& line : lines)
	{
		ImVec4 color(0.8f, 0.8f, 0.8f, 1.f);
		switch ((LogSeverityLevel)line.level)
		{
		case LogSeverityLevel::warning:
			color= ImVec4(1.f, 0.85f, 0.3f, 1.f);
			break;
		case LogSeverityLevel::error:
		case LogSeverityLevel::fatal:
			color= ImVec4(1.f, 0.4f, 0.4f, 1.f);
			break;
		case LogSeverityLevel::debug:
		case LogSeverityLevel::trace:
			color= ImVec4(0.6f, 0.6f, 0.6f, 1.f);
			break;
		default:
			break;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(line.text.c_str());
		ImGui::PopStyleColor();
	}

	// Only stick to the bottom while the view is already there, so scrolling
	// back to read something is not yanked away by the next log line
	if (m_bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
	{
		ImGui::SetScrollHereY(1.f);
	}

	ImGui::EndChild();
	ImGui::End();
}
