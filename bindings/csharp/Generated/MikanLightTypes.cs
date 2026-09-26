// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanDMXBufferFormat
	{
		DMXUncompressed= 0,
		DMXRLEEncoded= 1,
	};

	public enum MikanDMXScrollDirection
	{
		Left= 0,
		Right= 1,
		Up= 2,
		Down= 3,
	};

	public enum MikanDMXSequenceContentSource
	{
		Script= 0,
		ScrollBitmap= 1,
		ScrollText= 2,
		PlayAnimation= 3,
	};

	public enum MikanPixelGridOrigin
	{
		UpperLeft= 0,
		UpperRight= 1,
		LowerLeft= 2,
		LowerRight= 3,
	};

	public class MikanDMXData
	{
		[MikanFieldOrder(0)] public double server_time_seconds;
		[MikanFieldOrder(1)] public List<MikanUniverseDMXData> universes;
	};

	public class MikanDMXFixtureComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public int stage_id;
		[MikanFieldOrder(1)] public ushort dmx_universe;
		[MikanFieldOrder(2)] public ushort dmx_start_channel;
		[MikanFieldOrder(3)] public ushort dmx_channel_count;
		[MikanFieldOrder(4)] public bool is_disabled;
		[MikanFieldOrder(5)] public float max_wattage;
		[MikanFieldOrder(6)] public float lumens_per_watt;
	};

	public class MikanDMXFixtureGroupComponentValues : MikanComponentValues
	{
		[MikanFieldOrder(0)] public int stage_id;
		[MikanFieldOrder(1)] public List<int> fixture_ids;
	};

	public class MikanDMXFixtureGroupSystemValues : MikanSystemValues
	{
	};

	public class MikanDMXObjectSystemValues : MikanSystemValues
	{
		[MikanFieldOrder(0)] public string network_interface_ip;
		[MikanFieldOrder(1)] public byte dmx_priority;
		[MikanFieldOrder(2)] public float transmit_rate_hz;
	};

	public class MikanDMXPresetComponentValues : MikanComponentValues
	{
		[MikanFieldOrder(0)] public int group_id;
		[MikanFieldOrder(1)] public List<int> fixture_ids;
		[MikanFieldOrder(2)] public List<int> channel_counts;
		[MikanFieldOrder(3)] public List<byte> channel_data;
	};

	public class MikanDMXPresetSystemValues : MikanSystemValues
	{
	};

	public class MikanDMXSequenceComponentValues : MikanComponentValues
	{
		[MikanFieldOrder(0)] public int group_id;
		[MikanFieldOrder(1)] public int script_component_id;
		[MikanFieldOrder(2)] public float duration_seconds;
		[MikanFieldOrder(3)] public bool loop;
		[MikanFieldOrder(4)] public int playback_state;
		[MikanFieldOrder(5)] public float time_since_start;
		[MikanFieldOrder(6)] public MikanDMXSequenceContentSource content_source;
		[MikanFieldOrder(7)] public string content_path;
		[MikanFieldOrder(8)] public string scroll_text;
		[MikanFieldOrder(9)] public string font_path;
		[MikanFieldOrder(10)] public int text_pixel_height;
		[MikanFieldOrder(11)] public MikanVector3f foreground_color;
		[MikanFieldOrder(12)] public MikanVector3f background_color;
		[MikanFieldOrder(13)] public MikanDMXScrollDirection scroll_direction;
		[MikanFieldOrder(14)] public float scroll_speed;
		[MikanFieldOrder(15)] public int sprite_frame_width;
		[MikanFieldOrder(16)] public int sprite_frame_height;
		[MikanFieldOrder(17)] public float sprite_fps;
		[MikanFieldOrder(18)] public float playback_speed_scale;
		[MikanFieldOrder(19)] public float brightness;
	};

	public class MikanDMXSequenceSystemValues : MikanSystemValues
	{
	};

	public class MikanLightEnvironmentComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public List<float> sh_coefficients;
		[MikanFieldOrder(1)] public float exposure_scale;
		[MikanFieldOrder(2)] public float directionality;
		[MikanFieldOrder(3)] public MikanVector3f key_light_direction;
	};

	public class MikanLightEnvironmentSystemValues : MikanSystemValues
	{
	};

	public class MikanRGBPixelGridComponentValues : MikanDMXFixtureComponentValues
	{
		[MikanFieldOrder(0)] public int grid_columns;
		[MikanFieldOrder(1)] public int grid_rows;
		[MikanFieldOrder(2)] public MikanVector3f pixel_size_mm;
		[MikanFieldOrder(3)] public MikanVector2f pixel_separation_mm;
		[MikanFieldOrder(4)] public MikanPixelGridOrigin origin_pixel;
		[MikanFieldOrder(5)] public bool zig_zag;
	};

	public class MikanRGBPixelGridSystemValues : MikanSystemValues
	{
	};

	public class MikanRGBSpotLightComponentValues : MikanDMXFixtureComponentValues
	{
		[MikanFieldOrder(0)] public float cone_angle_degrees;
		[MikanFieldOrder(1)] public float cone_range_meters;
	};

	public class MikanRGBSpotLightSystemValues : MikanSystemValues
	{
	};

	public class MikanUniverseDMXData
	{
		[MikanFieldOrder(0)] public ushort dmx_universe_id;
		[MikanFieldOrder(1)] public MikanDMXBufferFormat buffer_format;
		[MikanFieldOrder(2)] public List<byte> buffer_data;
	};

}
