#include "DepthPreviewRenderer.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanShaderCache.h"
#include "MkGuiDrawUtils.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "StringUtils.h"

#include "Graphs/NodeEvaluator.h"

#include <algorithm>

// -- DepthPreviewSettings -----
void DepthPreviewSettings::writeToJSON(configuru::Config& pt) const
{
	pt["depth_preview_palette"]= k_depthPreviewPaletteStrings[(int)palette];
	pt["depth_preview_curve"]= k_depthPreviewCurveStrings[(int)curve];
	pt["depth_preview_gamma"]= gamma;
	pt["depth_preview_range_min"]= rangeMin;
	pt["depth_preview_range_max"]= rangeMax;
}

void DepthPreviewSettings::readFromJSON(const configuru::Config& pt)
{
	// A graph saved before the preview options existed, or one carrying a value this build
	// does not know, reads as the grayscale pass-through the previews always had
	const std::string paletteString= pt.get_or<std::string>(
		"depth_preview_palette", k_depthPreviewPaletteStrings[(int)eDepthPreviewPalette::grayscale]);
	palette= StringUtils::FindEnumValue<eDepthPreviewPalette>(paletteString, k_depthPreviewPaletteStrings);
	if (palette == eDepthPreviewPalette::INVALID)
	{
		palette= eDepthPreviewPalette::grayscale;
	}

	const std::string curveString=
		pt.get_or<std::string>("depth_preview_curve", k_depthPreviewCurveStrings[(int)eDepthPreviewCurve::linear]);
	curve= StringUtils::FindEnumValue<eDepthPreviewCurve>(curveString, k_depthPreviewCurveStrings);
	if (curve == eDepthPreviewCurve::INVALID)
	{
		curve= eDepthPreviewCurve::linear;
	}

	gamma= pt.get_or<float>("depth_preview_gamma", gamma);
	rangeMin= pt.get_or<float>("depth_preview_range_min", rangeMin);
	rangeMax= pt.get_or<float>("depth_preview_range_max", rangeMax);
}

bool DepthPreviewSettings::isPassThrough() const
{
	return palette == eDepthPreviewPalette::grayscale && curve == eDepthPreviewCurve::linear && rangeMin <= 0.f
		   && rangeMax >= 1.f;
}

// -- DepthPreviewRenderer -----
DepthPreviewRenderer::~DepthPreviewRenderer() { dispose(); }

void DepthPreviewRenderer::dispose()
{
	if (m_previewFrameBuffer != nullptr)
	{
		m_previewFrameBuffer->disposeResources();
		m_previewFrameBuffer= nullptr;
	}
	m_previewMaterialInstance= nullptr;
	m_quadMesh= nullptr;
}

IMkTexturePtr DepthPreviewRenderer::getPreviewTexture() const
{
	return m_previewFrameBuffer && m_previewFrameBuffer->isValid() ? m_previewFrameBuffer->getColorTexture()
																   : IMkTexturePtr();
}

