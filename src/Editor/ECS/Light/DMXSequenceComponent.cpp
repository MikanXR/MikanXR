#include "DMXSequenceComponent.h"
#include "PlaybackTime.h"
#include "ProjectScriptContext.h"
#include "AssetReferencePropertyMetaData.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXSequenceContent.h"
#include "DMXFixtureGroupSystem.h"
#include "EnumPropertyMetaData.h"
#include "FontAssetReference.h"
#include "FunctionInterface.h"
#include "Logger.h"
#include "MikanObjectSystem.h"
#include "MikanVariantTypes.h"
#include "PathUtils.h"
#include "PixelContentAssetReference.h"
#include "RGBPixelGridComponent.h"
#include "RGBSpotLightComponent.h"
#include "ScriptObjectSystem.h"
#include "StringUtils.h"

#include <algorithm>
#include <cmath>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

namespace
{
// The behavior methods a sequence calls on its script component
const char* k_sequenceStartMethodName= "SequenceStart";
const char* k_sequenceUpdateMethodName= "SequenceUpdate";
const char* k_sequenceStopMethodName= "SequenceStop";

const std::string k_sequenceStateStrings[]= {
	"propertyValues.sequence_stopped",
	"propertyValues.sequence_playing",
	"propertyValues.sequence_paused",
};

// The JSON spellings, which are not the localization keys below
const std::string g_contentSourceStrings[(int)eDMXSequenceContentSource::COUNT]= {
	"script",
	"scrollBitmap",
	"scrollText",
	"playAnimation",
};

// Localization keys, not display text (see EnumPropertyMetaData)
const std::string k_contentSourceLocKeys[]= {
	"propertyValues.sequence_source_script",
	"propertyValues.sequence_source_scroll_bitmap",
	"propertyValues.sequence_source_scroll_text",
	"propertyValues.sequence_source_play_animation",
};

const std::string k_scrollDirectionLocKeys[]= {
	"propertyValues.scroll_left",
	"propertyValues.scroll_right",
	"propertyValues.scroll_up",
	"propertyValues.scroll_down",
};

void colorToBytes(const MikanVector3f& color, uint8_t outRGB[3])
{
	outRGB[0]= (uint8_t)std::clamp((int)std::lround(color.x * 255.f), 0, 255);
	outRGB[1]= (uint8_t)std::clamp((int)std::lround(color.y * 255.f), 0, 255);
	outRGB[2]= (uint8_t)std::clamp((int)std::lround(color.z * 255.f), 0, 255);
}
} // namespace

const std::string* k_dmxSequenceContentSourceStrings= g_contentSourceStrings;

// -- DMXSequenceDefinition -----
const std::string DMXSequenceDefinition::k_groupIdPropertyId= "group_id";
const std::string DMXSequenceDefinition::k_scriptComponentIdPropertyId= "script_component_id";
const std::string DMXSequenceDefinition::k_durationSecondsPropertyId= "duration_seconds";
const std::string DMXSequenceDefinition::k_loopPropertyId= "loop";
const std::string DMXSequenceDefinition::k_contentSourcePropertyId= "content_source";
const std::string DMXSequenceDefinition::k_contentPathPropertyId= "content_path";
const std::string DMXSequenceDefinition::k_scrollTextPropertyId= "scroll_text";
const std::string DMXSequenceDefinition::k_fontPathPropertyId= "font_path";
const std::string DMXSequenceDefinition::k_textPixelHeightPropertyId= "text_pixel_height";
const std::string DMXSequenceDefinition::k_foregroundColorPropertyId= "foreground_color";
const std::string DMXSequenceDefinition::k_backgroundColorPropertyId= "background_color";
const std::string DMXSequenceDefinition::k_scrollDirectionPropertyId= "scroll_direction";
const std::string DMXSequenceDefinition::k_scrollSpeedPropertyId= "scroll_speed";
const std::string DMXSequenceDefinition::k_spriteFrameWidthPropertyId= "sprite_frame_width";
const std::string DMXSequenceDefinition::k_spriteFrameHeightPropertyId= "sprite_frame_height";
const std::string DMXSequenceDefinition::k_spriteFpsPropertyId= "sprite_fps";
const std::string DMXSequenceDefinition::k_playbackSpeedScalePropertyId= "playback_speed_scale";
const std::string DMXSequenceDefinition::k_brightnessPropertyId= "brightness";

DMXSequenceDefinition::DMXSequenceDefinition()
	: MikanComponentDefinition()
	, m_contentAssetRefConfig(PixelContentAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_fontAssetRefConfig(FontAssetReferenceFactory().allocateAssetReferenceConfig())
{
}

DMXSequenceDefinition::DMXSequenceDefinition(MikanDMXSequenceID sequenceId)
	: MikanComponentDefinition(sequenceId, "")
	, m_contentAssetRefConfig(PixelContentAssetReferenceFactory().allocateAssetReferenceConfig())
	, m_fontAssetRefConfig(FontAssetReferenceFactory().allocateAssetReferenceConfig())
{
}

configuru::Config DMXSequenceDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_groupIdPropertyId]= m_groupId;
	pt[k_scriptComponentIdPropertyId]= m_scriptComponentId;
	pt[k_durationSecondsPropertyId]= m_durationSeconds;
	pt[k_loopPropertyId]= m_bLoop;

	pt[k_contentSourcePropertyId]= g_contentSourceStrings[(int)m_contentSource];
	if (m_contentAssetRefConfig->isValid())
		pt[k_contentPathPropertyId]= m_contentAssetRefConfig->writeToJSON();
	pt[k_scrollTextPropertyId]= m_scrollText;
	if (m_fontAssetRefConfig->isValid())
		pt[k_fontPathPropertyId]= m_fontAssetRefConfig->writeToJSON();
	pt[k_textPixelHeightPropertyId]= m_textPixelHeight;
	writeVector3f(pt, k_foregroundColorPropertyId.c_str(), m_foregroundColor);
	writeVector3f(pt, k_backgroundColorPropertyId.c_str(), m_backgroundColor);
	pt[k_scrollDirectionPropertyId]= k_dmxScrollDirectionStrings[(int)m_scrollDirection];
	pt[k_scrollSpeedPropertyId]= m_scrollSpeed;
	pt[k_spriteFrameWidthPropertyId]= m_spriteFrameWidth;
	pt[k_spriteFrameHeightPropertyId]= m_spriteFrameHeight;
	pt[k_spriteFpsPropertyId]= m_spriteFps;
	pt[k_playbackSpeedScalePropertyId]= m_playbackSpeedScale;
	pt[k_brightnessPropertyId]= m_brightness;

	return pt;
}

void DMXSequenceDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_groupId= pt.get_or<MikanDMXFixtureGroupID>(k_groupIdPropertyId, m_groupId);
	// A project saved with a handler name rather than a script id loads unassigned
	m_scriptComponentId= pt.get_or<MikanScriptID>(k_scriptComponentIdPropertyId, m_scriptComponentId);
	m_durationSeconds= pt.get_or<float>(k_durationSecondsPropertyId, m_durationSeconds);
	m_bLoop= pt.get_or<bool>(k_loopPropertyId, m_bLoop);

	// A project saved before the content sources existed has none of these
	// keys, and must load as the Lua-driven sequence it was
	const std::string sourceName= pt.get_or<std::string>(
		k_contentSourcePropertyId, g_contentSourceStrings[(int)eDMXSequenceContentSource::script]);
	const eDMXSequenceContentSource source=
		StringUtils::FindEnumValue<eDMXSequenceContentSource>(sourceName, g_contentSourceStrings);
	m_contentSource= (source != eDMXSequenceContentSource::INVALID) ? source : eDMXSequenceContentSource::script;

	m_contentAssetRefConfig= PixelContentAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_contentPathPropertyId))
		m_contentAssetRefConfig->readFromJSON(pt[k_contentPathPropertyId]);

	m_scrollText= pt.get_or<std::string>(k_scrollTextPropertyId, m_scrollText);

	m_fontAssetRefConfig= FontAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_fontPathPropertyId))
		m_fontAssetRefConfig->readFromJSON(pt[k_fontPathPropertyId]);

	m_textPixelHeight= pt.get_or<int>(k_textPixelHeightPropertyId, m_textPixelHeight);

	// readVector3f zeroes its output when the key is absent, so a missing
	// color would load as black rather than keeping its default
	if (pt.has_key(k_foregroundColorPropertyId))
		readVector3f(pt, k_foregroundColorPropertyId.c_str(), m_foregroundColor);
	if (pt.has_key(k_backgroundColorPropertyId))
		readVector3f(pt, k_backgroundColorPropertyId.c_str(), m_backgroundColor);

	const std::string directionName= pt.get_or<std::string>(
		k_scrollDirectionPropertyId, k_dmxScrollDirectionStrings[(int)eDMXScrollDirection::left]);
	const eDMXScrollDirection direction=
		StringUtils::FindEnumValue<eDMXScrollDirection>(directionName, k_dmxScrollDirectionStrings);
	m_scrollDirection= (direction != eDMXScrollDirection::INVALID) ? direction : eDMXScrollDirection::left;

	m_scrollSpeed= pt.get_or<float>(k_scrollSpeedPropertyId, m_scrollSpeed);
	m_spriteFrameWidth= pt.get_or<int>(k_spriteFrameWidthPropertyId, m_spriteFrameWidth);
	m_spriteFrameHeight= pt.get_or<int>(k_spriteFrameHeightPropertyId, m_spriteFrameHeight);
	m_spriteFps= pt.get_or<float>(k_spriteFpsPropertyId, m_spriteFps);
	m_playbackSpeedScale= pt.get_or<float>(k_playbackSpeedScalePropertyId, m_playbackSpeedScale);
	m_brightness= pt.get_or<float>(k_brightnessPropertyId, m_brightness);
}

bool DMXSequenceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* values= initParams.getTypedPointer<MikanDMXSequenceComponentValues>();
	if (values)
	{
		m_groupId= values->group_id;
		m_scriptComponentId= values->script_component_id;
		m_durationSeconds= values->duration_seconds;
		m_bLoop= values->loop;

		m_contentSource= (eDMXSequenceContentSource)values->content_source;
		m_contentAssetRefConfig->assetPath= PathUtils::utf8CStrToPathString(values->content_path.getUtf8Value());
		m_scrollText= values->scroll_text.getUtf8Value();
		m_fontAssetRefConfig->assetPath= PathUtils::utf8CStrToPathString(values->font_path.getUtf8Value());
		m_textPixelHeight= values->text_pixel_height;
		m_foregroundColor= values->foreground_color;
		m_backgroundColor= values->background_color;
		m_scrollDirection= (eDMXScrollDirection)values->scroll_direction;
		m_scrollSpeed= values->scroll_speed;
		m_spriteFrameWidth= values->sprite_frame_width;
		m_spriteFrameHeight= values->sprite_frame_height;
		m_spriteFps= values->sprite_fps;
		m_playbackSpeedScale= values->playback_speed_scale;
		m_brightness= values->brightness;
	}

	if (m_groupId == INVALID_MIKAN_ID)
	{
		// If no group was specified, use the first one
		auto groupSystem= ownerObjectSystem->getObjectSystemOfType<DMXFixtureGroupSystem>();

		m_groupId= groupSystem->getFirstComponentId();
	}

	return true;
}

void DMXSequenceDefinition::setGroupId(MikanDMXFixtureGroupID groupId)
{
	if (groupId != m_groupId)
	{
		m_groupId= groupId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_groupIdPropertyId));
	}
}

void DMXSequenceDefinition::setScriptComponentId(MikanScriptID scriptComponentId)
{
	if (scriptComponentId != m_scriptComponentId)
	{
		m_scriptComponentId= scriptComponentId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scriptComponentIdPropertyId));
	}
}

