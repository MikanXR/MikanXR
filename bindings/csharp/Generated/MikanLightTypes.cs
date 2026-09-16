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
		public double server_time_seconds;
		public List<MikanUniverseDMXData> universes;
	};

	public class MikanDMXFixtureComponentValues : MikanTransformComponentValues
	{
		public int stage_id;
		public ushort dmx_universe;
		public ushort dmx_start_channel;
		public ushort dmx_channel_count;
		public bool is_disabled;
	};

	public class MikanDMXFixtureGroupComponentValues : MikanComponentValues
	{
		public int stage_id;
		public List<int> fixture_ids;
	};

	public class MikanDMXFixtureGroupSystemValues : MikanSystemValues
	{
	};

	public class MikanDMXObjectSystemValues : MikanSystemValues
	{
		public string network_interface_ip;
		public byte dmx_priority;
		public float transmit_rate_hz;
	};

	public class MikanDMXPresetComponentValues : MikanComponentValues
	{
		public int group_id;
		public List<int> fixture_ids;
		public List<int> channel_counts;
		public List<byte> channel_data;
	};

	public class MikanDMXPresetSystemValues : MikanSystemValues
	{
	};

	public class MikanDMXSequenceComponentValues : MikanComponentValues
	{
		public int group_id;
		public int script_component_id;
		public float duration_seconds;
		public bool loop;
		public int playback_state;
		public float time_since_start;
		public MikanDMXSequenceContentSource content_source;
		public string content_path;
		public string scroll_text;
		public string font_path;
		public int text_pixel_height;
		public MikanVector3f foreground_color;
		public MikanVector3f background_color;
		public MikanDMXScrollDirection scroll_direction;
		public float scroll_speed;
		public int sprite_frame_width;
		public int sprite_frame_height;
		public float sprite_fps;
		public float playback_speed_scale;
		public float brightness;
	};

	public class MikanDMXSequenceSystemValues : MikanSystemValues
	{
	};

	public class MikanLightEnvironmentComponentValues : MikanTransformComponentValues
	{
		public List<float> sh_coefficients;
		public float exposure_scale;
		public float directionality;
		public MikanVector3f key_light_direction;
	};

	public class MikanLightEnvironmentSystemValues : MikanSystemValues
	{
	};

	public class MikanRGBPixelGridComponentValues : MikanDMXFixtureComponentValues
	{
		public int grid_columns;
		public int grid_rows;
		public MikanVector3f pixel_size_mm;
		public MikanVector2f pixel_separation_mm;
		public MikanPixelGridOrigin origin_pixel;
		public bool zig_zag;
	};

	public class MikanRGBPixelGridSystemValues : MikanSystemValues
	{
	};

	public class MikanRGBSpotLightComponentValues : MikanDMXFixtureComponentValues
	{
		public float cone_angle_degrees;
		public float cone_range_meters;
	};

	public class MikanRGBSpotLightSystemValues : MikanSystemValues
	{
	};

	public class MikanUniverseDMXData
	{
		public ushort dmx_universe_id;
		public MikanDMXBufferFormat buffer_format;
		public List<byte> buffer_data;
	};

}
