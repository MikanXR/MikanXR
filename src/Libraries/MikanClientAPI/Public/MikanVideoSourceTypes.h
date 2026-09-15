#pragma once

#include "MikanAPIExport.h"
#include "MikanMathTypes.h"
#include "MikanPropertyTypes.h"
#include "MikanVRDeviceTypes.h"
#include "SerializableMap.h"
#include "SerializableObjectPtr.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanVideoSourceTypes.rfkh.h"
#endif

// -- Constants -----
enum class ENUM(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanVideoSourceType
{
	MONO ENUMVALUE_STRING("MONO"),
	STEREO ENUMVALUE_STRING("STEREO")
};

enum class ENUM(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanIntrinsicsType
{
	INVALID_CAMERA_INTRINSICS ENUMVALUE_STRING("INVALID"),
	MONO_CAMERA_INTRINSICS ENUMVALUE_STRING("MONO_CAMERA_INTRINSICS"),
	STEREO_CAMERA_INTRINSICS ENUMVALUE_STRING("STEREO_CAMERA_INTRINSICS"),
};

enum class ENUM(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanVideoSettingType
{
	INVALID ENUMVALUE_STRING("INVALID")= -1,

	BRIGHTNESS ENUMVALUE_STRING("Brightness"),
	CONTRAST ENUMVALUE_STRING("Contrast"),
	HUE ENUMVALUE_STRING("Hue"),
	SATURATION ENUMVALUE_STRING("Saturation"),
	SHARPNESS ENUMVALUE_STRING("Sharpness"),
	GAMMA ENUMVALUE_STRING("Gamma"),
	WHITE_BALANCE ENUMVALUE_STRING("WhiteBalance"),
	RED_BALANCE ENUMVALUE_STRING("RedBalance"),
	GREEN_BALANCE ENUMVALUE_STRING("GreenBalance"),
	BLUE_BALANCE ENUMVALUE_STRING("BlueBalance"),
	GAIN ENUMVALUE_STRING("Gain"),
	PAN ENUMVALUE_STRING("Pan"),
	TILT ENUMVALUE_STRING("Tilt"),
	ROLL ENUMVALUE_STRING("Roll"),
	ZOOM ENUMVALUE_STRING("Zoom"),
	EXPOSURE ENUMVALUE_STRING("Exposure"),
	IRIS ENUMVALUE_STRING("Iris"),
	FOCUS ENUMVALUE_STRING("Focus"),

	COUNT ENUMVALUE_STRING("Count"),
};

// -- Structures -----

/// Radial and tangential lens distortion coefficients computed during lens lens calibration
/// See the [OpenCV Docs](http://docs.opencv.org/2.4/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html)
/// for details
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanDistortionCoefficients
{
	FIELD() double k1= 0.0; ///< Radial Distortion Parameter 1 (r^2 numerator constant)
	FIELD() double k2= 0.0; ///< Radial Distortion Parameter 2 (r^4 numerator constant)
	FIELD() double k3= 0.0; ///< Radial Distortion Parameter 3 (r^6 numerator constant)
	FIELD() double k4= 0.0; ///< Radial Distortion Parameter 4 (r^2 divisor constant)
	FIELD() double k5= 0.0; ///< Radial Distortion Parameter 5 (r^4 divisor constant)
	FIELD() double k6= 0.0; ///< Radial Distortion Parameter 6 (r^6 divisor constant)
	FIELD() double p1= 0.0; ///< Tangential Distortion Parameter 1
	FIELD() double p2= 0.0; ///< Tangential Distortion Parameter 2

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanDistortionCoefficients_GENERATED
#endif
};

/// Camera intrinsic common properties
/// See the [OpenCV Docs](http://docs.opencv.org/2.4/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html)
/// for details
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanBaseIntrinsics
	: public Serialization::PolymorphicStruct
{
	FIELD() double pixel_width= 0.0;  ///< Width of the camera buffer in pixels
	FIELD() double pixel_height= 0.0; ///< Height of the camera buffer in pixels
	FIELD() double aspect_ratio= 0.0; ///< The aspect ratio of each pixel (y focal length / x focal length)
	FIELD() double hfov= 0.0;         ///< The horizontal field of view camera in degrees
	FIELD() double vfov= 0.0;         ///< The vertical field of view camera in degrees
	FIELD() double znear= 0.0;        ///< The distance of the near clipping plane in meters
	FIELD() double zfar= 0.0;         ///< The distance of the far clipping plane in meters

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanBaseIntrinsics_GENERATED
#endif
};

/// Camera intrinsic properties for a monoscopic camera
/// See the [OpenCV Docs](http://docs.opencv.org/2.4/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html)
/// for details
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanMonoIntrinsics
	: public MikanBaseIntrinsics
{
	// Distortion coefficients computed for the physical camera lens
	FIELD() MikanDistortionCoefficients distortion_coefficients;
	// Intrinsic camera matrix containing focal lengths and principal point for the raw distorted image
	FIELD() MikanMatrix3d distorted_camera_matrix;
	// Intrinsic camera matrix containing focal lengths and principal point for the undistorted image
	// NOTE: The hfov and vfov are computed from this matrix
	FIELD() MikanMatrix3d undistorted_camera_matrix;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanMonoIntrinsics_GENERATED
#endif
};

/// Camera intrinsic properties for a stereoscopic camera
/// See the [OpenCV Docs](http://docs.opencv.org/2.4/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html)
/// for details
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanStereoIntrinsics
	: public MikanBaseIntrinsics
{
	FIELD() MikanDistortionCoefficients left_distortion_coefficients; ///< Left lens distortion coefficients
	FIELD() MikanMatrix3d
		left_camera_matrix; ///< Intrinsic matrix for left camera containing focal lengths and principal point

	FIELD() MikanDistortionCoefficients right_distortion_coefficients; ///< Right lens distortion coefficients
	FIELD() MikanMatrix3d
		right_camera_matrix; ///< Intrinsic matrix for rotation camera containing focal lengths and principal point
	FIELD() MikanMatrix3d left_rectification_rotation;      ///< Rotation applied to left camera to rectify the image
	FIELD() MikanMatrix3d right_rectification_rotation;     ///< Rotation applied to right camera to rectify the image
	FIELD() MikanMatrix4x3d left_rectification_projection;  ///< Projection applied to left camera to rectify the image
	FIELD() MikanMatrix4x3d right_rectification_projection; ///< Projection applied to right camera to rectify the image
	FIELD() MikanMatrix3d rotation_between_cameras;         ///< Rotation between the left and right cameras
	FIELD() MikanVector3d translation_between_cameras;      ///< Translation between the left and right camera
	FIELD() MikanMatrix3d essential_matrix;    ///< Transform relating points in unit coordinate space between cameras
	FIELD() MikanMatrix3d fundamental_matrix;  ///< Transform relating points in pixel coordinates between cameras
	FIELD() MikanMatrix4d reprojection_matrix; ///< Transform relating pixel x,y + disparity to distance from cameras

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanStereoIntrinsics_GENERATED
#endif
};

/// Bundle containing all intrinsic video source properties
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanVideoSourceIntrinsics
{
	// MikanBaseIntrinsics derived type
	FIELD() Serialization::PolymorphicObjectPtr intrinsics_ptr;

	FIELD() MikanIntrinsicsType intrinsics_type;

	MikanVideoSourceIntrinsics()
		: intrinsics_ptr()
		, intrinsics_type(MikanIntrinsicsType::INVALID_CAMERA_INTRINSICS)
	{
	}

	const MikanMonoIntrinsics& getMonoIntrinsics() const
	{
		auto* monoIntrinsicsPtr= intrinsics_ptr.getTypedPointer<MikanMonoIntrinsics>();

		return *monoIntrinsicsPtr;
	}

	MikanMonoIntrinsics& getMonoIntrinsicsMutable() { return const_cast<MikanMonoIntrinsics&>(getMonoIntrinsics()); }

	const MikanStereoIntrinsics& getStereoIntrinsics() const
	{
		auto stereoIntrinsicsPtr= intrinsics_ptr.getTypedPointer<MikanStereoIntrinsics>();

		return *stereoIntrinsicsPtr;
	}

	MikanStereoIntrinsics& getStereoIntrinsicsMutable()
	{
		return const_cast<MikanStereoIntrinsics&>(getStereoIntrinsics());
	}

#if defined(MIKANAPI_REFLECTION_ENABLED) && defined(SERIALIZATION_REFLECTION_ENABLED)
	MikanMonoIntrinsics& makeMonoIntrinsics()
	{
		auto* monoIntrinsics= intrinsics_ptr.allocatedByType<MikanMonoIntrinsics>();
		intrinsics_type= MikanIntrinsicsType::MONO_CAMERA_INTRINSICS;

		return *monoIntrinsics;
	}

	MikanStereoIntrinsics& makeStereoIntrinsics()
	{
		auto* stereoIntrinsics= intrinsics_ptr.allocatedByType<MikanStereoIntrinsics>();
		intrinsics_type= MikanIntrinsicsType::STEREO_CAMERA_INTRINSICS;

		return *stereoIntrinsics;
	}
#endif // MIKANAPI_REFLECTION_ENABLED && SERIALIZATION_REFLECTION_ENABLED

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVideoSourceIntrinsics_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanVideoSourceValues
	: public MikanComponentValues
{
	FIELD() Serialization::PolymorphicObjectPtr intrinsics_ptr;
	FIELD() MikanIntrinsicsType intrinsics_type;
	FIELD() bool is_frame_mirrored;
	FIELD() bool is_buffer_mirrored;
	FIELD() int video_frame_queue_size;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVideoSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanNetworkVideoSourceValues
	: public MikanVideoSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String protocol;   ///< e.g., "RTMP", "RTSP"
	FIELD() Serialization::String ip_address; ///< IP address of the network video source
	FIELD() int port;                         ///< Port number of the network video source
	FIELD() Serialization::String path;       ///< Path of the network video source on the server

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanNetworkVideoSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanARKitVideoSourceValues
	: public MikanVideoSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() int base_port; ///< Video RTP on base_port+0, pose on base_port+2

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanARKitVideoSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanFileVideoSourceValues
	: public MikanVideoSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String media_path;        ///< Project-relative movie or image the source plays
	FIELD() Serialization::String marker_media_path; ///< Same shot with the origin ArUco marker in frame, for alignment
	FIELD() Serialization::String pose_track_path;   ///< Sidecar camera pose track for media_path, optional
	FIELD() Serialization::String marker_pose_track_path; ///< Sidecar camera pose track for marker_media_path, optional
	FIELD() bool loop;                                    ///< Wrap to the start when the movie ends
	FIELD() int playback_state;                           ///< 0 stopped, 1 playing, 2 paused; read only
	FIELD() float playback_time;                          ///< Seconds from the start of the movie; writing seeks
	FIELD() float duration_seconds;                       ///< Movie length, 0 for a still image, read only

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanFileVideoSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanUSBVideoSourceValues
	: public MikanVideoSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String current_friendly_name; ///< Current device friendly name
	FIELD() Serialization::String current_device_path;   ///< Current USB device path
	FIELD() Serialization::String video_mode;            ///< Current video mode name
	FIELD() Serialization::String
		video_resolution; ///< Current video resolution in the format "WIDTHxHEIGHT" (e.g., "1920x1080")
	FIELD() Serialization::String video_fps;           ///< Current video frame rate (e.g., "30")
	FIELD() Serialization::String video_format;        ///< Current video format (e.g., "YUY2")
	FIELD() Serialization::List<float> video_settings; ///< [0,1] Video Settings (See MikanVideoSettingType enum)
	FIELD() Serialization::List<Serialization::String>
		video_resolutions; ///< Current available video resolution options
	FIELD() Serialization::List<Serialization::String>
		video_frame_rates;                                            ///< Current available video frame rate options
	FIELD() Serialization::List<Serialization::String> video_formats; ///< Current available video format options

	// Video Setting Properties (All between [0,1])
	FIELD() bool brightness_valid;
	FIELD() float brightness_fraction;

	FIELD() bool contrast_valid;
	FIELD() float contrast_fraction;

	FIELD() bool hue_valid;
	FIELD() float hue_fraction;

	FIELD() bool saturation_valid;
	FIELD() float saturation_fraction;

	FIELD() bool sharpness_valid;
	FIELD() float sharpness_fraction;

	FIELD() bool gamma_valid;
	FIELD() float gamma_fraction;

	FIELD() bool white_balance_valid;
	FIELD() float white_balance_fraction;

	FIELD() bool red_balance_valid;
	FIELD() float red_balance_fraction;

	FIELD() bool green_balance_valid;
	FIELD() float green_balance_fraction;

	FIELD() bool blue_balance_valid;
	FIELD() float blue_balance_fraction;

	FIELD() bool gain_valid;
	FIELD() float gain_fraction;

	FIELD() bool pan_valid;
	FIELD() float pan_fraction;

	FIELD() bool tilt_valid;
	FIELD() float tilt_fraction;

	FIELD() bool roll_valid;
	FIELD() float roll_fraction;

	FIELD() bool zoom_valid;
	FIELD() float zoom_fraction;

	FIELD() bool exposure_valid;
	FIELD() float exposure_fraction;

	FIELD() bool iris_valid;
	FIELD() float iris_fraction;

	FIELD() bool focus_valid;
	FIELD() float focus_fraction;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanUSBVideoSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceTypes")) MikanUSBVideoSourceSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

	FIELD() Serialization::Map<Serialization::String, Serialization::String>
		usb_device_map; ///< Map of device path -> friendly name

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanUSBVideoSourceSystemValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanVideoSourceTypes_GENERATED
#endif