void DMXSequenceDefinition::setDurationSeconds(float durationSeconds)
{
	if (durationSeconds != m_durationSeconds)
	{
		m_durationSeconds= durationSeconds;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_durationSecondsPropertyId));
	}
}

void DMXSequenceDefinition::setLoop(bool bLoop)
{
	if (bLoop != m_bLoop)
	{
		m_bLoop= bLoop;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_loopPropertyId));
	}
}

void DMXSequenceDefinition::setContentSource(eDMXSequenceContentSource contentSource)
{
	if (m_contentSource != contentSource && contentSource != eDMXSequenceContentSource::INVALID)
	{
		m_contentSource= contentSource;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_contentSourcePropertyId));
	}
}

std::filesystem::path DMXSequenceDefinition::getContentPath() const { return m_contentAssetRefConfig->assetPath; }

void DMXSequenceDefinition::setContentPath(const std::filesystem::path& contentPath)
{
	const std::string stored= contentPath.empty() ? std::string() : PathUtils::makeStoredProjectPath(contentPath);

	if (stored != m_contentAssetRefConfig->assetPath)
	{
		m_contentAssetRefConfig->assetPath= stored;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_contentPathPropertyId));
	}
}

void DMXSequenceDefinition::setScrollText(const std::string& scrollText)
{
	if (scrollText != m_scrollText)
	{
		m_scrollText= scrollText;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scrollTextPropertyId));
	}
}

std::filesystem::path DMXSequenceDefinition::getFontPath() const
{
	// An unset face falls back to the bundled one rather than failing to
	// rasterize, so a fresh text sequence draws something
	return m_fontAssetRefConfig->assetPath.empty() ? FontAssetReferenceFactory::getDefaultFontPath()
												   : std::filesystem::path(m_fontAssetRefConfig->assetPath);
}

void DMXSequenceDefinition::setFontPath(const std::filesystem::path& fontPath)
{
	const std::string stored= fontPath.empty() ? std::string() : PathUtils::makeStoredProjectPath(fontPath);

	if (stored != m_fontAssetRefConfig->assetPath)
	{
		m_fontAssetRefConfig->assetPath= stored;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_fontPathPropertyId));
	}
}

void DMXSequenceDefinition::setTextPixelHeight(int textPixelHeight)
{
	const int clamped= std::max(textPixelHeight, 0);
	if (clamped != m_textPixelHeight)
	{
		m_textPixelHeight= clamped;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_textPixelHeightPropertyId));
	}
}

void DMXSequenceDefinition::setForegroundColor(const MikanVector3f& color)
{
	if (color.x != m_foregroundColor.x || color.y != m_foregroundColor.y || color.z != m_foregroundColor.z)
	{
		m_foregroundColor= color;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_foregroundColorPropertyId));
	}
}

void DMXSequenceDefinition::setBackgroundColor(const MikanVector3f& color)
{
	if (color.x != m_backgroundColor.x || color.y != m_backgroundColor.y || color.z != m_backgroundColor.z)
	{
		m_backgroundColor= color;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_backgroundColorPropertyId));
	}
}

void DMXSequenceDefinition::setScrollDirection(eDMXScrollDirection scrollDirection)
{
	if (m_scrollDirection != scrollDirection && scrollDirection != eDMXScrollDirection::INVALID)
	{
		m_scrollDirection= scrollDirection;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scrollDirectionPropertyId));
	}
}

void DMXSequenceDefinition::setScrollSpeed(float scrollSpeed)
{
	if (scrollSpeed != m_scrollSpeed)
	{
		m_scrollSpeed= scrollSpeed;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scrollSpeedPropertyId));
	}
}

void DMXSequenceDefinition::setSpriteFrameWidth(int spriteFrameWidth)
{
	const int clamped= std::max(spriteFrameWidth, 0);
	if (clamped != m_spriteFrameWidth)
	{
		m_spriteFrameWidth= clamped;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spriteFrameWidthPropertyId));
	}
}

void DMXSequenceDefinition::setSpriteFrameHeight(int spriteFrameHeight)
{
	const int clamped= std::max(spriteFrameHeight, 0);
	if (clamped != m_spriteFrameHeight)
	{
		m_spriteFrameHeight= clamped;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spriteFrameHeightPropertyId));
	}
}

void DMXSequenceDefinition::setSpriteFps(float spriteFps)
{
	if (spriteFps != m_spriteFps)
	{
		m_spriteFps= spriteFps;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spriteFpsPropertyId));
	}
}

void DMXSequenceDefinition::setPlaybackSpeedScale(float playbackSpeedScale)
{
	if (playbackSpeedScale != m_playbackSpeedScale)
	{
		m_playbackSpeedScale= playbackSpeedScale;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_playbackSpeedScalePropertyId));
	}
}

void DMXSequenceDefinition::setBrightness(float brightness)
{
	// Stored as written and clamped where it is applied. Clamping here would
	// let an out of range write report success while changing nothing, which
	// breaks the contract that a successful set notifies.
	if (brightness != m_brightness)
	{
		m_brightness= brightness;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_brightnessPropertyId));
	}
}

// -- DMXSequenceComponent -----
const std::string DMXSequenceComponent::k_playbackStatePropertyId= "playback_state";
const std::string DMXSequenceComponent::k_timeSinceStartPropertyId= "time_since_start";
const std::string DMXSequenceComponent::k_playFunctionId= "play_sequence";
const std::string DMXSequenceComponent::k_pauseFunctionId= "pause_sequence";
const std::string DMXSequenceComponent::k_stopFunctionId= "stop_sequence";

DMXSequenceComponent::DMXSequenceComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

DMXFixtureGroupComponentPtr DMXSequenceComponent::getGroup() const
{
	// A component built without an owner (the unit tests) has no systems to ask
	if (!getOwnerObject())
		return nullptr;

	DMXFixtureGroupSystemPtr groupSystem= getObjectSystemOfType<DMXFixtureGroupSystem>();
	if (!groupSystem)
		return nullptr;

	return groupSystem->getGroupById(getDMXSequenceDefinition()->getGroupId());
}

