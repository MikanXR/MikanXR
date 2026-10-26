// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanIntrinsicsType
	{
		INVALID= 0,
		MONO_CAMERA_INTRINSICS= 1,
		STEREO_CAMERA_INTRINSICS= 2,
	};

	internal static class MikanIntrinsicsTypeJson
	{
		public static JToken ToJson(MikanIntrinsicsType value)
		{
			switch (value)
			{
				case MikanIntrinsicsType.INVALID: return MikanJson.WriteString("INVALID");
				case MikanIntrinsicsType.MONO_CAMERA_INTRINSICS: return MikanJson.WriteString("MONO_CAMERA_INTRINSICS");
				case MikanIntrinsicsType.STEREO_CAMERA_INTRINSICS: return MikanJson.WriteString("STEREO_CAMERA_INTRINSICS");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanIntrinsicsType has no wire name for " + (long)value);
		}

		public static MikanIntrinsicsType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanIntrinsicsType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanIntrinsicsType.INVALID;
				case "MONO_CAMERA_INTRINSICS": return MikanIntrinsicsType.MONO_CAMERA_INTRINSICS;
				case "STEREO_CAMERA_INTRINSICS": return MikanIntrinsicsType.STEREO_CAMERA_INTRINSICS;
			}
			throw new Exception("MikanIntrinsicsType has no value named " + name);
		}
	};

	public enum MikanVideoSettingType
	{
		INVALID= -1,
		Brightness= 0,
		Contrast= 1,
		Hue= 2,
		Saturation= 3,
		Sharpness= 4,
		Gamma= 5,
		WhiteBalance= 6,
		RedBalance= 7,
		GreenBalance= 8,
		BlueBalance= 9,
		Gain= 10,
		Pan= 11,
		Tilt= 12,
		Roll= 13,
		Zoom= 14,
		Exposure= 15,
		Iris= 16,
		Focus= 17,
		Count= 18,
	};

	internal static class MikanVideoSettingTypeJson
	{
		public static JToken ToJson(MikanVideoSettingType value)
		{
			switch (value)
			{
				case MikanVideoSettingType.INVALID: return MikanJson.WriteString("INVALID");
				case MikanVideoSettingType.Brightness: return MikanJson.WriteString("Brightness");
				case MikanVideoSettingType.Contrast: return MikanJson.WriteString("Contrast");
				case MikanVideoSettingType.Hue: return MikanJson.WriteString("Hue");
				case MikanVideoSettingType.Saturation: return MikanJson.WriteString("Saturation");
				case MikanVideoSettingType.Sharpness: return MikanJson.WriteString("Sharpness");
				case MikanVideoSettingType.Gamma: return MikanJson.WriteString("Gamma");
				case MikanVideoSettingType.WhiteBalance: return MikanJson.WriteString("WhiteBalance");
				case MikanVideoSettingType.RedBalance: return MikanJson.WriteString("RedBalance");
				case MikanVideoSettingType.GreenBalance: return MikanJson.WriteString("GreenBalance");
				case MikanVideoSettingType.BlueBalance: return MikanJson.WriteString("BlueBalance");
				case MikanVideoSettingType.Gain: return MikanJson.WriteString("Gain");
				case MikanVideoSettingType.Pan: return MikanJson.WriteString("Pan");
				case MikanVideoSettingType.Tilt: return MikanJson.WriteString("Tilt");
				case MikanVideoSettingType.Roll: return MikanJson.WriteString("Roll");
				case MikanVideoSettingType.Zoom: return MikanJson.WriteString("Zoom");
				case MikanVideoSettingType.Exposure: return MikanJson.WriteString("Exposure");
				case MikanVideoSettingType.Iris: return MikanJson.WriteString("Iris");
				case MikanVideoSettingType.Focus: return MikanJson.WriteString("Focus");
				case MikanVideoSettingType.Count: return MikanJson.WriteString("Count");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanVideoSettingType has no wire name for " + (long)value);
		}

		public static MikanVideoSettingType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanVideoSettingType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanVideoSettingType.INVALID;
				case "Brightness": return MikanVideoSettingType.Brightness;
				case "Contrast": return MikanVideoSettingType.Contrast;
				case "Hue": return MikanVideoSettingType.Hue;
				case "Saturation": return MikanVideoSettingType.Saturation;
				case "Sharpness": return MikanVideoSettingType.Sharpness;
				case "Gamma": return MikanVideoSettingType.Gamma;
				case "WhiteBalance": return MikanVideoSettingType.WhiteBalance;
				case "RedBalance": return MikanVideoSettingType.RedBalance;
				case "GreenBalance": return MikanVideoSettingType.GreenBalance;
				case "BlueBalance": return MikanVideoSettingType.BlueBalance;
				case "Gain": return MikanVideoSettingType.Gain;
				case "Pan": return MikanVideoSettingType.Pan;
				case "Tilt": return MikanVideoSettingType.Tilt;
				case "Roll": return MikanVideoSettingType.Roll;
				case "Zoom": return MikanVideoSettingType.Zoom;
				case "Exposure": return MikanVideoSettingType.Exposure;
				case "Iris": return MikanVideoSettingType.Iris;
				case "Focus": return MikanVideoSettingType.Focus;
				case "Count": return MikanVideoSettingType.Count;
			}
			throw new Exception("MikanVideoSettingType has no value named " + name);
		}
	};

	public enum MikanVideoSourceType
	{
		MONO= 0,
		STEREO= 1,
	};

	internal static class MikanVideoSourceTypeJson
	{
		public static JToken ToJson(MikanVideoSourceType value)
		{
			switch (value)
			{
				case MikanVideoSourceType.MONO: return MikanJson.WriteString("MONO");
				case MikanVideoSourceType.STEREO: return MikanJson.WriteString("STEREO");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanVideoSourceType has no wire name for " + (long)value);
		}

		public static MikanVideoSourceType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanVideoSourceType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "MONO": return MikanVideoSourceType.MONO;
				case "STEREO": return MikanVideoSourceType.STEREO;
			}
			throw new Exception("MikanVideoSourceType has no value named " + name);
		}
	};

	public class MikanARKitVideoSourceValues : MikanVideoSourceValues
	{
		public const string k_componentClassName = "ARKitVideoSourceComponent";
		public const string k_ownerSystemName = "ARKitVideoObjectSystem";

		public int base_port;

		public override string RuntimeClassName => "MikanARKitVideoSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("base_port", MikanJson.WriteInt(this.base_port));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.base_port = MikanJson.ReadInt(MikanJson.Field(json, "base_port"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "base_port": value = this.base_port; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "base_port": return MikanJson.TryAssign(value, ref this.base_port);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanBaseIntrinsics : PolymorphicStruct
	{
		public double pixel_width;
		public double pixel_height;
		public double aspect_ratio;
		public double hfov;
		public double vfov;
		public double znear;
		public double zfar;

		public override string RuntimeClassName => "MikanBaseIntrinsics";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("pixel_width", MikanJson.WriteDouble(this.pixel_width));
			json.Add("pixel_height", MikanJson.WriteDouble(this.pixel_height));
			json.Add("aspect_ratio", MikanJson.WriteDouble(this.aspect_ratio));
			json.Add("hfov", MikanJson.WriteDouble(this.hfov));
			json.Add("vfov", MikanJson.WriteDouble(this.vfov));
			json.Add("znear", MikanJson.WriteDouble(this.znear));
			json.Add("zfar", MikanJson.WriteDouble(this.zfar));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.pixel_width = MikanJson.ReadDouble(MikanJson.Field(json, "pixel_width"));
			this.pixel_height = MikanJson.ReadDouble(MikanJson.Field(json, "pixel_height"));
			this.aspect_ratio = MikanJson.ReadDouble(MikanJson.Field(json, "aspect_ratio"));
			this.hfov = MikanJson.ReadDouble(MikanJson.Field(json, "hfov"));
			this.vfov = MikanJson.ReadDouble(MikanJson.Field(json, "vfov"));
			this.znear = MikanJson.ReadDouble(MikanJson.Field(json, "znear"));
			this.zfar = MikanJson.ReadDouble(MikanJson.Field(json, "zfar"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "pixel_width": value = this.pixel_width; return true;
				case "pixel_height": value = this.pixel_height; return true;
				case "aspect_ratio": value = this.aspect_ratio; return true;
				case "hfov": value = this.hfov; return true;
				case "vfov": value = this.vfov; return true;
				case "znear": value = this.znear; return true;
				case "zfar": value = this.zfar; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "pixel_width": return MikanJson.TryAssign(value, ref this.pixel_width);
				case "pixel_height": return MikanJson.TryAssign(value, ref this.pixel_height);
				case "aspect_ratio": return MikanJson.TryAssign(value, ref this.aspect_ratio);
				case "hfov": return MikanJson.TryAssign(value, ref this.hfov);
				case "vfov": return MikanJson.TryAssign(value, ref this.vfov);
				case "znear": return MikanJson.TryAssign(value, ref this.znear);
				case "zfar": return MikanJson.TryAssign(value, ref this.zfar);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDistortionCoefficients : MikanSerializableObject
	{
		public double k1;
		public double k2;
		public double k3;
		public double k4;
		public double k5;
		public double k6;
		public double p1;
		public double p2;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("k1", MikanJson.WriteDouble(this.k1));
			json.Add("k2", MikanJson.WriteDouble(this.k2));
			json.Add("k3", MikanJson.WriteDouble(this.k3));
			json.Add("k4", MikanJson.WriteDouble(this.k4));
			json.Add("k5", MikanJson.WriteDouble(this.k5));
			json.Add("k6", MikanJson.WriteDouble(this.k6));
			json.Add("p1", MikanJson.WriteDouble(this.p1));
			json.Add("p2", MikanJson.WriteDouble(this.p2));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.k1 = MikanJson.ReadDouble(MikanJson.Field(json, "k1"));
			this.k2 = MikanJson.ReadDouble(MikanJson.Field(json, "k2"));
			this.k3 = MikanJson.ReadDouble(MikanJson.Field(json, "k3"));
			this.k4 = MikanJson.ReadDouble(MikanJson.Field(json, "k4"));
			this.k5 = MikanJson.ReadDouble(MikanJson.Field(json, "k5"));
			this.k6 = MikanJson.ReadDouble(MikanJson.Field(json, "k6"));
			this.p1 = MikanJson.ReadDouble(MikanJson.Field(json, "p1"));
			this.p2 = MikanJson.ReadDouble(MikanJson.Field(json, "p2"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "k1": value = this.k1; return true;
				case "k2": value = this.k2; return true;
				case "k3": value = this.k3; return true;
				case "k4": value = this.k4; return true;
				case "k5": value = this.k5; return true;
				case "k6": value = this.k6; return true;
				case "p1": value = this.p1; return true;
				case "p2": value = this.p2; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "k1": return MikanJson.TryAssign(value, ref this.k1);
				case "k2": return MikanJson.TryAssign(value, ref this.k2);
				case "k3": return MikanJson.TryAssign(value, ref this.k3);
				case "k4": return MikanJson.TryAssign(value, ref this.k4);
				case "k5": return MikanJson.TryAssign(value, ref this.k5);
				case "k6": return MikanJson.TryAssign(value, ref this.k6);
				case "p1": return MikanJson.TryAssign(value, ref this.p1);
				case "p2": return MikanJson.TryAssign(value, ref this.p2);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanFileVideoSourceValues : MikanVideoSourceValues
	{
		public const string k_componentClassName = "FileVideoSourceComponent";
		public const string k_ownerSystemName = "FileVideoSourceSystem";

		public string media_path;
		public string pose_track_path;
		public bool loop;
		public int playback_state;
		public float playback_time;
		public float duration_seconds;

		public override string RuntimeClassName => "MikanFileVideoSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("media_path", MikanJson.WriteString(this.media_path));
			json.Add("pose_track_path", MikanJson.WriteString(this.pose_track_path));
			json.Add("loop", MikanJson.WriteBool(this.loop));
			json.Add("playback_state", MikanJson.WriteInt(this.playback_state));
			json.Add("playback_time", MikanJson.WriteFloat(this.playback_time));
			json.Add("duration_seconds", MikanJson.WriteFloat(this.duration_seconds));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.media_path = MikanJson.ReadString(MikanJson.Field(json, "media_path"));
			this.pose_track_path = MikanJson.ReadString(MikanJson.Field(json, "pose_track_path"));
			this.loop = MikanJson.ReadBool(MikanJson.Field(json, "loop"));
			this.playback_state = MikanJson.ReadInt(MikanJson.Field(json, "playback_state"));
			this.playback_time = MikanJson.ReadFloat(MikanJson.Field(json, "playback_time"));
			this.duration_seconds = MikanJson.ReadFloat(MikanJson.Field(json, "duration_seconds"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "media_path": value = this.media_path; return true;
				case "pose_track_path": value = this.pose_track_path; return true;
				case "loop": value = this.loop; return true;
				case "playback_state": value = this.playback_state; return true;
				case "playback_time": value = this.playback_time; return true;
				case "duration_seconds": value = this.duration_seconds; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "media_path": return MikanJson.TryAssign(value, ref this.media_path);
				case "pose_track_path": return MikanJson.TryAssign(value, ref this.pose_track_path);
				case "loop": return MikanJson.TryAssign(value, ref this.loop);
				case "playback_state": return MikanJson.TryAssign(value, ref this.playback_state);
				case "playback_time": return MikanJson.TryAssign(value, ref this.playback_time);
				case "duration_seconds": return MikanJson.TryAssign(value, ref this.duration_seconds);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanMonoIntrinsics : MikanBaseIntrinsics
	{
		public MikanDistortionCoefficients distortion_coefficients;
		public MikanMatrix3d distorted_camera_matrix;
		public MikanMatrix3d undistorted_camera_matrix;

		public override string RuntimeClassName => "MikanMonoIntrinsics";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("distortion_coefficients", MikanJson.WriteObject(this.distortion_coefficients));
			json.Add("distorted_camera_matrix", MikanJson.WriteObject(this.distorted_camera_matrix));
			json.Add("undistorted_camera_matrix", MikanJson.WriteObject(this.undistorted_camera_matrix));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.distortion_coefficients = MikanJson.ReadObject(MikanJson.Field(json, "distortion_coefficients"), this.distortion_coefficients ?? new MikanDistortionCoefficients());
			this.distorted_camera_matrix = MikanJson.ReadObject(MikanJson.Field(json, "distorted_camera_matrix"), this.distorted_camera_matrix ?? new MikanMatrix3d());
			this.undistorted_camera_matrix = MikanJson.ReadObject(MikanJson.Field(json, "undistorted_camera_matrix"), this.undistorted_camera_matrix ?? new MikanMatrix3d());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "distortion_coefficients": value = this.distortion_coefficients; return true;
				case "distorted_camera_matrix": value = this.distorted_camera_matrix; return true;
				case "undistorted_camera_matrix": value = this.undistorted_camera_matrix; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "distortion_coefficients": return MikanJson.TryAssign(value, ref this.distortion_coefficients);
				case "distorted_camera_matrix": return MikanJson.TryAssign(value, ref this.distorted_camera_matrix);
				case "undistorted_camera_matrix": return MikanJson.TryAssign(value, ref this.undistorted_camera_matrix);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanNetworkVideoSourceValues : MikanVideoSourceValues
	{
		public const string k_componentClassName = "NetworkVideoSourceComponent";
		public const string k_ownerSystemName = "NetworkVideoObjectSystem";

		public string protocol;
		public string ip_address;
		public int port;
		public string path;

		public override string RuntimeClassName => "MikanNetworkVideoSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("protocol", MikanJson.WriteString(this.protocol));
			json.Add("ip_address", MikanJson.WriteString(this.ip_address));
			json.Add("port", MikanJson.WriteInt(this.port));
			json.Add("path", MikanJson.WriteString(this.path));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.protocol = MikanJson.ReadString(MikanJson.Field(json, "protocol"));
			this.ip_address = MikanJson.ReadString(MikanJson.Field(json, "ip_address"));
			this.port = MikanJson.ReadInt(MikanJson.Field(json, "port"));
			this.path = MikanJson.ReadString(MikanJson.Field(json, "path"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "protocol": value = this.protocol; return true;
				case "ip_address": value = this.ip_address; return true;
				case "port": value = this.port; return true;
				case "path": value = this.path; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "protocol": return MikanJson.TryAssign(value, ref this.protocol);
				case "ip_address": return MikanJson.TryAssign(value, ref this.ip_address);
				case "port": return MikanJson.TryAssign(value, ref this.port);
				case "path": return MikanJson.TryAssign(value, ref this.path);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanStereoIntrinsics : MikanBaseIntrinsics
	{
		public MikanDistortionCoefficients left_distortion_coefficients;
		public MikanMatrix3d left_camera_matrix;
		public MikanDistortionCoefficients right_distortion_coefficients;
		public MikanMatrix3d right_camera_matrix;
		public MikanMatrix3d left_rectification_rotation;
		public MikanMatrix3d right_rectification_rotation;
		public MikanMatrix4x3d left_rectification_projection;
		public MikanMatrix4x3d right_rectification_projection;
		public MikanMatrix3d rotation_between_cameras;
		public MikanVector3d translation_between_cameras;
		public MikanMatrix3d essential_matrix;
		public MikanMatrix3d fundamental_matrix;
		public MikanMatrix4d reprojection_matrix;

		public override string RuntimeClassName => "MikanStereoIntrinsics";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("left_distortion_coefficients", MikanJson.WriteObject(this.left_distortion_coefficients));
			json.Add("left_camera_matrix", MikanJson.WriteObject(this.left_camera_matrix));
			json.Add("right_distortion_coefficients", MikanJson.WriteObject(this.right_distortion_coefficients));
			json.Add("right_camera_matrix", MikanJson.WriteObject(this.right_camera_matrix));
			json.Add("left_rectification_rotation", MikanJson.WriteObject(this.left_rectification_rotation));
			json.Add("right_rectification_rotation", MikanJson.WriteObject(this.right_rectification_rotation));
			json.Add("left_rectification_projection", MikanJson.WriteObject(this.left_rectification_projection));
			json.Add("right_rectification_projection", MikanJson.WriteObject(this.right_rectification_projection));
			json.Add("rotation_between_cameras", MikanJson.WriteObject(this.rotation_between_cameras));
			json.Add("translation_between_cameras", MikanJson.WriteObject(this.translation_between_cameras));
			json.Add("essential_matrix", MikanJson.WriteObject(this.essential_matrix));
			json.Add("fundamental_matrix", MikanJson.WriteObject(this.fundamental_matrix));
			json.Add("reprojection_matrix", MikanJson.WriteObject(this.reprojection_matrix));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.left_distortion_coefficients = MikanJson.ReadObject(MikanJson.Field(json, "left_distortion_coefficients"), this.left_distortion_coefficients ?? new MikanDistortionCoefficients());
			this.left_camera_matrix = MikanJson.ReadObject(MikanJson.Field(json, "left_camera_matrix"), this.left_camera_matrix ?? new MikanMatrix3d());
			this.right_distortion_coefficients = MikanJson.ReadObject(MikanJson.Field(json, "right_distortion_coefficients"), this.right_distortion_coefficients ?? new MikanDistortionCoefficients());
			this.right_camera_matrix = MikanJson.ReadObject(MikanJson.Field(json, "right_camera_matrix"), this.right_camera_matrix ?? new MikanMatrix3d());
			this.left_rectification_rotation = MikanJson.ReadObject(MikanJson.Field(json, "left_rectification_rotation"), this.left_rectification_rotation ?? new MikanMatrix3d());
			this.right_rectification_rotation = MikanJson.ReadObject(MikanJson.Field(json, "right_rectification_rotation"), this.right_rectification_rotation ?? new MikanMatrix3d());
			this.left_rectification_projection = MikanJson.ReadObject(MikanJson.Field(json, "left_rectification_projection"), this.left_rectification_projection ?? new MikanMatrix4x3d());
			this.right_rectification_projection = MikanJson.ReadObject(MikanJson.Field(json, "right_rectification_projection"), this.right_rectification_projection ?? new MikanMatrix4x3d());
			this.rotation_between_cameras = MikanJson.ReadObject(MikanJson.Field(json, "rotation_between_cameras"), this.rotation_between_cameras ?? new MikanMatrix3d());
			this.translation_between_cameras = MikanJson.ReadObject(MikanJson.Field(json, "translation_between_cameras"), this.translation_between_cameras ?? new MikanVector3d());
			this.essential_matrix = MikanJson.ReadObject(MikanJson.Field(json, "essential_matrix"), this.essential_matrix ?? new MikanMatrix3d());
			this.fundamental_matrix = MikanJson.ReadObject(MikanJson.Field(json, "fundamental_matrix"), this.fundamental_matrix ?? new MikanMatrix3d());
			this.reprojection_matrix = MikanJson.ReadObject(MikanJson.Field(json, "reprojection_matrix"), this.reprojection_matrix ?? new MikanMatrix4d());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "left_distortion_coefficients": value = this.left_distortion_coefficients; return true;
				case "left_camera_matrix": value = this.left_camera_matrix; return true;
				case "right_distortion_coefficients": value = this.right_distortion_coefficients; return true;
				case "right_camera_matrix": value = this.right_camera_matrix; return true;
				case "left_rectification_rotation": value = this.left_rectification_rotation; return true;
				case "right_rectification_rotation": value = this.right_rectification_rotation; return true;
				case "left_rectification_projection": value = this.left_rectification_projection; return true;
				case "right_rectification_projection": value = this.right_rectification_projection; return true;
				case "rotation_between_cameras": value = this.rotation_between_cameras; return true;
				case "translation_between_cameras": value = this.translation_between_cameras; return true;
				case "essential_matrix": value = this.essential_matrix; return true;
				case "fundamental_matrix": value = this.fundamental_matrix; return true;
				case "reprojection_matrix": value = this.reprojection_matrix; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "left_distortion_coefficients": return MikanJson.TryAssign(value, ref this.left_distortion_coefficients);
				case "left_camera_matrix": return MikanJson.TryAssign(value, ref this.left_camera_matrix);
				case "right_distortion_coefficients": return MikanJson.TryAssign(value, ref this.right_distortion_coefficients);
				case "right_camera_matrix": return MikanJson.TryAssign(value, ref this.right_camera_matrix);
				case "left_rectification_rotation": return MikanJson.TryAssign(value, ref this.left_rectification_rotation);
				case "right_rectification_rotation": return MikanJson.TryAssign(value, ref this.right_rectification_rotation);
				case "left_rectification_projection": return MikanJson.TryAssign(value, ref this.left_rectification_projection);
				case "right_rectification_projection": return MikanJson.TryAssign(value, ref this.right_rectification_projection);
				case "rotation_between_cameras": return MikanJson.TryAssign(value, ref this.rotation_between_cameras);
				case "translation_between_cameras": return MikanJson.TryAssign(value, ref this.translation_between_cameras);
				case "essential_matrix": return MikanJson.TryAssign(value, ref this.essential_matrix);
				case "fundamental_matrix": return MikanJson.TryAssign(value, ref this.fundamental_matrix);
				case "reprojection_matrix": return MikanJson.TryAssign(value, ref this.reprojection_matrix);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanUSBVideoSourceSystemValues : MikanSystemValues
	{
		public const string k_systemName = "USBVideoSourceSystem";

		public Dictionary<string, string> usb_device_map;

		public override string RuntimeClassName => "MikanUSBVideoSourceSystemValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("usb_device_map", MikanJson.WriteMap(this.usb_device_map, k0 => MikanJson.WriteString(k0), v0 => MikanJson.WriteString(v0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.usb_device_map = MikanJson.ReadMap(MikanJson.Field(json, "usb_device_map"), this.usb_device_map, k0 => MikanJson.ReadString(k0), v0 => MikanJson.ReadString(v0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "usb_device_map": value = this.usb_device_map; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "usb_device_map": return MikanJson.TryAssign(value, ref this.usb_device_map);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanUSBVideoSourceValues : MikanVideoSourceValues
	{
		public const string k_componentClassName = "USBVideoSourceComponent";
		public const string k_ownerSystemName = "USBVideoSourceSystem";

		public string current_friendly_name;
		public string current_device_path;
		public string video_mode;
		public string video_resolution;
		public string video_fps;
		public string video_format;
		public List<float> video_settings;
		public List<string> video_resolutions;
		public List<string> video_frame_rates;
		public List<string> video_formats;
		public bool brightness_valid;
		public float brightness_fraction;
		public bool contrast_valid;
		public float contrast_fraction;
		public bool hue_valid;
		public float hue_fraction;
		public bool saturation_valid;
		public float saturation_fraction;
		public bool sharpness_valid;
		public float sharpness_fraction;
		public bool gamma_valid;
		public float gamma_fraction;
		public bool white_balance_valid;
		public float white_balance_fraction;
		public bool red_balance_valid;
		public float red_balance_fraction;
		public bool green_balance_valid;
		public float green_balance_fraction;
		public bool blue_balance_valid;
		public float blue_balance_fraction;
		public bool gain_valid;
		public float gain_fraction;
		public bool pan_valid;
		public float pan_fraction;
		public bool tilt_valid;
		public float tilt_fraction;
		public bool roll_valid;
		public float roll_fraction;
		public bool zoom_valid;
		public float zoom_fraction;
		public bool exposure_valid;
		public float exposure_fraction;
		public bool iris_valid;
		public float iris_fraction;
		public bool focus_valid;
		public float focus_fraction;

		public override string RuntimeClassName => "MikanUSBVideoSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("current_friendly_name", MikanJson.WriteString(this.current_friendly_name));
			json.Add("current_device_path", MikanJson.WriteString(this.current_device_path));
			json.Add("video_mode", MikanJson.WriteString(this.video_mode));
			json.Add("video_resolution", MikanJson.WriteString(this.video_resolution));
			json.Add("video_fps", MikanJson.WriteString(this.video_fps));
			json.Add("video_format", MikanJson.WriteString(this.video_format));
			json.Add("video_settings", MikanJson.WriteList(this.video_settings, e0 => MikanJson.WriteFloat(e0)));
			json.Add("video_resolutions", MikanJson.WriteList(this.video_resolutions, e0 => MikanJson.WriteString(e0)));
			json.Add("video_frame_rates", MikanJson.WriteList(this.video_frame_rates, e0 => MikanJson.WriteString(e0)));
			json.Add("video_formats", MikanJson.WriteList(this.video_formats, e0 => MikanJson.WriteString(e0)));
			json.Add("brightness_valid", MikanJson.WriteBool(this.brightness_valid));
			json.Add("brightness_fraction", MikanJson.WriteFloat(this.brightness_fraction));
			json.Add("contrast_valid", MikanJson.WriteBool(this.contrast_valid));
			json.Add("contrast_fraction", MikanJson.WriteFloat(this.contrast_fraction));
			json.Add("hue_valid", MikanJson.WriteBool(this.hue_valid));
			json.Add("hue_fraction", MikanJson.WriteFloat(this.hue_fraction));
			json.Add("saturation_valid", MikanJson.WriteBool(this.saturation_valid));
			json.Add("saturation_fraction", MikanJson.WriteFloat(this.saturation_fraction));
			json.Add("sharpness_valid", MikanJson.WriteBool(this.sharpness_valid));
			json.Add("sharpness_fraction", MikanJson.WriteFloat(this.sharpness_fraction));
			json.Add("gamma_valid", MikanJson.WriteBool(this.gamma_valid));
			json.Add("gamma_fraction", MikanJson.WriteFloat(this.gamma_fraction));
			json.Add("white_balance_valid", MikanJson.WriteBool(this.white_balance_valid));
			json.Add("white_balance_fraction", MikanJson.WriteFloat(this.white_balance_fraction));
			json.Add("red_balance_valid", MikanJson.WriteBool(this.red_balance_valid));
			json.Add("red_balance_fraction", MikanJson.WriteFloat(this.red_balance_fraction));
			json.Add("green_balance_valid", MikanJson.WriteBool(this.green_balance_valid));
			json.Add("green_balance_fraction", MikanJson.WriteFloat(this.green_balance_fraction));
			json.Add("blue_balance_valid", MikanJson.WriteBool(this.blue_balance_valid));
			json.Add("blue_balance_fraction", MikanJson.WriteFloat(this.blue_balance_fraction));
			json.Add("gain_valid", MikanJson.WriteBool(this.gain_valid));
			json.Add("gain_fraction", MikanJson.WriteFloat(this.gain_fraction));
			json.Add("pan_valid", MikanJson.WriteBool(this.pan_valid));
			json.Add("pan_fraction", MikanJson.WriteFloat(this.pan_fraction));
			json.Add("tilt_valid", MikanJson.WriteBool(this.tilt_valid));
			json.Add("tilt_fraction", MikanJson.WriteFloat(this.tilt_fraction));
			json.Add("roll_valid", MikanJson.WriteBool(this.roll_valid));
			json.Add("roll_fraction", MikanJson.WriteFloat(this.roll_fraction));
			json.Add("zoom_valid", MikanJson.WriteBool(this.zoom_valid));
			json.Add("zoom_fraction", MikanJson.WriteFloat(this.zoom_fraction));
			json.Add("exposure_valid", MikanJson.WriteBool(this.exposure_valid));
			json.Add("exposure_fraction", MikanJson.WriteFloat(this.exposure_fraction));
			json.Add("iris_valid", MikanJson.WriteBool(this.iris_valid));
			json.Add("iris_fraction", MikanJson.WriteFloat(this.iris_fraction));
			json.Add("focus_valid", MikanJson.WriteBool(this.focus_valid));
			json.Add("focus_fraction", MikanJson.WriteFloat(this.focus_fraction));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.current_friendly_name = MikanJson.ReadString(MikanJson.Field(json, "current_friendly_name"));
			this.current_device_path = MikanJson.ReadString(MikanJson.Field(json, "current_device_path"));
			this.video_mode = MikanJson.ReadString(MikanJson.Field(json, "video_mode"));
			this.video_resolution = MikanJson.ReadString(MikanJson.Field(json, "video_resolution"));
			this.video_fps = MikanJson.ReadString(MikanJson.Field(json, "video_fps"));
			this.video_format = MikanJson.ReadString(MikanJson.Field(json, "video_format"));
			this.video_settings = MikanJson.ReadList(MikanJson.Field(json, "video_settings"), this.video_settings, e0 => MikanJson.ReadFloat(e0));
			this.video_resolutions = MikanJson.ReadList(MikanJson.Field(json, "video_resolutions"), this.video_resolutions, e0 => MikanJson.ReadString(e0));
			this.video_frame_rates = MikanJson.ReadList(MikanJson.Field(json, "video_frame_rates"), this.video_frame_rates, e0 => MikanJson.ReadString(e0));
			this.video_formats = MikanJson.ReadList(MikanJson.Field(json, "video_formats"), this.video_formats, e0 => MikanJson.ReadString(e0));
			this.brightness_valid = MikanJson.ReadBool(MikanJson.Field(json, "brightness_valid"));
			this.brightness_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "brightness_fraction"));
			this.contrast_valid = MikanJson.ReadBool(MikanJson.Field(json, "contrast_valid"));
			this.contrast_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "contrast_fraction"));
			this.hue_valid = MikanJson.ReadBool(MikanJson.Field(json, "hue_valid"));
			this.hue_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "hue_fraction"));
			this.saturation_valid = MikanJson.ReadBool(MikanJson.Field(json, "saturation_valid"));
			this.saturation_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "saturation_fraction"));
			this.sharpness_valid = MikanJson.ReadBool(MikanJson.Field(json, "sharpness_valid"));
			this.sharpness_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "sharpness_fraction"));
			this.gamma_valid = MikanJson.ReadBool(MikanJson.Field(json, "gamma_valid"));
			this.gamma_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "gamma_fraction"));
			this.white_balance_valid = MikanJson.ReadBool(MikanJson.Field(json, "white_balance_valid"));
			this.white_balance_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "white_balance_fraction"));
			this.red_balance_valid = MikanJson.ReadBool(MikanJson.Field(json, "red_balance_valid"));
			this.red_balance_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "red_balance_fraction"));
			this.green_balance_valid = MikanJson.ReadBool(MikanJson.Field(json, "green_balance_valid"));
			this.green_balance_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "green_balance_fraction"));
			this.blue_balance_valid = MikanJson.ReadBool(MikanJson.Field(json, "blue_balance_valid"));
			this.blue_balance_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "blue_balance_fraction"));
			this.gain_valid = MikanJson.ReadBool(MikanJson.Field(json, "gain_valid"));
			this.gain_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "gain_fraction"));
			this.pan_valid = MikanJson.ReadBool(MikanJson.Field(json, "pan_valid"));
			this.pan_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "pan_fraction"));
			this.tilt_valid = MikanJson.ReadBool(MikanJson.Field(json, "tilt_valid"));
			this.tilt_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "tilt_fraction"));
			this.roll_valid = MikanJson.ReadBool(MikanJson.Field(json, "roll_valid"));
			this.roll_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "roll_fraction"));
			this.zoom_valid = MikanJson.ReadBool(MikanJson.Field(json, "zoom_valid"));
			this.zoom_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "zoom_fraction"));
			this.exposure_valid = MikanJson.ReadBool(MikanJson.Field(json, "exposure_valid"));
			this.exposure_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "exposure_fraction"));
			this.iris_valid = MikanJson.ReadBool(MikanJson.Field(json, "iris_valid"));
			this.iris_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "iris_fraction"));
			this.focus_valid = MikanJson.ReadBool(MikanJson.Field(json, "focus_valid"));
			this.focus_fraction = MikanJson.ReadFloat(MikanJson.Field(json, "focus_fraction"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "current_friendly_name": value = this.current_friendly_name; return true;
				case "current_device_path": value = this.current_device_path; return true;
				case "video_mode": value = this.video_mode; return true;
				case "video_resolution": value = this.video_resolution; return true;
				case "video_fps": value = this.video_fps; return true;
				case "video_format": value = this.video_format; return true;
				case "video_settings": value = this.video_settings; return true;
				case "video_resolutions": value = this.video_resolutions; return true;
				case "video_frame_rates": value = this.video_frame_rates; return true;
				case "video_formats": value = this.video_formats; return true;
				case "brightness_valid": value = this.brightness_valid; return true;
				case "brightness_fraction": value = this.brightness_fraction; return true;
				case "contrast_valid": value = this.contrast_valid; return true;
				case "contrast_fraction": value = this.contrast_fraction; return true;
				case "hue_valid": value = this.hue_valid; return true;
				case "hue_fraction": value = this.hue_fraction; return true;
				case "saturation_valid": value = this.saturation_valid; return true;
				case "saturation_fraction": value = this.saturation_fraction; return true;
				case "sharpness_valid": value = this.sharpness_valid; return true;
				case "sharpness_fraction": value = this.sharpness_fraction; return true;
				case "gamma_valid": value = this.gamma_valid; return true;
				case "gamma_fraction": value = this.gamma_fraction; return true;
				case "white_balance_valid": value = this.white_balance_valid; return true;
				case "white_balance_fraction": value = this.white_balance_fraction; return true;
				case "red_balance_valid": value = this.red_balance_valid; return true;
				case "red_balance_fraction": value = this.red_balance_fraction; return true;
				case "green_balance_valid": value = this.green_balance_valid; return true;
				case "green_balance_fraction": value = this.green_balance_fraction; return true;
				case "blue_balance_valid": value = this.blue_balance_valid; return true;
				case "blue_balance_fraction": value = this.blue_balance_fraction; return true;
				case "gain_valid": value = this.gain_valid; return true;
				case "gain_fraction": value = this.gain_fraction; return true;
				case "pan_valid": value = this.pan_valid; return true;
				case "pan_fraction": value = this.pan_fraction; return true;
				case "tilt_valid": value = this.tilt_valid; return true;
				case "tilt_fraction": value = this.tilt_fraction; return true;
				case "roll_valid": value = this.roll_valid; return true;
				case "roll_fraction": value = this.roll_fraction; return true;
				case "zoom_valid": value = this.zoom_valid; return true;
				case "zoom_fraction": value = this.zoom_fraction; return true;
				case "exposure_valid": value = this.exposure_valid; return true;
				case "exposure_fraction": value = this.exposure_fraction; return true;
				case "iris_valid": value = this.iris_valid; return true;
				case "iris_fraction": value = this.iris_fraction; return true;
				case "focus_valid": value = this.focus_valid; return true;
				case "focus_fraction": value = this.focus_fraction; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "current_friendly_name": return MikanJson.TryAssign(value, ref this.current_friendly_name);
				case "current_device_path": return MikanJson.TryAssign(value, ref this.current_device_path);
				case "video_mode": return MikanJson.TryAssign(value, ref this.video_mode);
				case "video_resolution": return MikanJson.TryAssign(value, ref this.video_resolution);
				case "video_fps": return MikanJson.TryAssign(value, ref this.video_fps);
				case "video_format": return MikanJson.TryAssign(value, ref this.video_format);
				case "video_settings": return MikanJson.TryAssign(value, ref this.video_settings);
				case "video_resolutions": return MikanJson.TryAssign(value, ref this.video_resolutions);
				case "video_frame_rates": return MikanJson.TryAssign(value, ref this.video_frame_rates);
				case "video_formats": return MikanJson.TryAssign(value, ref this.video_formats);
				case "brightness_valid": return MikanJson.TryAssign(value, ref this.brightness_valid);
				case "brightness_fraction": return MikanJson.TryAssign(value, ref this.brightness_fraction);
				case "contrast_valid": return MikanJson.TryAssign(value, ref this.contrast_valid);
				case "contrast_fraction": return MikanJson.TryAssign(value, ref this.contrast_fraction);
				case "hue_valid": return MikanJson.TryAssign(value, ref this.hue_valid);
				case "hue_fraction": return MikanJson.TryAssign(value, ref this.hue_fraction);
				case "saturation_valid": return MikanJson.TryAssign(value, ref this.saturation_valid);
				case "saturation_fraction": return MikanJson.TryAssign(value, ref this.saturation_fraction);
				case "sharpness_valid": return MikanJson.TryAssign(value, ref this.sharpness_valid);
				case "sharpness_fraction": return MikanJson.TryAssign(value, ref this.sharpness_fraction);
				case "gamma_valid": return MikanJson.TryAssign(value, ref this.gamma_valid);
				case "gamma_fraction": return MikanJson.TryAssign(value, ref this.gamma_fraction);
				case "white_balance_valid": return MikanJson.TryAssign(value, ref this.white_balance_valid);
				case "white_balance_fraction": return MikanJson.TryAssign(value, ref this.white_balance_fraction);
				case "red_balance_valid": return MikanJson.TryAssign(value, ref this.red_balance_valid);
				case "red_balance_fraction": return MikanJson.TryAssign(value, ref this.red_balance_fraction);
				case "green_balance_valid": return MikanJson.TryAssign(value, ref this.green_balance_valid);
				case "green_balance_fraction": return MikanJson.TryAssign(value, ref this.green_balance_fraction);
				case "blue_balance_valid": return MikanJson.TryAssign(value, ref this.blue_balance_valid);
				case "blue_balance_fraction": return MikanJson.TryAssign(value, ref this.blue_balance_fraction);
				case "gain_valid": return MikanJson.TryAssign(value, ref this.gain_valid);
				case "gain_fraction": return MikanJson.TryAssign(value, ref this.gain_fraction);
				case "pan_valid": return MikanJson.TryAssign(value, ref this.pan_valid);
				case "pan_fraction": return MikanJson.TryAssign(value, ref this.pan_fraction);
				case "tilt_valid": return MikanJson.TryAssign(value, ref this.tilt_valid);
				case "tilt_fraction": return MikanJson.TryAssign(value, ref this.tilt_fraction);
				case "roll_valid": return MikanJson.TryAssign(value, ref this.roll_valid);
				case "roll_fraction": return MikanJson.TryAssign(value, ref this.roll_fraction);
				case "zoom_valid": return MikanJson.TryAssign(value, ref this.zoom_valid);
				case "zoom_fraction": return MikanJson.TryAssign(value, ref this.zoom_fraction);
				case "exposure_valid": return MikanJson.TryAssign(value, ref this.exposure_valid);
				case "exposure_fraction": return MikanJson.TryAssign(value, ref this.exposure_fraction);
				case "iris_valid": return MikanJson.TryAssign(value, ref this.iris_valid);
				case "iris_fraction": return MikanJson.TryAssign(value, ref this.iris_fraction);
				case "focus_valid": return MikanJson.TryAssign(value, ref this.focus_valid);
				case "focus_fraction": return MikanJson.TryAssign(value, ref this.focus_fraction);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVideoSourceIntrinsics : MikanSerializableObject
	{
		public PolymorphicObject intrinsics_ptr;
		public MikanIntrinsicsType intrinsics_type;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("intrinsics_ptr", MikanJson.WritePolymorphic(this.intrinsics_ptr));
			json.Add("intrinsics_type", MikanIntrinsicsTypeJson.ToJson(this.intrinsics_type));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.intrinsics_ptr = MikanJson.ReadPolymorphic(MikanJson.Field(json, "intrinsics_ptr"), this.intrinsics_ptr);
			this.intrinsics_type = MikanIntrinsicsTypeJson.FromJson(MikanJson.Field(json, "intrinsics_type"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "intrinsics_ptr": value = this.intrinsics_ptr; return true;
				case "intrinsics_type": value = this.intrinsics_type; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "intrinsics_ptr": return MikanJson.TryAssign(value, ref this.intrinsics_ptr);
				case "intrinsics_type": return MikanJson.TryAssign(value, ref this.intrinsics_type);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVideoSourceValues : MikanComponentValues
	{
		public PolymorphicObject intrinsics_ptr;
		public MikanIntrinsicsType intrinsics_type;
		public bool is_frame_mirrored;
		public bool is_buffer_mirrored;
		public int video_frame_queue_size;

		public override string RuntimeClassName => "MikanVideoSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("intrinsics_ptr", MikanJson.WritePolymorphic(this.intrinsics_ptr));
			json.Add("intrinsics_type", MikanIntrinsicsTypeJson.ToJson(this.intrinsics_type));
			json.Add("is_frame_mirrored", MikanJson.WriteBool(this.is_frame_mirrored));
			json.Add("is_buffer_mirrored", MikanJson.WriteBool(this.is_buffer_mirrored));
			json.Add("video_frame_queue_size", MikanJson.WriteInt(this.video_frame_queue_size));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.intrinsics_ptr = MikanJson.ReadPolymorphic(MikanJson.Field(json, "intrinsics_ptr"), this.intrinsics_ptr);
			this.intrinsics_type = MikanIntrinsicsTypeJson.FromJson(MikanJson.Field(json, "intrinsics_type"));
			this.is_frame_mirrored = MikanJson.ReadBool(MikanJson.Field(json, "is_frame_mirrored"));
			this.is_buffer_mirrored = MikanJson.ReadBool(MikanJson.Field(json, "is_buffer_mirrored"));
			this.video_frame_queue_size = MikanJson.ReadInt(MikanJson.Field(json, "video_frame_queue_size"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "intrinsics_ptr": value = this.intrinsics_ptr; return true;
				case "intrinsics_type": value = this.intrinsics_type; return true;
				case "is_frame_mirrored": value = this.is_frame_mirrored; return true;
				case "is_buffer_mirrored": value = this.is_buffer_mirrored; return true;
				case "video_frame_queue_size": value = this.video_frame_queue_size; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "intrinsics_ptr": return MikanJson.TryAssign(value, ref this.intrinsics_ptr);
				case "intrinsics_type": return MikanJson.TryAssign(value, ref this.intrinsics_type);
				case "is_frame_mirrored": return MikanJson.TryAssign(value, ref this.is_frame_mirrored);
				case "is_buffer_mirrored": return MikanJson.TryAssign(value, ref this.is_buffer_mirrored);
				case "video_frame_queue_size": return MikanJson.TryAssign(value, ref this.video_frame_queue_size);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
