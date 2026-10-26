// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanDMXBufferFormat
	{
		DMXUncompressed= 0,
		DMXRLEEncoded= 1,
	};

	internal static class MikanDMXBufferFormatJson
	{
		public static JToken ToJson(MikanDMXBufferFormat value)
		{
			switch (value)
			{
				case MikanDMXBufferFormat.DMXUncompressed: return MikanJson.WriteString("DMXUncompressed");
				case MikanDMXBufferFormat.DMXRLEEncoded: return MikanJson.WriteString("DMXRLEEncoded");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanDMXBufferFormat has no wire name for " + (long)value);
		}

		public static MikanDMXBufferFormat FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanDMXBufferFormat)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "DMXUncompressed": return MikanDMXBufferFormat.DMXUncompressed;
				case "DMXRLEEncoded": return MikanDMXBufferFormat.DMXRLEEncoded;
			}
			throw new Exception("MikanDMXBufferFormat has no value named " + name);
		}
	};

	public enum MikanDMXScrollDirection
	{
		Left= 0,
		Right= 1,
		Up= 2,
		Down= 3,
	};

	internal static class MikanDMXScrollDirectionJson
	{
		public static JToken ToJson(MikanDMXScrollDirection value)
		{
			switch (value)
			{
				case MikanDMXScrollDirection.Left: return MikanJson.WriteString("Left");
				case MikanDMXScrollDirection.Right: return MikanJson.WriteString("Right");
				case MikanDMXScrollDirection.Up: return MikanJson.WriteString("Up");
				case MikanDMXScrollDirection.Down: return MikanJson.WriteString("Down");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanDMXScrollDirection has no wire name for " + (long)value);
		}

		public static MikanDMXScrollDirection FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanDMXScrollDirection)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Left": return MikanDMXScrollDirection.Left;
				case "Right": return MikanDMXScrollDirection.Right;
				case "Up": return MikanDMXScrollDirection.Up;
				case "Down": return MikanDMXScrollDirection.Down;
			}
			throw new Exception("MikanDMXScrollDirection has no value named " + name);
		}
	};

	public enum MikanDMXSequenceContentSource
	{
		Script= 0,
		ScrollBitmap= 1,
		ScrollText= 2,
		PlayAnimation= 3,
	};

	internal static class MikanDMXSequenceContentSourceJson
	{
		public static JToken ToJson(MikanDMXSequenceContentSource value)
		{
			switch (value)
			{
				case MikanDMXSequenceContentSource.Script: return MikanJson.WriteString("Script");
				case MikanDMXSequenceContentSource.ScrollBitmap: return MikanJson.WriteString("ScrollBitmap");
				case MikanDMXSequenceContentSource.ScrollText: return MikanJson.WriteString("ScrollText");
				case MikanDMXSequenceContentSource.PlayAnimation: return MikanJson.WriteString("PlayAnimation");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanDMXSequenceContentSource has no wire name for " + (long)value);
		}

		public static MikanDMXSequenceContentSource FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanDMXSequenceContentSource)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Script": return MikanDMXSequenceContentSource.Script;
				case "ScrollBitmap": return MikanDMXSequenceContentSource.ScrollBitmap;
				case "ScrollText": return MikanDMXSequenceContentSource.ScrollText;
				case "PlayAnimation": return MikanDMXSequenceContentSource.PlayAnimation;
			}
			throw new Exception("MikanDMXSequenceContentSource has no value named " + name);
		}
	};

	public enum MikanPixelGridOrigin
	{
		UpperLeft= 0,
		UpperRight= 1,
		LowerLeft= 2,
		LowerRight= 3,
	};

	internal static class MikanPixelGridOriginJson
	{
		public static JToken ToJson(MikanPixelGridOrigin value)
		{
			switch (value)
			{
				case MikanPixelGridOrigin.UpperLeft: return MikanJson.WriteString("UpperLeft");
				case MikanPixelGridOrigin.UpperRight: return MikanJson.WriteString("UpperRight");
				case MikanPixelGridOrigin.LowerLeft: return MikanJson.WriteString("LowerLeft");
				case MikanPixelGridOrigin.LowerRight: return MikanJson.WriteString("LowerRight");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanPixelGridOrigin has no wire name for " + (long)value);
		}

		public static MikanPixelGridOrigin FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanPixelGridOrigin)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "UpperLeft": return MikanPixelGridOrigin.UpperLeft;
				case "UpperRight": return MikanPixelGridOrigin.UpperRight;
				case "LowerLeft": return MikanPixelGridOrigin.LowerLeft;
				case "LowerRight": return MikanPixelGridOrigin.LowerRight;
			}
			throw new Exception("MikanPixelGridOrigin has no value named " + name);
		}
	};

	public class MikanDMXData : MikanSerializableObject
	{
		public double server_time_seconds;
		public List<MikanUniverseDMXData> universes;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("server_time_seconds", MikanJson.WriteDouble(this.server_time_seconds));
			json.Add("universes", MikanJson.WriteList(this.universes, e0 => MikanJson.WriteObject(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.server_time_seconds = MikanJson.ReadDouble(MikanJson.Field(json, "server_time_seconds"));
			this.universes = MikanJson.ReadList(MikanJson.Field(json, "universes"), this.universes, e0 => MikanJson.ReadObject(e0, new MikanUniverseDMXData()));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "server_time_seconds": value = this.server_time_seconds; return true;
				case "universes": value = this.universes; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "server_time_seconds": return MikanJson.TryAssign(value, ref this.server_time_seconds);
				case "universes": return MikanJson.TryAssign(value, ref this.universes);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXFixtureComponentValues : MikanTransformComponentValues
	{
		public int stage_id;
		public ushort dmx_universe;
		public ushort dmx_start_channel;
		public ushort dmx_channel_count;
		public bool is_disabled;
		public float max_wattage;
		public float lumens_per_watt;

		public override string RuntimeClassName => "MikanDMXFixtureComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("stage_id", MikanJson.WriteInt(this.stage_id));
			json.Add("dmx_universe", MikanJson.WriteUShort(this.dmx_universe));
			json.Add("dmx_start_channel", MikanJson.WriteUShort(this.dmx_start_channel));
			json.Add("dmx_channel_count", MikanJson.WriteUShort(this.dmx_channel_count));
			json.Add("is_disabled", MikanJson.WriteBool(this.is_disabled));
			json.Add("max_wattage", MikanJson.WriteFloat(this.max_wattage));
			json.Add("lumens_per_watt", MikanJson.WriteFloat(this.lumens_per_watt));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.stage_id = MikanJson.ReadInt(MikanJson.Field(json, "stage_id"));
			this.dmx_universe = MikanJson.ReadUShort(MikanJson.Field(json, "dmx_universe"));
			this.dmx_start_channel = MikanJson.ReadUShort(MikanJson.Field(json, "dmx_start_channel"));
			this.dmx_channel_count = MikanJson.ReadUShort(MikanJson.Field(json, "dmx_channel_count"));
			this.is_disabled = MikanJson.ReadBool(MikanJson.Field(json, "is_disabled"));
			this.max_wattage = MikanJson.ReadFloat(MikanJson.Field(json, "max_wattage"));
			this.lumens_per_watt = MikanJson.ReadFloat(MikanJson.Field(json, "lumens_per_watt"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "stage_id": value = this.stage_id; return true;
				case "dmx_universe": value = this.dmx_universe; return true;
				case "dmx_start_channel": value = this.dmx_start_channel; return true;
				case "dmx_channel_count": value = this.dmx_channel_count; return true;
				case "is_disabled": value = this.is_disabled; return true;
				case "max_wattage": value = this.max_wattage; return true;
				case "lumens_per_watt": value = this.lumens_per_watt; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "stage_id": return MikanJson.TryAssign(value, ref this.stage_id);
				case "dmx_universe": return MikanJson.TryAssign(value, ref this.dmx_universe);
				case "dmx_start_channel": return MikanJson.TryAssign(value, ref this.dmx_start_channel);
				case "dmx_channel_count": return MikanJson.TryAssign(value, ref this.dmx_channel_count);
				case "is_disabled": return MikanJson.TryAssign(value, ref this.is_disabled);
				case "max_wattage": return MikanJson.TryAssign(value, ref this.max_wattage);
				case "lumens_per_watt": return MikanJson.TryAssign(value, ref this.lumens_per_watt);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXFixtureGroupComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "DMXFixtureGroupComponent";
		public const string k_ownerSystemName = "DMXFixtureGroupSystem";

		public int stage_id;
		public List<int> fixture_ids;

		public override string RuntimeClassName => "MikanDMXFixtureGroupComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("stage_id", MikanJson.WriteInt(this.stage_id));
			json.Add("fixture_ids", MikanJson.WriteList(this.fixture_ids, e0 => MikanJson.WriteInt(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.stage_id = MikanJson.ReadInt(MikanJson.Field(json, "stage_id"));
			this.fixture_ids = MikanJson.ReadList(MikanJson.Field(json, "fixture_ids"), this.fixture_ids, e0 => MikanJson.ReadInt(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "stage_id": value = this.stage_id; return true;
				case "fixture_ids": value = this.fixture_ids; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "stage_id": return MikanJson.TryAssign(value, ref this.stage_id);
				case "fixture_ids": return MikanJson.TryAssign(value, ref this.fixture_ids);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXFixtureGroupSystemValues : MikanSystemValues
	{
		public const string k_systemName = "DMXFixtureGroupSystem";


		public override string RuntimeClassName => "MikanDMXFixtureGroupSystemValues";
	};

	public class MikanDMXObjectSystemValues : MikanSystemValues
	{
		public const string k_systemName = "DMXObjectSystem";

		public string network_interface_ip;
		public byte dmx_priority;
		public float transmit_rate_hz;

		public override string RuntimeClassName => "MikanDMXObjectSystemValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("network_interface_ip", MikanJson.WriteString(this.network_interface_ip));
			json.Add("dmx_priority", MikanJson.WriteByte(this.dmx_priority));
			json.Add("transmit_rate_hz", MikanJson.WriteFloat(this.transmit_rate_hz));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.network_interface_ip = MikanJson.ReadString(MikanJson.Field(json, "network_interface_ip"));
			this.dmx_priority = MikanJson.ReadByte(MikanJson.Field(json, "dmx_priority"));
			this.transmit_rate_hz = MikanJson.ReadFloat(MikanJson.Field(json, "transmit_rate_hz"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "network_interface_ip": value = this.network_interface_ip; return true;
				case "dmx_priority": value = this.dmx_priority; return true;
				case "transmit_rate_hz": value = this.transmit_rate_hz; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "network_interface_ip": return MikanJson.TryAssign(value, ref this.network_interface_ip);
				case "dmx_priority": return MikanJson.TryAssign(value, ref this.dmx_priority);
				case "transmit_rate_hz": return MikanJson.TryAssign(value, ref this.transmit_rate_hz);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXPresetComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "DMXPresetComponent";
		public const string k_ownerSystemName = "DMXPresetSystem";

		public int group_id;
		public List<int> fixture_ids;
		public List<int> channel_counts;
		public List<byte> channel_data;

		public override string RuntimeClassName => "MikanDMXPresetComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("group_id", MikanJson.WriteInt(this.group_id));
			json.Add("fixture_ids", MikanJson.WriteList(this.fixture_ids, e0 => MikanJson.WriteInt(e0)));
			json.Add("channel_counts", MikanJson.WriteList(this.channel_counts, e0 => MikanJson.WriteInt(e0)));
			json.Add("channel_data", MikanJson.WriteList(this.channel_data, e0 => MikanJson.WriteByte(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.group_id = MikanJson.ReadInt(MikanJson.Field(json, "group_id"));
			this.fixture_ids = MikanJson.ReadList(MikanJson.Field(json, "fixture_ids"), this.fixture_ids, e0 => MikanJson.ReadInt(e0));
			this.channel_counts = MikanJson.ReadList(MikanJson.Field(json, "channel_counts"), this.channel_counts, e0 => MikanJson.ReadInt(e0));
			this.channel_data = MikanJson.ReadList(MikanJson.Field(json, "channel_data"), this.channel_data, e0 => MikanJson.ReadByte(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "group_id": value = this.group_id; return true;
				case "fixture_ids": value = this.fixture_ids; return true;
				case "channel_counts": value = this.channel_counts; return true;
				case "channel_data": value = this.channel_data; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "group_id": return MikanJson.TryAssign(value, ref this.group_id);
				case "fixture_ids": return MikanJson.TryAssign(value, ref this.fixture_ids);
				case "channel_counts": return MikanJson.TryAssign(value, ref this.channel_counts);
				case "channel_data": return MikanJson.TryAssign(value, ref this.channel_data);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXPresetSystemValues : MikanSystemValues
	{
		public const string k_systemName = "DMXPresetSystem";


		public override string RuntimeClassName => "MikanDMXPresetSystemValues";
	};

	public class MikanDMXSequenceComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "DMXSequenceComponent";
		public const string k_ownerSystemName = "DMXSequenceSystem";

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

		public override string RuntimeClassName => "MikanDMXSequenceComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("group_id", MikanJson.WriteInt(this.group_id));
			json.Add("script_component_id", MikanJson.WriteInt(this.script_component_id));
			json.Add("duration_seconds", MikanJson.WriteFloat(this.duration_seconds));
			json.Add("loop", MikanJson.WriteBool(this.loop));
			json.Add("playback_state", MikanJson.WriteInt(this.playback_state));
			json.Add("time_since_start", MikanJson.WriteFloat(this.time_since_start));
			json.Add("content_source", MikanDMXSequenceContentSourceJson.ToJson(this.content_source));
			json.Add("content_path", MikanJson.WriteString(this.content_path));
			json.Add("scroll_text", MikanJson.WriteString(this.scroll_text));
			json.Add("font_path", MikanJson.WriteString(this.font_path));
			json.Add("text_pixel_height", MikanJson.WriteInt(this.text_pixel_height));
			json.Add("foreground_color", MikanJson.WriteObject(this.foreground_color));
			json.Add("background_color", MikanJson.WriteObject(this.background_color));
			json.Add("scroll_direction", MikanDMXScrollDirectionJson.ToJson(this.scroll_direction));
			json.Add("scroll_speed", MikanJson.WriteFloat(this.scroll_speed));
			json.Add("sprite_frame_width", MikanJson.WriteInt(this.sprite_frame_width));
			json.Add("sprite_frame_height", MikanJson.WriteInt(this.sprite_frame_height));
			json.Add("sprite_fps", MikanJson.WriteFloat(this.sprite_fps));
			json.Add("playback_speed_scale", MikanJson.WriteFloat(this.playback_speed_scale));
			json.Add("brightness", MikanJson.WriteFloat(this.brightness));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.group_id = MikanJson.ReadInt(MikanJson.Field(json, "group_id"));
			this.script_component_id = MikanJson.ReadInt(MikanJson.Field(json, "script_component_id"));
			this.duration_seconds = MikanJson.ReadFloat(MikanJson.Field(json, "duration_seconds"));
			this.loop = MikanJson.ReadBool(MikanJson.Field(json, "loop"));
			this.playback_state = MikanJson.ReadInt(MikanJson.Field(json, "playback_state"));
			this.time_since_start = MikanJson.ReadFloat(MikanJson.Field(json, "time_since_start"));
			this.content_source = MikanDMXSequenceContentSourceJson.FromJson(MikanJson.Field(json, "content_source"));
			this.content_path = MikanJson.ReadString(MikanJson.Field(json, "content_path"));
			this.scroll_text = MikanJson.ReadString(MikanJson.Field(json, "scroll_text"));
			this.font_path = MikanJson.ReadString(MikanJson.Field(json, "font_path"));
			this.text_pixel_height = MikanJson.ReadInt(MikanJson.Field(json, "text_pixel_height"));
			this.foreground_color = MikanJson.ReadObject(MikanJson.Field(json, "foreground_color"), this.foreground_color ?? new MikanVector3f());
			this.background_color = MikanJson.ReadObject(MikanJson.Field(json, "background_color"), this.background_color ?? new MikanVector3f());
			this.scroll_direction = MikanDMXScrollDirectionJson.FromJson(MikanJson.Field(json, "scroll_direction"));
			this.scroll_speed = MikanJson.ReadFloat(MikanJson.Field(json, "scroll_speed"));
			this.sprite_frame_width = MikanJson.ReadInt(MikanJson.Field(json, "sprite_frame_width"));
			this.sprite_frame_height = MikanJson.ReadInt(MikanJson.Field(json, "sprite_frame_height"));
			this.sprite_fps = MikanJson.ReadFloat(MikanJson.Field(json, "sprite_fps"));
			this.playback_speed_scale = MikanJson.ReadFloat(MikanJson.Field(json, "playback_speed_scale"));
			this.brightness = MikanJson.ReadFloat(MikanJson.Field(json, "brightness"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "group_id": value = this.group_id; return true;
				case "script_component_id": value = this.script_component_id; return true;
				case "duration_seconds": value = this.duration_seconds; return true;
				case "loop": value = this.loop; return true;
				case "playback_state": value = this.playback_state; return true;
				case "time_since_start": value = this.time_since_start; return true;
				case "content_source": value = this.content_source; return true;
				case "content_path": value = this.content_path; return true;
				case "scroll_text": value = this.scroll_text; return true;
				case "font_path": value = this.font_path; return true;
				case "text_pixel_height": value = this.text_pixel_height; return true;
				case "foreground_color": value = this.foreground_color; return true;
				case "background_color": value = this.background_color; return true;
				case "scroll_direction": value = this.scroll_direction; return true;
				case "scroll_speed": value = this.scroll_speed; return true;
				case "sprite_frame_width": value = this.sprite_frame_width; return true;
				case "sprite_frame_height": value = this.sprite_frame_height; return true;
				case "sprite_fps": value = this.sprite_fps; return true;
				case "playback_speed_scale": value = this.playback_speed_scale; return true;
				case "brightness": value = this.brightness; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "group_id": return MikanJson.TryAssign(value, ref this.group_id);
				case "script_component_id": return MikanJson.TryAssign(value, ref this.script_component_id);
				case "duration_seconds": return MikanJson.TryAssign(value, ref this.duration_seconds);
				case "loop": return MikanJson.TryAssign(value, ref this.loop);
				case "playback_state": return MikanJson.TryAssign(value, ref this.playback_state);
				case "time_since_start": return MikanJson.TryAssign(value, ref this.time_since_start);
				case "content_source": return MikanJson.TryAssign(value, ref this.content_source);
				case "content_path": return MikanJson.TryAssign(value, ref this.content_path);
				case "scroll_text": return MikanJson.TryAssign(value, ref this.scroll_text);
				case "font_path": return MikanJson.TryAssign(value, ref this.font_path);
				case "text_pixel_height": return MikanJson.TryAssign(value, ref this.text_pixel_height);
				case "foreground_color": return MikanJson.TryAssign(value, ref this.foreground_color);
				case "background_color": return MikanJson.TryAssign(value, ref this.background_color);
				case "scroll_direction": return MikanJson.TryAssign(value, ref this.scroll_direction);
				case "scroll_speed": return MikanJson.TryAssign(value, ref this.scroll_speed);
				case "sprite_frame_width": return MikanJson.TryAssign(value, ref this.sprite_frame_width);
				case "sprite_frame_height": return MikanJson.TryAssign(value, ref this.sprite_frame_height);
				case "sprite_fps": return MikanJson.TryAssign(value, ref this.sprite_fps);
				case "playback_speed_scale": return MikanJson.TryAssign(value, ref this.playback_speed_scale);
				case "brightness": return MikanJson.TryAssign(value, ref this.brightness);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXSequenceSystemValues : MikanSystemValues
	{
		public const string k_systemName = "DMXSequenceSystem";


		public override string RuntimeClassName => "MikanDMXSequenceSystemValues";
	};

	public class MikanLightEnvironmentComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "LightEnvironmentComponent";
		public const string k_ownerSystemName = "LightEnvironmentSystem";

		public List<float> sh_coefficients;
		public float exposure_scale;
		public float directionality;
		public MikanVector3f key_light_direction;

		public override string RuntimeClassName => "MikanLightEnvironmentComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("sh_coefficients", MikanJson.WriteList(this.sh_coefficients, e0 => MikanJson.WriteFloat(e0)));
			json.Add("exposure_scale", MikanJson.WriteFloat(this.exposure_scale));
			json.Add("directionality", MikanJson.WriteFloat(this.directionality));
			json.Add("key_light_direction", MikanJson.WriteObject(this.key_light_direction));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.sh_coefficients = MikanJson.ReadList(MikanJson.Field(json, "sh_coefficients"), this.sh_coefficients, e0 => MikanJson.ReadFloat(e0));
			this.exposure_scale = MikanJson.ReadFloat(MikanJson.Field(json, "exposure_scale"));
			this.directionality = MikanJson.ReadFloat(MikanJson.Field(json, "directionality"));
			this.key_light_direction = MikanJson.ReadObject(MikanJson.Field(json, "key_light_direction"), this.key_light_direction ?? new MikanVector3f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "sh_coefficients": value = this.sh_coefficients; return true;
				case "exposure_scale": value = this.exposure_scale; return true;
				case "directionality": value = this.directionality; return true;
				case "key_light_direction": value = this.key_light_direction; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "sh_coefficients": return MikanJson.TryAssign(value, ref this.sh_coefficients);
				case "exposure_scale": return MikanJson.TryAssign(value, ref this.exposure_scale);
				case "directionality": return MikanJson.TryAssign(value, ref this.directionality);
				case "key_light_direction": return MikanJson.TryAssign(value, ref this.key_light_direction);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanLightEnvironmentSystemValues : MikanSystemValues
	{
		public const string k_systemName = "LightEnvironmentSystem";


		public override string RuntimeClassName => "MikanLightEnvironmentSystemValues";
	};

	public class MikanRGBPixelGridComponentValues : MikanDMXFixtureComponentValues
	{
		public const string k_componentClassName = "RGBPixelGridComponent";
		public const string k_ownerSystemName = "RGBPixelGridSystem";

		public int grid_columns;
		public int grid_rows;
		public MikanVector3f pixel_size_mm;
		public MikanVector2f pixel_separation_mm;
		public MikanPixelGridOrigin origin_pixel;
		public bool zig_zag;

		public override string RuntimeClassName => "MikanRGBPixelGridComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("grid_columns", MikanJson.WriteInt(this.grid_columns));
			json.Add("grid_rows", MikanJson.WriteInt(this.grid_rows));
			json.Add("pixel_size_mm", MikanJson.WriteObject(this.pixel_size_mm));
			json.Add("pixel_separation_mm", MikanJson.WriteObject(this.pixel_separation_mm));
			json.Add("origin_pixel", MikanPixelGridOriginJson.ToJson(this.origin_pixel));
			json.Add("zig_zag", MikanJson.WriteBool(this.zig_zag));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.grid_columns = MikanJson.ReadInt(MikanJson.Field(json, "grid_columns"));
			this.grid_rows = MikanJson.ReadInt(MikanJson.Field(json, "grid_rows"));
			this.pixel_size_mm = MikanJson.ReadObject(MikanJson.Field(json, "pixel_size_mm"), this.pixel_size_mm ?? new MikanVector3f());
			this.pixel_separation_mm = MikanJson.ReadObject(MikanJson.Field(json, "pixel_separation_mm"), this.pixel_separation_mm ?? new MikanVector2f());
			this.origin_pixel = MikanPixelGridOriginJson.FromJson(MikanJson.Field(json, "origin_pixel"));
			this.zig_zag = MikanJson.ReadBool(MikanJson.Field(json, "zig_zag"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "grid_columns": value = this.grid_columns; return true;
				case "grid_rows": value = this.grid_rows; return true;
				case "pixel_size_mm": value = this.pixel_size_mm; return true;
				case "pixel_separation_mm": value = this.pixel_separation_mm; return true;
				case "origin_pixel": value = this.origin_pixel; return true;
				case "zig_zag": value = this.zig_zag; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "grid_columns": return MikanJson.TryAssign(value, ref this.grid_columns);
				case "grid_rows": return MikanJson.TryAssign(value, ref this.grid_rows);
				case "pixel_size_mm": return MikanJson.TryAssign(value, ref this.pixel_size_mm);
				case "pixel_separation_mm": return MikanJson.TryAssign(value, ref this.pixel_separation_mm);
				case "origin_pixel": return MikanJson.TryAssign(value, ref this.origin_pixel);
				case "zig_zag": return MikanJson.TryAssign(value, ref this.zig_zag);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRGBPixelGridSystemValues : MikanSystemValues
	{
		public const string k_systemName = "RGBPixelGridSystem";


		public override string RuntimeClassName => "MikanRGBPixelGridSystemValues";
	};

	public class MikanRGBSpotLightComponentValues : MikanDMXFixtureComponentValues
	{
		public const string k_componentClassName = "RGBSpotLightComponent";
		public const string k_ownerSystemName = "RGBSpotLightSystem";

		public float cone_angle_degrees;
		public float cone_range_meters;

		public override string RuntimeClassName => "MikanRGBSpotLightComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("cone_angle_degrees", MikanJson.WriteFloat(this.cone_angle_degrees));
			json.Add("cone_range_meters", MikanJson.WriteFloat(this.cone_range_meters));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.cone_angle_degrees = MikanJson.ReadFloat(MikanJson.Field(json, "cone_angle_degrees"));
			this.cone_range_meters = MikanJson.ReadFloat(MikanJson.Field(json, "cone_range_meters"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "cone_angle_degrees": value = this.cone_angle_degrees; return true;
				case "cone_range_meters": value = this.cone_range_meters; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "cone_angle_degrees": return MikanJson.TryAssign(value, ref this.cone_angle_degrees);
				case "cone_range_meters": return MikanJson.TryAssign(value, ref this.cone_range_meters);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRGBSpotLightSystemValues : MikanSystemValues
	{
		public const string k_systemName = "RGBSpotLightSystem";


		public override string RuntimeClassName => "MikanRGBSpotLightSystemValues";
	};

	public class MikanUniverseDMXData : MikanSerializableObject
	{
		public ushort dmx_universe_id;
		public MikanDMXBufferFormat buffer_format;
		public List<byte> buffer_data;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("dmx_universe_id", MikanJson.WriteUShort(this.dmx_universe_id));
			json.Add("buffer_format", MikanDMXBufferFormatJson.ToJson(this.buffer_format));
			json.Add("buffer_data", MikanJson.WriteList(this.buffer_data, e0 => MikanJson.WriteByte(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.dmx_universe_id = MikanJson.ReadUShort(MikanJson.Field(json, "dmx_universe_id"));
			this.buffer_format = MikanDMXBufferFormatJson.FromJson(MikanJson.Field(json, "buffer_format"));
			this.buffer_data = MikanJson.ReadList(MikanJson.Field(json, "buffer_data"), this.buffer_data, e0 => MikanJson.ReadByte(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "dmx_universe_id": value = this.dmx_universe_id; return true;
				case "buffer_format": value = this.buffer_format; return true;
				case "buffer_data": value = this.buffer_data; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "dmx_universe_id": return MikanJson.TryAssign(value, ref this.dmx_universe_id);
				case "buffer_format": return MikanJson.TryAssign(value, ref this.buffer_format);
				case "buffer_data": return MikanJson.TryAssign(value, ref this.buffer_data);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