CommonScriptContextPtr DMXSequenceComponent::getScriptContext() const
{
	// A component built without an owner (the unit tests) has no systems to ask
	if (!getOwnerObject())
		return nullptr;

	ScriptObjectSystemPtr scriptSystem= getObjectSystemOfType<ScriptObjectSystem>();
	return scriptSystem ? scriptSystem->getScriptContext() : nullptr;
}

bool DMXSequenceComponent::hasScriptHandler() const
{
	const MikanScriptID scriptId= getDMXSequenceDefinition()->getScriptComponentId();
	if (scriptId == INVALID_MIKAN_ID)
		return false;

	CommonScriptContextPtr scriptContext= getScriptContext();

	return scriptContext && scriptContext->behaviorHasMethod(scriptId, k_sequenceUpdateMethodName);
}

void DMXSequenceComponent::play()
{
	if (m_state == eDMXSequenceState::Playing)
		return;

	if (m_state == eDMXSequenceState::Stopped)
	{
		m_timeSinceStart= 0.f;
		m_frameBuffer.clear();

		const bool bScriptDriven= getDMXSequenceDefinition()->getContentSource() == eDMXSequenceContentSource::script;

		// The handler runs first in every mode, so a script can pick the text
		// or the image before the content is built from it. A rasterized
		// source needs no handler, so only a script-driven sequence is stopped
		// by one that is missing or throws.
		if (hasScriptHandler())
		{
			if (!callHandler(k_sequenceStartMethodName, false, 0.f) && bScriptDriven)
				return;
		}
		else if (bScriptDriven)
		{
			return;
		}

		if (!bScriptDriven)
		{
			rebuildContent();
			updateGeneratedContent();
		}

		applyFrameBuffer();
	}

	m_state= eDMXSequenceState::Playing;
}

void DMXSequenceComponent::pause()
{
	if (m_state == eDMXSequenceState::Playing)
	{
		m_state= eDMXSequenceState::Paused;
	}
}

void DMXSequenceComponent::stop()
{
	if (m_state == eDMXSequenceState::Stopped)
		return;

	// The last frame stays on the fixtures unless the handler writes another
	m_state= eDMXSequenceState::Stopped;
	m_timeSinceStart= 0.f;

	// A script's content picks last only as long as the run it picked them for
	m_bHasRuntimeText= false;
	m_runtimeText.clear();
	m_bHasRuntimeContentPath= false;
	m_runtimeContentPath.clear();
	m_bContentDirty= true;

	if (hasScriptHandler() && callHandler(k_sequenceStopMethodName, false, 0.f))
	{
		applyFrameBuffer();
	}
}

void DMXSequenceComponent::advanceTime(float& inoutTime, float delta, float duration, bool bLoop, bool& outWrapped,
									   bool& outFinished)
{
	advancePlaybackTime(inoutTime, delta, duration, bLoop, outWrapped, outFinished);
}

void DMXSequenceComponent::tick(float deltaSeconds)
{
	if (m_state != eDMXSequenceState::Playing)
		return;

	DMXSequenceDefinitionPtr definition= getDMXSequenceDefinition();
	const bool bScriptDriven= definition->getContentSource() == eDMXSequenceContentSource::script;

	// A handler that vanished with a script reload ends a script-driven
	// sequence. A rasterized one has no handler to lose, so it plays on.
	if (bScriptDriven)
	{
		if (!hasScriptHandler())
		{
			m_state= eDMXSequenceState::Stopped;
			m_timeSinceStart= 0.f;
			return;
		}
	}

	// Zero or less takes the content's own period, so a loop wraps exactly
	// where the content does and a one-shot ends when it has played out
	float duration= definition->getDurationSeconds();
	if (!bScriptDriven && duration <= 0.f)
		duration= getContentPeriodSeconds();

	bool bWrapped= false;
	bool bFinished= false;
	advanceTime(m_timeSinceStart, deltaSeconds, duration, definition->getLoop(), bWrapped, bFinished);

	if (bScriptDriven)
	{
		if (!callHandler(k_sequenceUpdateMethodName, true, deltaSeconds))
			return;
	}
	else
	{
		// The editor owns the pixels here, so the handler is not called per
		// frame; it had its say in start
		updateGeneratedContent();
	}

	applyFrameBuffer();

	if (bFinished)
	{
		stop();
	}
}

