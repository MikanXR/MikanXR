#include "AppStage.h"
#include "LightEnvironmentComponent.h"
#include "LocText.h"
#include "Shared/GuiPanel_LightEnvironmentComponent.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

#include <cmath>

// Band each SH coefficient belongs to, for labelling. Order-2 real SH: one
// constant term, three linear, five quadratic.
static const char* k_shCoefficientLabels[k_shCoefficientCount]= {
	"L0  (ambient)", "L1  Y", "L1  Z", "L1  X", "L2  XY", "L2  YZ", "L2  3Z2-1", "L2  XZ", "L2  X2-Y2"};

bool GuiPanel_LightEnvironmentComponent::init() { return initTypedPropertyInterface<LightEnvironmentComponent>(); }

void GuiPanel_LightEnvironmentComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		LightEnvironmentDefinition::k_shCoefficientsPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			LightEnvironmentComponentPtr component= getLightEnvironmentComponent();
			if (!component)
				return false;

			const SHLightingEnvironment environment= component->getScaledLightingEnvironment();

			ImGui::TextUnformatted(locText("componentPanel.sphericalHarmonics"));

			// The one coefficient that is a color in its own right: the l=0
			// term convolved for Lambertian response is the ambient the scene
			// receives, so show it as the headline swatch.
			const glm::vec3 ambient= environment.evalIrradiance(glm::vec3(0.f, 1.f, 0.f));
			ImGui::ColorButton(
				"##shAmbient",
				ImVec4(std::fmin(std::fmax(ambient.r, 0.f), 1.f), std::fmin(std::fmax(ambient.g, 0.f), 1.f),
					   std::fmin(std::fmax(ambient.b, 0.f), 1.f), 1.f),
				ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
				ImVec2(24.f * MkGui::getUiScale(), 24.f * MkGui::getUiScale()));
			ImGui::SameLine();
			ImGui::Text(locText("componentPanel.skyIrradianceFmt"), ambient.r, ambient.g, ambient.b);

			// The raw coefficients are signed radiance, not colors: the l=1 and
			// l=2 bands are routinely negative, and a swatch cannot show a
			// negative channel. So the swatch shows each coefficient's
			// magnitude relative to the largest one present, and the numbers
			// beside it carry the sign that the swatch cannot.
			float peakMagnitude= 0.f;
			for (int i= 0; i < k_shCoefficientCount; ++i)
			{
				const glm::vec3& c= environment.coefficients[i];
				peakMagnitude=
					std::fmax(peakMagnitude, std::fmax(std::fabs(c.r), std::fmax(std::fabs(c.g), std::fabs(c.b))));
			}
			const float normalizeScale= (peakMagnitude > 1e-6f) ? 1.f / peakMagnitude : 0.f;

			for (int i= 0; i < k_shCoefficientCount; ++i)
			{
				const glm::vec3& coefficient= environment.coefficients[i];
				const ImVec4 swatch(std::fabs(coefficient.r) * normalizeScale,
									std::fabs(coefficient.g) * normalizeScale,
									std::fabs(coefficient.b) * normalizeScale, 1.f);

				ImGui::PushID(i);
				ImGui::ColorButton("##shCoefficient", swatch,
								   ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker
									   | ImGuiColorEditFlags_NoTooltip,
								   ImVec2(18.f * MkGui::getUiScale(), 18.f * MkGui::getUiScale()));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip(locText("componentPanel.shCoefficientTooltipFmt"), k_shCoefficientLabels[i],
									  coefficient.r, coefficient.g, coefficient.b);
				}
				ImGui::SameLine();
				ImGui::Text("%-11s %+.4f %+.4f %+.4f", k_shCoefficientLabels[i], coefficient.r, coefficient.g,
							coefficient.b);
				ImGui::PopID();
			}

			ImGui::TextDisabled("%s", locText("componentPanel.exposureScaleNote"));

			return true;
		});

	// Directionality is the confidence signal for the whole estimate, so it is
	// called out rather than left as a bare float - the same amber warning the
	// capture tool shows, for the same reason.
	m_entityAccessor->setPropertyRenderer(
		LightEnvironmentDefinition::k_directionalityPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			LightEnvironmentComponentPtr component= getLightEnvironmentComponent();
			if (!component)
				return false;

			const float directionality= component->getLightEnvironmentDefinition()->getDirectionality();
			constexpr float k_lowDirectionalityThreshold= 0.25f;

			if (directionality < k_lowDirectionalityThreshold)
			{
				ImGui::TextColored(ImVec4(1.f, 0.7f, 0.f, 1.f), locText("componentPanel.directionalityNearAmbientFmt"),
								   directionality);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", locText("componentPanel.directionalityLowTooltip"));
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), locText("componentPanel.directionalityFmt"),
								   directionality);
			}

			return true;
		});
}

LightEnvironmentComponentPtr GuiPanel_LightEnvironmentComponent::getLightEnvironmentComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
		return std::static_pointer_cast<LightEnvironmentComponent>(component);
	return nullptr;
}
