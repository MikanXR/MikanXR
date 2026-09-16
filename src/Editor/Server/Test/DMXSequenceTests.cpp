#include "DMXSequenceTests.h"
#include "unit_test.h"

#include "Light/DMXSequenceComponent.h"
#include "Light/DMXSequenceContent.h"
#include "Light/RGBPixelGridComponent.h"

#include <array>
#include <assert.h>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

namespace
{
DMXSequenceComponentPtr makeSequenceComponent()
{
	auto component= std::make_shared<DMXSequenceComponent>(MikanObjectWeakPtr());
	component->setDefinition(std::make_shared<DMXSequenceDefinition>());
	return component;
}

// CommonConfig is non-copyable, so the definition is built on the heap
RGBPixelGridDefinitionPtr makeSequenceTestGrid(int columns, int rows, eDMXPixelGridOrigin origin, bool bZigZag)
{
	auto definition= std::make_shared<RGBPixelGridDefinition>();
	definition->resizeGrid(columns, rows);
	definition->setOriginPixel(origin);
	definition->setZigZag(bZigZag);
	return definition;
}

// A canvas whose every pixel encodes its own coordinates, so a blit can be
// checked cell by cell
DMXPixelCanvas makeCoordinateCanvas(int width, int height)
{
	DMXPixelCanvas canvas;
	canvas.resize(width, height);

	for (int y= 0; y < height; ++y)
		for (int x= 0; x < width; ++x)
			canvas.setPixel(x, y, (uint8_t)(x + 1), (uint8_t)(y + 1), 200);

	return canvas;
}
} // namespace

bool run_dmx_sequence_tests()
{
	UNIT_TEST_MODULE_BEGIN("dmx_sequence")
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_json_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_advance_time);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_frame_buffer_writers);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_scroll_offset);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_animation_frame);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_sprite_sheet_slicing);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_utf8_decode);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_canvas_blit);
	UNIT_TEST_MODULE_CALL_TEST(dmx_sequence_test_brightness);
	UNIT_TEST_MODULE_END()
}