RGBPixelGridComponentPtr DMXSequenceComponent::getTargetPixelGrid() const
{
	DMXFixtureGroupComponentPtr group= getGroup();
	if (!group)
		return nullptr;

	for (const MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
	{
		RGBPixelGridComponentPtr pixelGrid=
			std::dynamic_pointer_cast<RGBPixelGridComponent>(group->resolveFixture(fixtureId));
		if (pixelGrid)
			return pixelGrid;
	}

	return nullptr;
}

std::string DMXSequenceComponent::getEffectiveText() const
{
	return m_bHasRuntimeText ? m_runtimeText : getDMXSequenceDefinition()->getScrollText();
}

std::filesystem::path DMXSequenceComponent::getEffectiveContentPath() const
{
	return m_bHasRuntimeContentPath ? m_runtimeContentPath : getDMXSequenceDefinition()->getContentPath();
}

void DMXSequenceComponent::setRuntimeText(const std::string& text)
{
	m_bHasRuntimeText= true;
	m_runtimeText= text;
	m_bContentDirty= true;
}

void DMXSequenceComponent::setRuntimeContentPath(const std::filesystem::path& contentPath)
{
	m_bHasRuntimeContentPath= true;
	m_runtimeContentPath= contentPath;
	m_bContentDirty= true;
}

bool DMXSequenceComponent::rebuildContent()
{
	m_contentFrames.clear();
	m_contentFrameDelaysMs.clear();
	m_bContentDirty= false;

	DMXSequenceDefinitionPtr definition= getDMXSequenceDefinition();
	std::string error;

	switch (definition->getContentSource())
	{
	case eDMXSequenceContentSource::scrollBitmap:
	{
		DMXPixelCanvas canvas;
		const std::filesystem::path resolvedPath= PathUtils::resolveProjectResource(getEffectiveContentPath());
		if (!DMXSequenceContent::loadImage(resolvedPath, canvas, error))
			break;

		m_contentFrames.push_back(std::move(canvas));
	}
	break;
	case eDMXSequenceContentSource::scrollText:
	{
		// A height of zero tracks the grid, which is what makes one line of
		// text fill a panel of any size
		int pixelHeight= definition->getTextPixelHeight();
		if (pixelHeight <= 0)
		{
			RGBPixelGridComponentPtr pixelGrid= getTargetPixelGrid();
			pixelHeight= pixelGrid ? pixelGrid->getRGBPixelGridDefinition()->getRows() : 0;
		}

		uint8_t foreground[3], background[3];
		colorToBytes(definition->getForegroundColor(), foreground);
		colorToBytes(definition->getBackgroundColor(), background);

		DMXPixelCanvas canvas;
		const std::filesystem::path resolvedFontPath= PathUtils::resolveProjectResource(definition->getFontPath());
		if (!DMXSequenceContent::rasterizeText(getEffectiveText(), resolvedFontPath, pixelHeight, foreground,
											   background, canvas, error))
		{
			break;
		}

		m_contentFrames.push_back(std::move(canvas));
	}
	break;
	case eDMXSequenceContentSource::playAnimation:
		DMXSequenceContent::loadAnimation(PathUtils::resolveProjectResource(getEffectiveContentPath()),
										  definition->getSpriteFrameWidth(), definition->getSpriteFrameHeight(),
										  definition->getSpriteFps(), m_contentFrames, m_contentFrameDelaysMs, error);
		break;
	default:
		break;
	}

	if (m_contentFrames.empty())
	{
		MIKAN_LOG_ERROR("DMXSequenceComponent::rebuildContent")
			<< "Sequence " << getName() << " has no content: " << (error.empty() ? "nothing to build" : error);
		return false;
	}

	return true;
}

float DMXSequenceComponent::getContentPeriodSeconds() const
{
	DMXSequenceDefinitionPtr definition= getDMXSequenceDefinition();

	if (definition->getContentSource() == eDMXSequenceContentSource::playAnimation)
	{
		return DMXSequenceContent::computeAnimationPeriod(m_contentFrameDelaysMs, definition->getPlaybackSpeedScale());
	}

	if (m_contentFrames.empty())
		return 0.f;

	RGBPixelGridComponentPtr pixelGrid= getTargetPixelGrid();
	if (!pixelGrid)
		return 0.f;

	const DMXPixelCanvas& canvas= m_contentFrames.front();
	RGBPixelGridDefinitionPtr gridDefinition= pixelGrid->getRGBPixelGridDefinition();
	const eDMXScrollDirection direction= definition->getScrollDirection();
	const bool bHorizontal= (direction == eDMXScrollDirection::left || direction == eDMXScrollDirection::right);

	return DMXSequenceContent::computeScrollPeriod(
		definition->getScrollSpeed(), bHorizontal ? canvas.width : canvas.height,
		bHorizontal ? gridDefinition->getColumns() : gridDefinition->getRows());
}

void DMXSequenceComponent::updateGeneratedContent()
{
	if (m_bContentDirty)
		rebuildContent();

	if (m_contentFrames.empty())
		return;

	RGBPixelGridComponentPtr pixelGrid= getTargetPixelGrid();
	if (!pixelGrid)
		return;

	DMXSequenceDefinitionPtr definition= getDMXSequenceDefinition();
	RGBPixelGridDefinitionPtr gridDefinition= pixelGrid->getRGBPixelGridDefinition();

	uint8_t background[3];
	colorToBytes(definition->getBackgroundColor(), background);

	int srcX= 0;
	int srcY= 0;
	const DMXPixelCanvas* canvas= &m_contentFrames.front();

	if (definition->getContentSource() == eDMXSequenceContentSource::playAnimation)
	{
		const int frameIndex= DMXSequenceContent::computeAnimationFrame(
			m_timeSinceStart, m_contentFrameDelaysMs, definition->getPlaybackSpeedScale(), definition->getLoop());
		if (frameIndex < 0 || frameIndex >= (int)m_contentFrames.size())
			return;

		// A frame is shown whole, centred on the grid rather than scrolled
		canvas= &m_contentFrames[frameIndex];
		srcX= (canvas->width - gridDefinition->getColumns()) / 2;
		srcY= (canvas->height - gridDefinition->getRows()) / 2;
	}
	else
	{
		const eDMXScrollDirection direction= definition->getScrollDirection();
		const bool bHorizontal= (direction == eDMXScrollDirection::left || direction == eDMXScrollDirection::right);
		const float offset= DMXSequenceContent::computeScrollOffset(
			m_timeSinceStart, definition->getScrollSpeed(), direction, bHorizontal ? canvas->width : canvas->height,
			bHorizontal ? gridDefinition->getColumns() : gridDefinition->getRows(), definition->getLoop());

		// Snapped to whole pixels: a grid this small has nothing to gain from
		// sampling between them
		if (bHorizontal)
		{
			srcX= (int)std::floor(offset);
			srcY= (canvas->height - gridDefinition->getRows()) / 2;
		}
		else
		{
			srcX= (canvas->width - gridDefinition->getColumns()) / 2;
			srcY= (int)std::floor(offset);
		}
	}

	DMXSequenceContent::blitWindow(*canvas, srcX, srcY, *gridDefinition, background,
								   m_frameBuffer[pixelGrid->getComponentId()]);
}

void DMXSequenceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
												   const ConfigPropertyChangeSet& changedPropertySet)
{
	MikanComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	// Any property the content is built from invalidates it, so an edit shows
	// up on the next frame without reloading the file every frame
	static const std::string* k_contentProperties[]= {
		&DMXSequenceDefinition::k_contentSourcePropertyId,     &DMXSequenceDefinition::k_contentPathPropertyId,
		&DMXSequenceDefinition::k_scrollTextPropertyId,        &DMXSequenceDefinition::k_fontPathPropertyId,
		&DMXSequenceDefinition::k_textPixelHeightPropertyId,   &DMXSequenceDefinition::k_foregroundColorPropertyId,
		&DMXSequenceDefinition::k_backgroundColorPropertyId,   &DMXSequenceDefinition::k_spriteFrameWidthPropertyId,
		&DMXSequenceDefinition::k_spriteFrameHeightPropertyId, &DMXSequenceDefinition::k_spriteFpsPropertyId,
	};

	for (const std::string* propertyName : k_contentProperties)
	{
		if (changedPropertySet.hasPropertyName(*propertyName))
		{
			m_bContentDirty= true;
			break;
		}
	}

	// The dimmer re-sends the frame it already has, so dragging the slider
	// tracks on the fixtures even while the sequence is paused or stopped and
	// nothing is calling tick. Writing channel values sends DMX without
	// touching a config, so this cannot re-enter the change chain.
	if (changedPropertySet.hasPropertyName(DMXSequenceDefinition::k_brightnessPropertyId))
	{
		applyFrameBuffer();
	}
}

