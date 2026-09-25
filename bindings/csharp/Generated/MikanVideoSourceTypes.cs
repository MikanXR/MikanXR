// This file is auto generated. DO NO EDIT.
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

	public enum MikanVideoSourceType
	{
		MONO= 0,
		STEREO= 1,
	};

	public class MikanARKitVideoSourceValues : MikanVideoSourceValues
	{
		[MikanFieldOrder(0)] public int base_port;
	};

	public class MikanBaseIntrinsics : PolymorphicStruct
	{
		[MikanFieldOrder(0)] public double pixel_width;
		[MikanFieldOrder(1)] public double pixel_height;
		[MikanFieldOrder(2)] public double aspect_ratio;
		[MikanFieldOrder(3)] public double hfov;
		[MikanFieldOrder(4)] public double vfov;
		[MikanFieldOrder(5)] public double znear;
		[MikanFieldOrder(6)] public double zfar;
	};

	public class MikanDistortionCoefficients
	{
		[MikanFieldOrder(0)] public double k1;
		[MikanFieldOrder(1)] public double k2;
		[MikanFieldOrder(2)] public double k3;
		[MikanFieldOrder(3)] public double k4;
		[MikanFieldOrder(4)] public double k5;
		[MikanFieldOrder(5)] public double k6;
		[MikanFieldOrder(6)] public double p1;
		[MikanFieldOrder(7)] public double p2;
	};

	public class MikanFileVideoSourceValues : MikanVideoSourceValues
	{
		[MikanFieldOrder(0)] public string media_path;
		[MikanFieldOrder(1)] public string marker_media_path;
		[MikanFieldOrder(2)] public string pose_track_path;
		[MikanFieldOrder(3)] public string marker_pose_track_path;
		[MikanFieldOrder(4)] public bool loop;
		[MikanFieldOrder(5)] public int playback_state;
		[MikanFieldOrder(6)] public float playback_time;
		[MikanFieldOrder(7)] public float duration_seconds;
	};

	public class MikanMonoIntrinsics : MikanBaseIntrinsics
	{
		[MikanFieldOrder(0)] public MikanDistortionCoefficients distortion_coefficients;
		[MikanFieldOrder(1)] public MikanMatrix3d distorted_camera_matrix;
		[MikanFieldOrder(2)] public MikanMatrix3d undistorted_camera_matrix;
	};

	public class MikanNetworkVideoSourceValues : MikanVideoSourceValues
	{
		[MikanFieldOrder(0)] public string protocol;
		[MikanFieldOrder(1)] public string ip_address;
		[MikanFieldOrder(2)] public int port;
		[MikanFieldOrder(3)] public string path;
	};

	public class MikanStereoIntrinsics : MikanBaseIntrinsics
	{
		[MikanFieldOrder(0)] public MikanDistortionCoefficients left_distortion_coefficients;
		[MikanFieldOrder(1)] public MikanMatrix3d left_camera_matrix;
		[MikanFieldOrder(2)] public MikanDistortionCoefficients right_distortion_coefficients;
		[MikanFieldOrder(3)] public MikanMatrix3d right_camera_matrix;
		[MikanFieldOrder(4)] public MikanMatrix3d left_rectification_rotation;
		[MikanFieldOrder(5)] public MikanMatrix3d right_rectification_rotation;
		[MikanFieldOrder(6)] public MikanMatrix4x3d left_rectification_projection;
		[MikanFieldOrder(7)] public MikanMatrix4x3d right_rectification_projection;
		[MikanFieldOrder(8)] public MikanMatrix3d rotation_between_cameras;
		[MikanFieldOrder(9)] public MikanVector3d translation_between_cameras;
		[MikanFieldOrder(10)] public MikanMatrix3d essential_matrix;
		[MikanFieldOrder(11)] public MikanMatrix3d fundamental_matrix;
		[MikanFieldOrder(12)] public MikanMatrix4d reprojection_matrix;
	};

	public class MikanUSBVideoSourceSystemValues : MikanSystemValues
	{
		[MikanFieldOrder(0)] public Dictionary<string, string> usb_device_map;
	};

	public class MikanUSBVideoSourceValues : MikanVideoSourceValues
	{
		[MikanFieldOrder(0)] public string current_friendly_name;
		[MikanFieldOrder(1)] public string current_device_path;
		[MikanFieldOrder(2)] public string video_mode;
		[MikanFieldOrder(3)] public string video_resolution;
		[MikanFieldOrder(4)] public string video_fps;
		[MikanFieldOrder(5)] public string video_format;
		[MikanFieldOrder(6)] public List<float> video_settings;
		[MikanFieldOrder(7)] public List<string> video_resolutions;
		[MikanFieldOrder(8)] public List<string> video_frame_rates;
		[MikanFieldOrder(9)] public List<string> video_formats;
		[MikanFieldOrder(10)] public bool brightness_valid;
		[MikanFieldOrder(11)] public float brightness_fraction;
		[MikanFieldOrder(12)] public bool contrast_valid;
		[MikanFieldOrder(13)] public float contrast_fraction;
		[MikanFieldOrder(14)] public bool hue_valid;
		[MikanFieldOrder(15)] public float hue_fraction;
		[MikanFieldOrder(16)] public bool saturation_valid;
		[MikanFieldOrder(17)] public float saturation_fraction;
		[MikanFieldOrder(18)] public bool sharpness_valid;
		[MikanFieldOrder(19)] public float sharpness_fraction;
		[MikanFieldOrder(20)] public bool gamma_valid;
		[MikanFieldOrder(21)] public float gamma_fraction;
		[MikanFieldOrder(22)] public bool white_balance_valid;
		[MikanFieldOrder(23)] public float white_balance_fraction;
		[MikanFieldOrder(24)] public bool red_balance_valid;
		[MikanFieldOrder(25)] public float red_balance_fraction;
		[MikanFieldOrder(26)] public bool green_balance_valid;
		[MikanFieldOrder(27)] public float green_balance_fraction;
		[MikanFieldOrder(28)] public bool blue_balance_valid;
		[MikanFieldOrder(29)] public float blue_balance_fraction;
		[MikanFieldOrder(30)] public bool gain_valid;
		[MikanFieldOrder(31)] public float gain_fraction;
		[MikanFieldOrder(32)] public bool pan_valid;
		[MikanFieldOrder(33)] public float pan_fraction;
		[MikanFieldOrder(34)] public bool tilt_valid;
		[MikanFieldOrder(35)] public float tilt_fraction;
		[MikanFieldOrder(36)] public bool roll_valid;
		[MikanFieldOrder(37)] public float roll_fraction;
		[MikanFieldOrder(38)] public bool zoom_valid;
		[MikanFieldOrder(39)] public float zoom_fraction;
		[MikanFieldOrder(40)] public bool exposure_valid;
		[MikanFieldOrder(41)] public float exposure_fraction;
		[MikanFieldOrder(42)] public bool iris_valid;
		[MikanFieldOrder(43)] public float iris_fraction;
		[MikanFieldOrder(44)] public bool focus_valid;
		[MikanFieldOrder(45)] public float focus_fraction;
	};

	public class MikanVideoSourceIntrinsics
	{
		[MikanFieldOrder(0)] public PolymorphicObject intrinsics_ptr;
		[MikanFieldOrder(1)] public MikanIntrinsicsType intrinsics_type;
	};

	public class MikanVideoSourceValues : MikanComponentValues
	{
		[MikanFieldOrder(0)] public PolymorphicObject intrinsics_ptr;
		[MikanFieldOrder(1)] public MikanIntrinsicsType intrinsics_type;
		[MikanFieldOrder(2)] public bool is_frame_mirrored;
		[MikanFieldOrder(3)] public bool is_buffer_mirrored;
		[MikanFieldOrder(4)] public int video_frame_queue_size;
	};

}
