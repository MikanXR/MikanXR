#pragma once

#include "AssetFwd.h"
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "DMXFixtureComponent.h"
#include "DMXSequenceContent.h"
#include "LightSystemFwd.h"
#include "MikanComponent.h"
#include "MikanLightTypes.h"
#include "MikanTypeFwd.h"
#include "ScriptingFwd.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class eDMXSequenceState : int
{
	Stopped= 0,
	Playing= 1,
	Paused= 2,
};

// Where a sequence's pixels come from. Everything else about a sequence, the
// group binding, the playback controls, the frame buffer, is shared across all
// four; only this decides who fills the buffer each frame.
enum class eDMXSequenceContentSource : int
{
	INVALID= -1,

	// A Lua handler writes the frame buffer itself
	script= 0,
	// The editor rasterizes and scrolls a still image
	scrollBitmap,
	// The editor rasterizes and scrolls a line of UTF-8 text
	scrollText,
	// The editor plays an animation's frames on their own timing
	playAnimation,

	COUNT
};
extern const std::string* k_dmxSequenceContentSourceStrings;

// -- DMXSequenceDefinition -----
// A script-driven animation of one fixture group: the SequenceUpdate method
// of the named script component's behavior is called every frame while the
// sequence plays and writes into the sequence's frame buffer.
class DMXSequenceDefinition : public MikanComponentDefinition
{
public:
	DMXSequenceDefinition();
	DMXSequenceDefinition(MikanDMXSequenceID sequenceId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanDMXSequenceID getSequenceId() const { return getComponentId(); }

	static const std::string k_groupIdPropertyId;
	inline MikanDMXFixtureGroupID getGroupId() const { return m_groupId; }
	void setGroupId(MikanDMXFixtureGroupID groupId);

	// The script component whose behavior drives the sequence, INVALID_MIKAN_ID for none
	static const std::string k_scriptComponentIdPropertyId;
	inline MikanScriptID getScriptComponentId() const { return m_scriptComponentId; }
	void setScriptComponentId(MikanScriptID scriptComponentId);

	// Zero or less runs until stopped
	static const std::string k_durationSecondsPropertyId;
	inline float getDurationSeconds() const { return m_durationSeconds; }
	void setDurationSeconds(float durationSeconds);

	static const std::string k_loopPropertyId;
	inline bool getLoop() const { return m_bLoop; }
	void setLoop(bool bLoop);

	static const std::string k_contentSourcePropertyId;
	inline eDMXSequenceContentSource getContentSource() const { return m_contentSource; }
	void setContentSource(eDMXSequenceContentSource contentSource);

	// The still image a scrolling bitmap shows, or the GIF or sprite sheet an
	// animation plays. One property, since only one source is live at a time.
	static const std::string k_contentPathPropertyId;
	std::filesystem::path getContentPath() const;
	void setContentPath(const std::filesystem::path& contentPath);

	static const std::string k_scrollTextPropertyId;
	inline const std::string& getScrollText() const { return m_scrollText; }
	void setScrollText(const std::string& scrollText);

	// Empty falls back to the bundled face, which carries kana and kanji
	static const std::string k_fontPathPropertyId;
	std::filesystem::path getFontPath() const;
	void setFontPath(const std::filesystem::path& fontPath);

	// Zero means the grid's own row count
	static const std::string k_textPixelHeightPropertyId;
	inline int getTextPixelHeight() const { return m_textPixelHeight; }
	void setTextPixelHeight(int textPixelHeight);

	static const std::string k_foregroundColorPropertyId;
	inline const MikanVector3f& getForegroundColor() const { return m_foregroundColor; }
	void setForegroundColor(const MikanVector3f& color);

	// Also fills the grid wherever the content does not reach
	static const std::string k_backgroundColorPropertyId;
	inline const MikanVector3f& getBackgroundColor() const { return m_backgroundColor; }
	void setBackgroundColor(const MikanVector3f& color);

	static const std::string k_scrollDirectionPropertyId;
	inline eDMXScrollDirection getScrollDirection() const { return m_scrollDirection; }
	void setScrollDirection(eDMXScrollDirection scrollDirection);

	static const std::string k_scrollSpeedPropertyId;
	inline float getScrollSpeed() const { return m_scrollSpeed; }
	void setScrollSpeed(float scrollSpeed);

	// Zero means square frames the height of the sheet. Ignored for a GIF.
	static const std::string k_spriteFrameWidthPropertyId;
	inline int getSpriteFrameWidth() const { return m_spriteFrameWidth; }
	void setSpriteFrameWidth(int spriteFrameWidth);

	static const std::string k_spriteFrameHeightPropertyId;
	inline int getSpriteFrameHeight() const { return m_spriteFrameHeight; }
	void setSpriteFrameHeight(int spriteFrameHeight);

	static const std::string k_spriteFpsPropertyId;
	inline float getSpriteFps() const { return m_spriteFps; }
	void setSpriteFps(float spriteFps);

	// Scales a GIF's own delays and a sprite sheet's frame rate alike
	static const std::string k_playbackSpeedScalePropertyId;
	inline float getPlaybackSpeedScale() const { return m_playbackSpeedScale; }
	void setPlaybackSpeedScale(float playbackSpeedScale);

	// A 0 to 1 dimmer on everything the sequence sends, since a source image
	// authored for a screen is usually too bright on a panel. The panel draws
	// it as a percentage, and the range is enforced where it is applied.
	static const std::string k_brightnessPropertyId;
	inline float getBrightness() const { return m_brightness; }
	void setBrightness(float brightness);

private:
	MikanDMXFixtureGroupID m_groupId= INVALID_MIKAN_ID;
	MikanScriptID m_scriptComponentId= INVALID_MIKAN_ID;
	float m_durationSeconds= 10.f;
	bool m_bLoop= true;

	eDMXSequenceContentSource m_contentSource= eDMXSequenceContentSource::script;
	AssetReferenceConfigPtr m_contentAssetRefConfig;
	std::string m_scrollText= "MikanXR";
	AssetReferenceConfigPtr m_fontAssetRefConfig;
	int m_textPixelHeight= 0;
	MikanVector3f m_foregroundColor= {1.f, 1.f, 1.f};
	MikanVector3f m_backgroundColor= {0.f, 0.f, 0.f};
	eDMXScrollDirection m_scrollDirection= eDMXScrollDirection::left;
	float m_scrollSpeed= 8.f;
	int m_spriteFrameWidth= 0;
	int m_spriteFrameHeight= 0;
	float m_spriteFps= 10.f;
	float m_playbackSpeedScale= 1.f;
	float m_brightness= 1.f;
};

// -- DMXSequenceComponent -----
class DMXSequenceComponent : public MikanComponent
{
public:
	DMXSequenceComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "DMXSequenceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline DMXSequenceDefinitionPtr getDMXSequenceDefinition() const
	{
		return std::static_pointer_cast<DMXSequenceDefinition>(m_definition);
	}