bool DMXSequenceComponent::callHandler(const char* methodName, bool bWithTime, float deltaSeconds)
{
	CommonScriptContextPtr scriptContext= getScriptContext();
	if (!scriptContext)
		return false;

	const MikanScriptID scriptId= getDMXSequenceDefinition()->getScriptComponentId();
	std::string error;
	const bool bSuccess= scriptContext->callBehaviorMethod(
		scriptId, methodName,
		[this, bWithTime, deltaSeconds](lua_State* L) -> int
		{
			// The count has to match what actually landed on the stack, since
			// lua_pcall takes it on faith and a failed push leaves nothing
			// behind. A short count reaches the handler as a missing argument,
			// which fails that sequence alone.
			int argCount= 0;

			if (!luabridge::push(L, this))
				return argCount;
			argCount++;

			if (!bWithTime)
				return argCount;

			if (!luabridge::push(L, m_timeSinceStart))
				return argCount;
			argCount++;

			if (!luabridge::push(L, deltaSeconds))
				return argCount;
			argCount++;

			return argCount;
		},
		error, eScriptErrorKind::sequence);

	if (!bSuccess)
	{
		MIKAN_LOG_ERROR("DMXSequenceComponent::callHandler")
			<< "Sequence " << getName() << " script " << scriptId << " " << methodName << " failed: " << error;
		m_state= eDMXSequenceState::Stopped;
		m_timeSinceStart= 0.f;
	}

	return bSuccess;
}