bool dmx_sequence_test_json_round_trip()
{
	UNIT_TEST_BEGIN("definition round-trips through JSON")

	DMXSequenceDefinition source;
	source.setGroupId(1263);
	source.setScriptComponentId(1234);
	source.setDurationSeconds(2.5f);
	source.setLoop(false);
	source.setContentSource(eDMXSequenceContentSource::scrollText);
	source.setContentPath("textures/banner.gif");
	source.setScrollText("\xE3\x81\x82 Mikan");
	source.setTextPixelHeight(12);
	source.setForegroundColor({0.25f, 0.5f, 0.75f});
	source.setBackgroundColor({0.125f, 0.f, 1.f});
	source.setScrollDirection(eDMXScrollDirection::up);
	source.setScrollSpeed(24.f);
	source.setSpriteFrameWidth(16);
	source.setSpriteFrameHeight(8);
	source.setSpriteFps(24.f);
	source.setPlaybackSpeedScale(0.5f);
	source.setBrightness(0.4f);

	DMXSequenceDefinition restored;
	restored.readFromJSON(source.writeToJSON());

	success= (restored.getGroupId() == 1263);
	assert(success);
	success&= (restored.getScriptComponentId() == 1234);
	assert(success);
	success&= fabsf(restored.getDurationSeconds() - 2.5f) < 1e-5f;
	assert(success);
	success&= !restored.getLoop();
	assert(success);

	success&= (restored.getContentSource() == eDMXSequenceContentSource::scrollText);
	assert(success);
	success&= (restored.getContentPath() == std::filesystem::path("textures/banner.gif"));
	assert(success);
	success&= (restored.getScrollText() == "\xE3\x81\x82 Mikan");
	assert(success);
	success&= (restored.getTextPixelHeight() == 12);
	assert(success);
	success&= fabsf(restored.getForegroundColor().y - 0.5f) < 1e-5f;
	assert(success);
	success&= fabsf(restored.getBackgroundColor().x - 0.125f) < 1e-5f;
	assert(success);
	success&= (restored.getScrollDirection() == eDMXScrollDirection::up);
	assert(success);
	success&= fabsf(restored.getScrollSpeed() - 24.f) < 1e-5f;
	assert(success);
	success&= (restored.getSpriteFrameWidth() == 16 && restored.getSpriteFrameHeight() == 8);
	assert(success);
	success&= fabsf(restored.getSpriteFps() - 24.f) < 1e-5f;
	assert(success);
	success&= fabsf(restored.getPlaybackSpeedScale() - 0.5f) < 1e-5f;
	assert(success);
	success&= fabsf(restored.getBrightness() - 0.4f) < 1e-5f;
	assert(success);

	// A project saved before the content sources existed keeps its behaviour:
	// none of the new keys are present, and it loads as script-driven with the
	// colors at their defaults rather than at zero
	configuru::Config legacy= source.writeToJSON();
	legacy.erase(DMXSequenceDefinition::k_contentSourcePropertyId);
	legacy.erase(DMXSequenceDefinition::k_foregroundColorPropertyId);
	legacy.erase(DMXSequenceDefinition::k_backgroundColorPropertyId);
	legacy.erase(DMXSequenceDefinition::k_scrollDirectionPropertyId);
	legacy.erase(DMXSequenceDefinition::k_brightnessPropertyId);

	DMXSequenceDefinition migrated;
	migrated.readFromJSON(legacy);
	success&= (migrated.getContentSource() == eDMXSequenceContentSource::script);
	assert(success);
	success&= (migrated.getScrollDirection() == eDMXScrollDirection::left);
	assert(success);
	success&= (migrated.getForegroundColor().x == 1.f && migrated.getBackgroundColor().x == 0.f);
	assert(success);
	// A sequence that predates the dimmer plays at full output
	success&= (migrated.getBrightness() == 1.f);
	assert(success);

	// A project saved with a handler name has no script to map it to, so the
	// sequence loads unassigned and is picked again in the panel
	configuru::Config named= source.writeToJSON();
	named.erase(DMXSequenceDefinition::k_scriptComponentIdPropertyId);
	named["sequence_name"]= "chase";
	DMXSequenceDefinition unassigned;
	unassigned.readFromJSON(named);
	success&= (unassigned.getScriptComponentId() == INVALID_MIKAN_ID);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_advance_time()
{
	UNIT_TEST_BEGIN("advanceTime wraps, finishes, or runs unbounded")

	bool bWrapped= false;
	bool bFinished= false;

	// Looping wraps past the duration
	float time= 1.8f;
	DMXSequenceComponent::advanceTime(time, 0.5f, 2.f, true, bWrapped, bFinished);
	success= bWrapped && !bFinished && fabsf(time - 0.3f) < 1e-5f;
	assert(success);

	// Not looping clamps to the duration and finishes
	time= 1.8f;
	DMXSequenceComponent::advanceTime(time, 0.5f, 2.f, false, bWrapped, bFinished);
	success&= !bWrapped && bFinished && time == 2.f;
	assert(success);

	// Inside the duration nothing happens
	time= 0.5f;
	DMXSequenceComponent::advanceTime(time, 0.25f, 2.f, false, bWrapped, bFinished);
	success&= !bWrapped && !bFinished && fabsf(time - 0.75f) < 1e-5f;
	assert(success);

	// Zero duration is unbounded
	time= 100.f;
	DMXSequenceComponent::advanceTime(time, 1.f, 0.f, false, bWrapped, bFinished);
	success&= !bWrapped && !bFinished && time == 101.f;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_frame_buffer_writers()
{
	UNIT_TEST_BEGIN("frame buffer writers grow and overwrite slices")

	DMXSequenceComponentPtr sequence= makeSequenceComponent();

	std::vector<uint8_t> values;
	success= !sequence->getFrameBufferValues(1223, values);
	assert(success);

	// A color write creates a three-byte slice
	sequence->setFixtureColor(1223, 255, 40, 0);
	success&= sequence->getFrameBufferValues(1223, values) && values == std::vector<uint8_t>{255, 40, 0};
	assert(success);

	// A channel write replaces the slice at any length
	sequence->setFixtureChannels(1223, {1, 2, 3, 4, 5, 6});
	success&= sequence->getFrameBufferValues(1223, values) && values.size() == 6 && values[5] == 6;
	assert(success);

	// A color write onto a longer slice keeps the tail
	sequence->setFixtureColor(1223, 9, 9, 9);
	success&= sequence->getFrameBufferValues(1223, values) && values == std::vector<uint8_t>{9, 9, 9, 4, 5, 6};
	assert(success);

	// Without a group a pixel write is ignored rather than guessing a layout
	sequence->setPixel(1091, 0, 0, 1, 2, 3);
	success&= !sequence->getFrameBufferValues(1091, values);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_scroll_offset()
{
	UNIT_TEST_BEGIN("the scroll sweep carries the content fully across the grid")

	const int contentExtent= 40;
	const int gridExtent= 16;
	const float speed= 8.f;

	// One sweep runs the content in from one side and out the other
	const float period= DMXSequenceContent::computeScrollPeriod(speed, contentExtent, gridExtent);
	success= fabsf(period - (float)(contentExtent + gridExtent) / speed) < 1e-5f;
	assert(success);

	// Scrolling left starts with the window a whole grid before the content
	float offset= DMXSequenceContent::computeScrollOffset(0.f, speed, eDMXScrollDirection::left, contentExtent,
														  gridExtent, false);
	success&= fabsf(offset + (float)gridExtent) < 1e-5f;
	assert(success);

	// and ends past its far edge, so the last pixel has cleared the grid
	offset= DMXSequenceContent::computeScrollOffset(period, speed, eDMXScrollDirection::left, contentExtent, gridExtent,
													false);
	success&= fabsf(offset - (float)contentExtent) < 1e-5f;
	assert(success);

	// Right is the same sweep run backwards
	success&= fabsf(DMXSequenceContent::computeScrollOffset(0.f, speed, eDMXScrollDirection::right, contentExtent,
															gridExtent, false)
					- (float)contentExtent)
			  < 1e-5f;
	assert(success);
	success&= fabsf(DMXSequenceContent::computeScrollOffset(period, speed, eDMXScrollDirection::right, contentExtent,
															gridExtent, false)
					+ (float)gridExtent)
			  < 1e-5f;
	assert(success);

	// Up and down travel the same distances as left and right
	success&= fabsf(DMXSequenceContent::computeScrollOffset(1.f, speed, eDMXScrollDirection::up, contentExtent,
															gridExtent, false)
					- DMXSequenceContent::computeScrollOffset(1.f, speed, eDMXScrollDirection::left, contentExtent,
															  gridExtent, false))
			  < 1e-5f;
	assert(success);

	// Looping wraps on the sweep, so one period in is back at the start
	success&= fabsf(DMXSequenceContent::computeScrollOffset(period, speed, eDMXScrollDirection::left, contentExtent,
															gridExtent, true)
					+ (float)gridExtent)
			  < 1e-5f;
	assert(success);
	success&= fabsf(DMXSequenceContent::computeScrollOffset(period + 1.f, speed, eDMXScrollDirection::left,
															contentExtent, gridExtent, true)
					- DMXSequenceContent::computeScrollOffset(1.f, speed, eDMXScrollDirection::left, contentExtent,
															  gridExtent, true))
			  < 1e-4f;
	assert(success);

	// A stopped scroll has no period rather than dividing by zero
	success&= DMXSequenceContent::computeScrollPeriod(0.f, contentExtent, gridExtent) == 0.f;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_animation_frame()
{
	UNIT_TEST_BEGIN("the animation frame follows the delays and the speed scale")

	const std::vector<int> delays= {100, 200, 100};

	success= fabsf(DMXSequenceContent::computeAnimationPeriod(delays, 1.f) - 0.4f) < 1e-5f;
	assert(success);
	// Twice the speed is half the period
	success&= fabsf(DMXSequenceContent::computeAnimationPeriod(delays, 2.f) - 0.2f) < 1e-5f;
	assert(success);

	// Each frame holds for its own delay
	success&= DMXSequenceContent::computeAnimationFrame(0.f, delays, 1.f, true) == 0;
	assert(success);
	success&= DMXSequenceContent::computeAnimationFrame(0.05f, delays, 1.f, true) == 0;
	assert(success);
	success&= DMXSequenceContent::computeAnimationFrame(0.15f, delays, 1.f, true) == 1;
	assert(success);
	success&= DMXSequenceContent::computeAnimationFrame(0.25f, delays, 1.f, true) == 1;
	assert(success);
	success&= DMXSequenceContent::computeAnimationFrame(0.35f, delays, 1.f, true) == 2;
	assert(success);

	// Looping wraps back to the first frame
	success&= DMXSequenceContent::computeAnimationFrame(0.45f, delays, 1.f, true) == 0;
	assert(success);

	// Not looping holds the last frame instead
	success&= DMXSequenceContent::computeAnimationFrame(0.45f, delays, 1.f, false) == 2;
	assert(success);
	success&= DMXSequenceContent::computeAnimationFrame(100.f, delays, 1.f, false) == 2;
	assert(success);

	// The speed scale moves the frame boundaries with the period
	success&= DMXSequenceContent::computeAnimationFrame(0.15f, delays, 2.f, true) == 2;
	assert(success);

	// No frames is answered rather than indexed into
	success&= DMXSequenceContent::computeAnimationFrame(0.f, {}, 1.f, true) == -1;
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_sprite_sheet_slicing()
{
	UNIT_TEST_BEGIN("a sprite sheet splits left to right, then top to bottom")

	// Two columns by two rows of 3x2 frames
	const DMXPixelCanvas sheet= makeCoordinateCanvas(6, 4);

	std::vector<DMXPixelCanvas> frames;
	success= DMXSequenceContent::sliceSpriteSheet(sheet, 3, 2, frames) && frames.size() == 4;
	assert(success);

	// Frame 1 is the top right cell, so its first pixel is the sheet's (3, 0)
	uint8_t pixel[3];
	frames[1].getPixel(0, 0, nullptr, pixel);
	success&= (pixel[0] == 4 && pixel[1] == 1);
	assert(success);

	// Frame 2 starts the second row, at the sheet's (0, 2)
	frames[2].getPixel(0, 0, nullptr, pixel);
	success&= (pixel[0] == 1 && pixel[1] == 3);
	assert(success);

	// A zero frame size means square frames the height of the sheet
	const DMXPixelCanvas strip= makeCoordinateCanvas(12, 4);
	success&= DMXSequenceContent::sliceSpriteSheet(strip, 0, 0, frames) && frames.size() == 3;
	assert(success);
	success&= (frames[0].width == 4 && frames[0].height == 4);
	assert(success);

	// A frame larger than the sheet is refused rather than reading past it
	success&= !DMXSequenceContent::sliceSpriteSheet(sheet, 100, 2, frames);
	assert(success);
	success&= !DMXSequenceContent::sliceSpriteSheet(DMXPixelCanvas(), 3, 2, frames);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_utf8_decode()
{
	UNIT_TEST_BEGIN("UTF-8 decodes every sequence length and survives bad input")

	std::vector<uint32_t> codepoints;

	DMXSequenceContent::decodeUtf8("AB", codepoints);
	success= (codepoints.size() == 2 && codepoints[0] == 'A' && codepoints[1] == 'B');
	assert(success);

	// Two, three, and four byte sequences: U+00E9, U+3042, U+1F600
	DMXSequenceContent::decodeUtf8("\xC3\xA9\xE3\x81\x82\xF0\x9F\x98\x80", codepoints);
	success&=
		(codepoints.size() == 3 && codepoints[0] == 0xE9u && codepoints[1] == 0x3042u && codepoints[2] == 0x1F600u);
	assert(success);

	// A stray continuation byte becomes one replacement and the scan moves on
	DMXSequenceContent::decodeUtf8("\x80"
								   "A",
								   codepoints);
	success&= (codepoints.size() == 2 && codepoints[0] == 0xFFFDu && codepoints[1] == 'A');
	assert(success);

	// A truncated sequence at the end does not read past the string
	DMXSequenceContent::decodeUtf8("A\xE3\x81", codepoints);
	success&= (codepoints.size() >= 2 && codepoints[0] == 'A' && codepoints[1] == 0xFFFDu);
	assert(success);

	DMXSequenceContent::decodeUtf8("", codepoints);
	success&= codepoints.empty();
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_canvas_blit()
{
	UNIT_TEST_BEGIN("a canvas window lands at each cell's wire position")

	const int columns= 4;
	const int rows= 3;
	const DMXPixelCanvas canvas= makeCoordinateCanvas(columns, rows);
	const uint8_t background[3]= {7, 8, 9};

	// Upper left with no zig-zag is the identity layout, so cell (col, row)
	// sits at row * columns + col
	RGBPixelGridDefinitionPtr grid= makeSequenceTestGrid(columns, rows, eDMXPixelGridOrigin::upperLeft, false);
	std::vector<uint8_t> values;
	DMXSequenceContent::blitWindow(canvas, 0, 0, *grid, background, values);

	success= (values.size() == (size_t)columns * rows * 3);
	assert(success);
	for (int row= 0; row < rows && success; ++row)
	{
		for (int col= 0; col < columns && success; ++col)
		{
			const size_t offset= (size_t)(row * columns + col) * 3;
			success&= (values[offset] == (uint8_t)(col + 1) && values[offset + 1] == (uint8_t)(row + 1));
			assert(success);
		}
	}

	// The same window through a reversed, serpentine layout puts every cell
	// where its wire index says, not where its coordinates would
	grid= makeSequenceTestGrid(columns, rows, eDMXPixelGridOrigin::lowerRight, true);
	DMXSequenceContent::blitWindow(canvas, 0, 0, *grid, background, values);
	for (int row= 0; row < rows && success; ++row)
	{
		for (int col= 0; col < columns && success; ++col)
		{
			const size_t offset= (size_t)grid->getPixelWireIndex(col, row) * 3;
			success&= (values[offset] == (uint8_t)(col + 1) && values[offset + 1] == (uint8_t)(row + 1));
			assert(success);
		}
	}

	// A window shifted past the content leaves the background behind
	grid= makeSequenceTestGrid(columns, rows, eDMXPixelGridOrigin::upperLeft, false);
	DMXSequenceContent::blitWindow(canvas, 2, 0, *grid, background, values);
	// Cell (2, 0) reads the canvas at x = 4, which is outside it
	const size_t pastOffset= (size_t)grid->getPixelWireIndex(2, 0) * 3;
	success&= (values[pastOffset] == background[0] && values[pastOffset + 1] == background[1]
			   && values[pastOffset + 2] == background[2]);
	assert(success);
	// while cell (0, 0) has slid onto the canvas's x = 2
	const size_t shiftedOffset= (size_t)grid->getPixelWireIndex(0, 0) * 3;
	success&= (values[shiftedOffset] == 3);
	assert(success);

	UNIT_TEST_COMPLETE()
}

bool dmx_sequence_test_brightness()
{
	UNIT_TEST_BEGIN("the dimmer scales channels and matches an HSV value scale")

	// Full brightness is the identity, and is skipped rather than rounded
	std::vector<uint8_t> values= {255, 128, 0, 7};
	DMXSequenceComponent::applyBrightness(values, 1.f);
	success= (values == std::vector<uint8_t>{255, 128, 0, 7});
	assert(success);

	// Half, rounded to nearest
	values= {255, 128, 0, 7};
	DMXSequenceComponent::applyBrightness(values, 0.5f);
	success&= (values == std::vector<uint8_t>{128, 64, 0, 4});
	assert(success);

	// Zero is off, and out of range values are clamped rather than wrapping
	values= {255, 128, 3};
	DMXSequenceComponent::applyBrightness(values, 0.f);
	success&= (values == std::vector<uint8_t>{0, 0, 0});
	assert(success);
	values= {255, 128, 3};
	DMXSequenceComponent::applyBrightness(values, 5.f);
	success&= (values == std::vector<uint8_t>{255, 128, 3});
	assert(success);
	values= {255, 128, 3};
	DMXSequenceComponent::applyBrightness(values, -1.f);
	success&= (values == std::vector<uint8_t>{0, 0, 0});
	assert(success);

	// The byte multiply is what an HSV round trip that scales V produces: for a
	// fixed hue and saturation every channel is linear in V. Checked against a
	// reference conversion rather than asserted.
	const std::vector<std::array<uint8_t, 3>> colors= {{200, 100, 50}, {255, 0, 0},     {13, 200, 255},
													   {0, 0, 0},      {255, 255, 255}, {90, 90, 12}};
	for (const float scale : {0.25f, 0.5f, 0.75f})
	{
		for (const std::array<uint8_t, 3>& color : colors)
		{
			const float r= color[0] / 255.f, g= color[1] / 255.f, b= color[2] / 255.f;
			const float maxChannel= std::max({r, g, b});
			const float minChannel= std::min({r, g, b});
			const float chroma= maxChannel - minChannel;

			// Rebuild RGB from hue, saturation, and a scaled value
			const float scaledValue= maxChannel * scale;
			const float saturation= maxChannel > 0.f ? chroma / maxChannel : 0.f;
			const float scaledChroma= scaledValue * saturation;
			float hue= 0.f;
			if (chroma > 0.f)
			{
				if (maxChannel == r)
					hue= 60.f * std::fmod((g - b) / chroma, 6.f);
				else if (maxChannel == g)
					hue= 60.f * (((b - r) / chroma) + 2.f);
				else
					hue= 60.f * (((r - g) / chroma) + 4.f);
			}
			if (hue < 0.f)
				hue+= 360.f;

			const float x= scaledChroma * (1.f - std::fabs(std::fmod(hue / 60.f, 2.f) - 1.f));
			const float m= scaledValue - scaledChroma;
			float rgb[3]= {m, m, m};
			const int sector= (int)(hue / 60.f) % 6;
			const int order[6][2]= {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 0}, {0, 2}};
			rgb[order[sector][0]]+= scaledChroma;
			rgb[order[sector][1]]+= x;

			std::vector<uint8_t> scaled= {color[0], color[1], color[2]};
			DMXSequenceComponent::applyBrightness(scaled, scale);

			for (int channel= 0; channel < 3; ++channel)
			{
				const int fromHsv= std::clamp((int)std::lround(rgb[channel] * 255.f), 0, 255);
				success&= (std::abs((int)scaled[channel] - fromHsv) <= 1);
				assert(success);
			}
		}
	}

	UNIT_TEST_COMPLETE()
}