	// The group this sequence animates, null when the id resolves to nothing
	DMXFixtureGroupComponentPtr getGroup() const;

	// -- Playback --
	inline eDMXSequenceState getPlaybackState() const { return m_state; }
	inline float getTimeSinceStart() const { return m_timeSinceStart; }
	inline bool isPlaying() const { return m_state == eDMXSequenceState::Playing; }
	// From Stopped: restart at zero and call the handler's start; from Paused: resume
	void play();
	void pause();
	// Calls the handler's stop and leaves the last frame on the fixtures
	void stop();
	// Advance a playing sequence by one frame: time, the handler's update, then
	// the frame buffer to the fixtures. Called by the owning system.
	void tick(float deltaSeconds);

	// Advance time by delta against a duration (zero or less is unbounded).
	// Looping wraps time; otherwise time clamps to the duration and outFinished
	// is set so the caller runs a final update and stops.
	static void advanceTime(float& inoutTime, float delta, float duration, bool bLoop, bool& outWrapped,
							bool& outFinished);

	// -- Frame buffer, written by the handler and applied after each update --
	inline const DMXFixtureValueMap& getFrameBuffer() const { return m_frameBuffer; }
	bool getFrameBufferValues(MikanLightID fixtureId, std::vector<uint8_t>& outValues) const;
	void setFixtureChannels(MikanLightID fixtureId, const std::vector<uint8_t>& values);
	void setFixtureColor(MikanLightID fixtureId, uint8_t r, uint8_t g, uint8_t b);
	// Ignored unless the fixture is a pixel grid member and the pixel is in range
	void setPixel(MikanLightID fixtureId, int col, int row, uint8_t r, uint8_t g, uint8_t b);
	// Every member: spot lights get one triple, pixel grids every pixel
	void fillGroup(uint8_t r, uint8_t g, uint8_t b);