void DMXSequenceComponent::applyFrameBuffer()
{
	DMXFixtureGroupComponentPtr group= getGroup();
	if (!group)
		return;

	for (const MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
	{
		auto it= m_frameBuffer.find(fixtureId);
		if (it == m_frameBuffer.end())
			continue;

		DMXFixtureComponentPtr fixture= group->resolveFixture(fixtureId);
		if (!fixture)
			continue;

		std::vector<uint8_t> values= it->second;
		values.resize(fixture->getDMXFixtureDefinition()->getDMXChannelCount(), 0);

		// Every fixture kind a group can hold is RGB triples, so the dimmer
		// scales every channel. A fixture with non-color channels would need
		// this to ask the fixture which of its channels carry color.
		applyBrightness(values, getDMXSequenceDefinition()->getBrightness());

		fixture->setChannelValues(values);
	}
}

void DMXSequenceComponent::applyBrightness(std::vector<uint8_t>& inoutValues, float brightness)
{
	const float scale= std::clamp(brightness, 0.f, 1.f);
	if (scale >= 1.f)
		return;

	for (uint8_t& value : inoutValues)
	{
		value= (uint8_t)std::clamp((int)std::lround(value * scale), 0, 255);
	}
}

bool DMXSequenceComponent::getAppliedFixtureValues(MikanLightID fixtureId, std::vector<uint8_t>& outValues) const
{
	if (!getFrameBufferValues(fixtureId, outValues))
		return false;

	applyBrightness(outValues, getDMXSequenceDefinition()->getBrightness());

	return true;
}

bool DMXSequenceComponent::getFrameBufferValues(MikanLightID fixtureId, std::vector<uint8_t>& outValues) const
{
	auto it= m_frameBuffer.find(fixtureId);
	if (it == m_frameBuffer.end())
		return false;

	outValues= it->second;
	return true;
}

void DMXSequenceComponent::setFixtureChannels(MikanLightID fixtureId, const std::vector<uint8_t>& values)
{
	m_frameBuffer[fixtureId]= values;
}

void DMXSequenceComponent::setFixtureColor(MikanLightID fixtureId, uint8_t r, uint8_t g, uint8_t b)
{
	std::vector<uint8_t>& values= m_frameBuffer[fixtureId];
	if (values.size() < 3)
		values.resize(3, 0);
	values[0]= r;
	values[1]= g;
	values[2]= b;
}

void DMXSequenceComponent::setPixel(MikanLightID fixtureId, int col, int row, uint8_t r, uint8_t g, uint8_t b)
{
	DMXFixtureGroupComponentPtr group= getGroup();
	RGBPixelGridComponentPtr pixelGrid=
		group ? std::dynamic_pointer_cast<RGBPixelGridComponent>(group->resolveFixture(fixtureId)) : nullptr;
	if (!pixelGrid)
		return;

	// Checked before the buffer is reached, so an out of range cell does not
	// leave an empty slice behind for a fixture nothing was written to
	RGBPixelGridDefinitionPtr gridDefinition= pixelGrid->getRGBPixelGridDefinition();
	if (gridDefinition->getPixelWireIndex(col, row) < 0)
		return;

	// The grid's origin corner and zig-zag wiring decide where the cell lands
	DMXSequenceContent::writeGridPixel(*gridDefinition, col, row, r, g, b, m_frameBuffer[fixtureId]);
}

void DMXSequenceComponent::fillGroup(uint8_t r, uint8_t g, uint8_t b)
{
	DMXFixtureGroupComponentPtr group= getGroup();
	if (!group)
		return;

	for (const MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
	{
		DMXFixtureComponentPtr fixture= group->resolveFixture(fixtureId);
		if (!fixture)
			continue;

		// One triple repeated across the fixture's channels
		const size_t channelCount= fixture->getDMXFixtureDefinition()->getDMXChannelCount();
		std::vector<uint8_t> values(channelCount, 0);
		for (size_t i= 0; i + 2 < channelCount; i+= 3)
		{
			values[i]= r;
			values[i + 1]= g;
			values[i + 2]= b;
		}
		m_frameBuffer[fixtureId]= values;
	}
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle DMXSequenceComponent::getClientAPIValuesStructType() const
{
	return &MikanDMXSequenceComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void DMXSequenceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_groupIdPropertyId, MikanVariantType::INT));
	// The panel draws the script picker itself
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_scriptComponentIdPropertyId,
																  MikanVariantType::INT)
								 ->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_durationSecondsPropertyId,
																  MikanVariantType::FLOAT));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_loopPropertyId, MikanVariantType::BOOL));

	// Where the pixels come from, and the settings each source reads. The panel
	// hides the ones the active source does not use.
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_contentSourcePropertyId, MikanVariantType::INT)
			->addMetaData(
				std::make_shared<EnumPropertyMetaData>(k_contentSourceLocKeys, (int)eDMXSequenceContentSource::COUNT)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_contentPathPropertyId, MikanVariantType::STRING)
			->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
				AssetReferenceFactory::createFactory<PixelContentAssetReferenceFactory>())));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_scrollTextPropertyId, MikanVariantType::STRING));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_fontPathPropertyId, MikanVariantType::STRING)
			->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
				AssetReferenceFactory::createFactory<FontAssetReferenceFactory>())));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_textPixelHeightPropertyId,
																  MikanVariantType::INT));
	// Drawn by the panel as color pickers rather than three drag floats
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_foregroundColorPropertyId,
																  MikanVariantType::VECTOR3F));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_backgroundColorPropertyId,
																  MikanVariantType::VECTOR3F));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_scrollDirectionPropertyId, MikanVariantType::INT)
			->addMetaData(
				std::make_shared<EnumPropertyMetaData>(k_scrollDirectionLocKeys, (int)eDMXScrollDirection::COUNT)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_scrollSpeedPropertyId, MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_spriteFrameWidthPropertyId,
																  MikanVariantType::INT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_spriteFrameHeightPropertyId,
																  MikanVariantType::INT));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_spriteFpsPropertyId, MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_playbackSpeedScalePropertyId,
																  MikanVariantType::FLOAT));
	// Drawn by the panel as a percentage slider rather than a raw fraction
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(DMXSequenceDefinition::k_brightnessPropertyId, MikanVariantType::FLOAT));
	// Runtime playback status, drawn by the panel as a status line
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(k_playbackStatePropertyId, MikanVariantType::INT)
								 ->setReadOnly()
								 ->setUIHidden()
								 ->addMetaData(std::make_shared<EnumPropertyMetaData>(k_sequenceStateStrings, 3)));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(k_timeSinceStartPropertyId, MikanVariantType::FLOAT)
								 ->setReadOnly()
								 ->setUIHidden());
}

