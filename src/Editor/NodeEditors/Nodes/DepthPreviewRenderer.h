#pragma once

#include "CommonConfig.h"
#include "CompositorConstants.h"
#include "IMkGuiStyle.h"
#include "MikanRendererFwd.h"

#include <memory>

class NodeEvaluator;

/// How a depth node's editor preview maps linear depth to color.
///
/// The defaults reproduce the plain grayscale ramp the previews have always shown, so a
/// graph saved before these existed looks unchanged and the pass is skipped entirely.
struct DepthPreviewSettings
{
	eDepthPreviewPalette palette= eDepthPreviewPalette::grayscale;
	eDepthPreviewCurve curve= eDepthPreviewCurve::linear;
	float gamma= 1.f;
	/// The slice of [0, 1] linear depth the ramp spans
	float rangeMin= 0.f;
	float rangeMax= 1.f;

	void writeToJSON(configuru::Config& pt) const;
	void readFromJSON(const configuru::Config& pt);

	/// True while these reproduce the source texture, which makes the whole pass skippable
	bool isPassThrough() const;
};

/// Colorizes a linear depth texture for a node's editor preview.
///
/// The depth nodes write linear depth into a color attachment as a plain grayscale ramp,
/// which is hard to read: a typical zNear 0.1 / zFar 20 puts the subject in the bottom
/// fifth of the range. This renders that texture through a range window, a curve and a
/// palette into a small frame buffer of its own.
///
/// Its output is for the node body only. The texture a depth node puts on its output pin
/// stays the untouched linear depth that WriteDepthNode and the client depth mask
/// materials decode, which is why this never writes back into the source frame buffer.
class DepthPreviewRenderer
{
public:
	DepthPreviewRenderer()= default;
	~DepthPreviewRenderer();

	/// Renders the colorized preview from a node's linear depth color attachment.
	/// Does nothing while the settings are pass-through, or when the source is missing.
	void render(NodeEvaluator& evaluator, IMkTexturePtr linearDepthTexture, const DepthPreviewSettings& settings);

	/// The colorized texture, or null when nothing has been rendered. A caller with null
	/// here shows its own source texture instead.
	IMkTexturePtr getPreviewTexture() const;

	void dispose();

private:
	IMkFrameBufferPtr m_previewFrameBuffer;
	MkMaterialInstancePtr m_previewMaterialInstance;
	IMkTriangulatedMeshPtr m_quadMesh;

	// The node body draws this at 100x100, so there is nothing to gain from matching the
	// video resolution here
	static const int k_previewWidth= 256;
};

/// Draws the palette, curve and range rows shared by every node carrying a depth preview
void drawDepthPreviewProperties(MkGuiStyleConstPtr propertyStyle, DepthPreviewSettings& inout_settings);