	// Scale channel bytes by the dimmer, rounding to nearest. Scaling the bytes
	// is what converting to HSV, scaling V, and converting back would produce:
	// for a fixed hue and saturation each of R, G, and B is linear in V, so the
	// round trip reduces to this multiply.
	static void applyBrightness(std::vector<uint8_t>& inoutValues, float brightness);
	// What a fixture is actually sent: the frame buffer slice with the dimmer
	// applied. The panel draws these, so its swatches match the light.
	bool getAppliedFixtureValues(MikanLightID fixtureId, std::vector<uint8_t>& outValues) const;

	// -- Runtime content overrides, for a script picking content on start --
	// Both are cleared on stop, so they never touch the definition and never
	// record a transaction
	void setRuntimeText(const std::string& text);
	void setRuntimeContentPath(const std::filesystem::path& contentPath);
	// The text and path actually in use: the override when one is set,
	// otherwise the definition's own value
	std::string getEffectiveText() const;
	std::filesystem::path getEffectiveContentPath() const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_playbackStatePropertyId;
	static const std::string k_timeSinceStartPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_playFunctionId;
	static const std::string k_pauseFunctionId;
	static const std::string k_stopFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

	// -- IComponent ----
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

private:
	CommonScriptContextPtr getScriptContext() const;
	// Whether this sequence names a loaded script whose behavior has a
	// SequenceUpdate method. A rasterized source may name none, which is not
	// an error.
	bool hasScriptHandler() const;
	// Call one behavior method with (sequence[, time, delta]); false on a Lua
	// error, which is logged with the sequence and script names
	bool callHandler(const char* methodName, bool bWithTime, float deltaSeconds);
	void applyFrameBuffer();

	// The pixel grid a rasterized source draws onto: the group's first grid
	// member. Null when the group holds none.
	RGBPixelGridComponentPtr getTargetPixelGrid() const;
	// Decode or rasterize the current source into m_contentFrames, at most once
	// per play or per content property change. False leaves the frames empty
	// and logs why.
	bool rebuildContent();
	// The period one pass of the current content takes, which stands in for
	// duration_seconds when that is zero or less
	float getContentPeriodSeconds() const;
	// Write this instant of a rasterized source into the frame buffer
	void updateGeneratedContent();

	eDMXSequenceState m_state= eDMXSequenceState::Stopped;
	float m_timeSinceStart= 0.f;
	DMXFixtureValueMap m_frameBuffer;

	// Runtime only: decoded content, and the script's content overrides
	std::vector<DMXPixelCanvas> m_contentFrames;
	std::vector<int> m_contentFrameDelaysMs;
	bool m_bContentDirty= true;
	bool m_bHasRuntimeText= false;
	std::string m_runtimeText;
	bool m_bHasRuntimeContentPath= false;
	std::filesystem::path m_runtimeContentPath;
};