void DepthPreviewRenderer::render(NodeEvaluator& evaluator, IMkTexturePtr linearDepthTexture,
								  const DepthPreviewSettings& settings)
{
	// Pass-through settings produce the source texture, so the caller may as well show that
	// one directly and skip the pass entirely
	if (linearDepthTexture == nullptr || settings.isPassThrough())
	{
		dispose();
		return;
	}

	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	if (graphicsContext == nullptr)
		return;

	const int sourceWidth= (int)linearDepthTexture->getTextureWidth();
	const int sourceHeight= (int)linearDepthTexture->getTextureHeight();
	if (sourceWidth <= 0 || sourceHeight <= 0)
		return;

	const int previewWidth= std::min(k_previewWidth, sourceWidth);
	const int previewHeight= std::max((previewWidth * sourceHeight) / sourceWidth, 1);

	if (m_previewFrameBuffer == nullptr)
	{
		m_previewFrameBuffer= createMkFrameBuffer("DepthPreviewRenderer");
		m_previewFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
		m_previewFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGB);
	}
	m_previewFrameBuffer->setSize(previewWidth, previewHeight);

	if (!m_previewFrameBuffer->isValid())
	{
		if (!m_previewFrameBuffer->createResources())
		{
			MIKAN_LOG_ERROR("DepthPreviewRenderer::render") << "Failed to create the depth preview frame buffer";
			m_previewFrameBuffer= nullptr;
			return;
		}

		m_previewMaterialInstance= nullptr;
	}

	if (m_previewMaterialInstance == nullptr)
	{
		MkMaterialConstPtr material=
			graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_DEPTH_COLORIZE);
		if (material == nullptr)
		{
			MIKAN_LOG_ERROR("DepthPreviewRenderer::render") << "Failed to get the depth colorize material";
			return;
		}

		m_previewMaterialInstance= createMkMaterialInstance(material);
	}

	if (m_quadMesh == nullptr)
	{
		m_quadMesh= createFullscreenQuadMesh(graphicsContext, false);
	}

	MkScopedObjectBinding previewBinding(graphicsContext->getMkStateStack().getCurrentState(),
										 "Depth Preview Framebuffer Scope", m_previewFrameBuffer);
	if (!previewBinding)
		return;

	MkMaterialConstPtr material= m_previewMaterialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		m_previewMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, linearDepthTexture);
		m_previewMaterialInstance->setFloatBySemantic(eUniformSemantic::zNear, settings.rangeMin);
		m_previewMaterialInstance->setFloatBySemantic(eUniformSemantic::zFar, settings.rangeMax);
		m_previewMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, (float)(int)settings.palette);
		m_previewMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant1, (float)(int)settings.curve);
		m_previewMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant2, settings.gamma);

		if (auto materialInstanceBinding= m_previewMaterialInstance->bindMaterialInstance(materialBinding))
		{
			m_quadMesh->drawElements();
		}
	}
}

// -- Shared property sheet rows -----
void drawDepthPreviewProperties(MkGuiStyleConstPtr propertyStyle, DepthPreviewSettings& inout_settings)
{
	const std::string paletteItems=
		std::string(locText("nodes.depthPaletteGrayscale")) + '\0' + locText("nodes.depthPaletteGrayscaleInverted")
		+ '\0' + locText("nodes.depthPaletteTurbo") + '\0' + locText("nodes.depthPaletteBanded") + '\0';
	int iPalette= (int)inout_settings.palette;
	if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "depthPreviewPalette", locText("nodes.depthPreviewPalette"),
										  paletteItems.c_str(), iPalette))
	{
		inout_settings.palette= (eDepthPreviewPalette)iPalette;
	}

	const std::string curveItems= std::string(locText("nodes.depthCurveLinear")) + '\0'
								  + locText("nodes.depthCurveGamma") + '\0' + locText("nodes.depthCurveLog") + '\0';
	int iCurve= (int)inout_settings.curve;
	if (MkGui::drawSimpleComboBoxProperty(propertyStyle, "depthPreviewCurve", locText("nodes.depthPreviewCurve"),
										  curveItems.c_str(), iCurve))
	{
		inout_settings.curve= (eDepthPreviewCurve)iCurve;
	}

	// Only the gamma curve reads the exponent, so the row is hidden for the other two
	if (inout_settings.curve == eDepthPreviewCurve::gamma)
	{
		MkGui::drawFloatSliderProperty(propertyStyle, "depthPreviewGamma", locText("nodes.depthPreviewGamma"),
									   inout_settings.gamma, 0.1f, 4.f, 0.1f, 4.f);
	}

	MkGui::drawFloatSliderProperty(propertyStyle, "depthPreviewRangeMin", locText("nodes.depthPreviewRangeMin"),
								   inout_settings.rangeMin, 0.f, 1.f, 0.f, 1.f);
	MkGui::drawFloatSliderProperty(propertyStyle, "depthPreviewRangeMax", locText("nodes.depthPreviewRangeMax"),
								   inout_settings.rangeMax, 0.f, 1.f, 0.f, 1.f);
}
