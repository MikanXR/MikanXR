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
		public int base_port;
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
	};

	public class MikanDistortionCoefficients
	{
		public double k1;
		public double k2;
		public double k3;
		public double k4;
		public double k5;
		public double k6;
		public double p1;
		public double p2;
	};

	public class MikanFileVideoSourceValues : MikanVideoSourceValues
	{
		public string media_path;
		public string marker_media_path;
		public string pose_track_path;
		public string marker_pose_track_path;
		public bool loop;
		public int playback_state;
		public float playback_time;
		public float duration_seconds;
	};

	public class MikanMonoIntrinsics : MikanBaseIntrinsics
	{
		public MikanDistortionCoefficients distortion_coefficients;
		public MikanMatrix3d distorted_camera_matrix;
		public MikanMatrix3d undistorted_camera_matrix;
	};

	public class MikanNetworkVideoSourceValues : MikanVideoSourceValues
	{
		public string protocol;
		public string ip_address;
		public int port;
		public string path;
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
	};

	public class MikanUSBVideoSourceSystemValues : MikanSystemValues
	{
		public Dictionary<string, string> usb_device_map;
	};

	public class MikanUSBVideoSourceValues : MikanVideoSourceValues
	{
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
	};

	public class MikanVideoSourceIntrinsics
	{
		public PolymorphicObject intrinsics_ptr;
		public MikanIntrinsicsType intrinsics_type;
	};

	public class MikanVideoSourceValues : MikanComponentValues
	{
		public PolymorphicObject intrinsics_ptr;
		public MikanIntrinsicsType intrinsics_type;
		public bool is_frame_mirrored;
		public bool is_buffer_mirrored;
		public int video_frame_queue_size;
	};

}