bool DMXSequenceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	DMXSequenceDefinitionPtr definition= getDMXSequenceDefinition();

	if (propertyName == DMXSequenceDefinition::k_groupIdPropertyId)
	{
		outValue= static_cast<int>(definition->getGroupId());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scriptComponentIdPropertyId)
	{
		outValue= static_cast<int>(definition->getScriptComponentId());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_durationSecondsPropertyId)
	{
		outValue= definition->getDurationSeconds();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_loopPropertyId)
	{
		outValue= definition->getLoop();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_contentSourcePropertyId)
	{
		outValue= static_cast<int>(definition->getContentSource());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_contentPathPropertyId)
	{
		outValue= definition->getContentPath().string();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scrollTextPropertyId)
	{
		outValue= definition->getScrollText();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_fontPathPropertyId)
	{
		outValue= definition->getFontPath().string();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_textPixelHeightPropertyId)
	{
		outValue= definition->getTextPixelHeight();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_foregroundColorPropertyId)
	{
		outValue= definition->getForegroundColor();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_backgroundColorPropertyId)
	{
		outValue= definition->getBackgroundColor();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scrollDirectionPropertyId)
	{
		outValue= static_cast<int>(definition->getScrollDirection());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scrollSpeedPropertyId)
	{
		outValue= definition->getScrollSpeed();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_spriteFrameWidthPropertyId)
	{
		outValue= definition->getSpriteFrameWidth();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_spriteFrameHeightPropertyId)
	{
		outValue= definition->getSpriteFrameHeight();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_spriteFpsPropertyId)
	{
		outValue= definition->getSpriteFps();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_playbackSpeedScalePropertyId)
	{
		outValue= definition->getPlaybackSpeedScale();
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_brightnessPropertyId)
	{
		outValue= definition->getBrightness();
		return true;
	}
	else if (propertyName == k_playbackStatePropertyId)
	{
		outValue= static_cast<int>(m_state);
		return true;
	}
	else if (propertyName == k_timeSinceStartPropertyId)
	{
		outValue= m_timeSinceStart;
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool DMXSequenceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	DMXSequenceDefinitionPtr definition= getDMXSequenceDefinition();

	if (propertyName == DMXSequenceDefinition::k_groupIdPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setGroupId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scriptComponentIdPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setScriptComponentId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_durationSecondsPropertyId)
	{
		if (inValue.value_type != MikanVariantType::FLOAT)
			return false;

		definition->setDurationSeconds(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_loopPropertyId)
	{
		if (inValue.value_type != MikanVariantType::BOOL)
			return false;

		definition->setLoop(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_contentSourcePropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setContentSource((eDMXSequenceContentSource)inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_contentPathPropertyId)
	{
		if (inValue.value_type != MikanVariantType::STRING)
			return false;

		definition->setContentPath(inValue.getUtf8Value());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scrollTextPropertyId)
	{
		if (inValue.value_type != MikanVariantType::STRING)
			return false;

		definition->setScrollText(inValue.getUtf8Value());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_fontPathPropertyId)
	{
		if (inValue.value_type != MikanVariantType::STRING)
			return false;

		definition->setFontPath(inValue.getUtf8Value());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_textPixelHeightPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setTextPixelHeight(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_foregroundColorPropertyId)
	{
		if (inValue.value_type != MikanVariantType::VECTOR3F)
			return false;

		definition->setForegroundColor(inValue.getVector3fValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_backgroundColorPropertyId)
	{
		if (inValue.value_type != MikanVariantType::VECTOR3F)
			return false;

		definition->setBackgroundColor(inValue.getVector3fValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scrollDirectionPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setScrollDirection((eDMXScrollDirection)inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_scrollSpeedPropertyId)
	{
		if (inValue.value_type != MikanVariantType::FLOAT)
			return false;

		definition->setScrollSpeed(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_spriteFrameWidthPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setSpriteFrameWidth(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_spriteFrameHeightPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setSpriteFrameHeight(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_spriteFpsPropertyId)
	{
		if (inValue.value_type != MikanVariantType::FLOAT)
			return false;

		definition->setSpriteFps(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_playbackSpeedScalePropertyId)
	{
		if (inValue.value_type != MikanVariantType::FLOAT)
			return false;

		definition->setPlaybackSpeedScale(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == DMXSequenceDefinition::k_brightnessPropertyId)
	{
		if (inValue.value_type != MikanVariantType::FLOAT)
			return false;

		definition->setBrightness(inValue.getFloatValue());
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void DMXSequenceComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_playFunctionId, "Play"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_pauseFunctionId, "Pause"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_stopFunctionId, "Stop"));
}

bool DMXSequenceComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_playFunctionId)
	{
		play();
		return true;
	}
	else if (functionName == k_pauseFunctionId)
	{
		pause();
		return true;
	}
	else if (functionName == k_stopFunctionId)
	{
		stop();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}

// -- Lua Binding ----
void DMXSequenceComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<DMXSequenceComponent, MikanComponent>(DMXSequenceComponent::k_componentClassName.c_str())
		.addProperty("groupId",
					 [](DMXSequenceComponent* c) -> int { return c->getDMXSequenceDefinition()->getGroupId(); })
		.addProperty("scriptComponentId", [](DMXSequenceComponent* c) -> int
					 { return c->getDMXSequenceDefinition()->getScriptComponentId(); })
		.addProperty("timeSinceStart", [](DMXSequenceComponent* c) -> float { return c->getTimeSinceStart(); })
		.addProperty("isPlaying", [](DMXSequenceComponent* c) -> bool { return c->isPlaying(); })
		.addFunction("getGroup",
					 [](DMXSequenceComponent* c) -> DMXFixtureGroupComponent* { return c->getGroup().get(); })
		.addFunction("play", [](DMXSequenceComponent* c) { c->play(); })
		.addFunction("pause", [](DMXSequenceComponent* c) { c->pause(); })
		.addFunction("stop", [](DMXSequenceComponent* c) { c->stop(); })
		.addFunction("setFixtureColor", [](DMXSequenceComponent* c, int fixtureId, int r, int g, int b)
					 { c->setFixtureColor(fixtureId, (uint8_t)r, (uint8_t)g, (uint8_t)b); })
		.addFunction("setPixel", [](DMXSequenceComponent* c, int fixtureId, int col, int row, int r, int g, int b)
					 { c->setPixel(fixtureId, col, row, (uint8_t)r, (uint8_t)g, (uint8_t)b); })
		.addFunction("setFixtureChannels",
					 [](DMXSequenceComponent* c, int fixtureId, luabridge::LuaRef bytes)
					 {
						 std::vector<uint8_t> values;
						 if (bytes.isTable())
						 {
							 const int count= static_cast<int>(bytes.length());
							 for (int i= 1; i <= count; ++i)
								 values.push_back(
									 static_cast<uint8_t>(std::clamp(bytes[i].unsafe_cast<int>(), 0, 255)));
						 }
						 c->setFixtureChannels(fixtureId, values);
					 })
		.addFunction("fillGroup", [](DMXSequenceComponent* c, int r, int g, int b)
					 { c->fillGroup((uint8_t)r, (uint8_t)g, (uint8_t)b); })
		// Content a handler picks in its start callback. Both last only for the
		// run they were set for and never write the definition.
		.addProperty("contentSource", [](DMXSequenceComponent* c) -> int
					 { return (int)c->getDMXSequenceDefinition()->getContentSource(); })
		.addProperty("brightness",
					 [](DMXSequenceComponent* c) -> float { return c->getDMXSequenceDefinition()->getBrightness(); })
		.addFunction("setText", [](DMXSequenceComponent* c, const std::string& text) { c->setRuntimeText(text); })
		.addFunction("setContentPath",
					 [](DMXSequenceComponent* c, const std::string& path) { c->setRuntimeContentPath(path); })
		.addFunction("getText", [](DMXSequenceComponent* c) -> std::string { return c->getEffectiveText(); })
		.addFunction("getContentPath",
					 [](DMXSequenceComponent* c) -> std::string { return c->getEffectiveContentPath().string(); })
		.endClass();
}
