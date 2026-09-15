// This file is auto generated. DO NOT EDIT.

import { PolymorphicObject, PolymorphicStruct } from './../PolymorphicObject.js';
import { MikanComponentValues } from './MikanComponentTypes.js';
import { MikanMatrix3d, MikanMatrix4d, MikanMatrix4x3d, MikanVector3d } from './MikanMathTypes.js';
import { MikanSystemValues } from './MikanPropertyTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanIntrinsicsType {
  INVALID = 0,
  MONO_CAMERA_INTRINSICS = 1,
  STEREO_CAMERA_INTRINSICS = 2
}

export enum MikanVideoSettingType {
  INVALID = -1,
  Brightness = 0,
  Contrast = 1,
  Hue = 2,
  Saturation = 3,
  Sharpness = 4,
  Gamma = 5,
  WhiteBalance = 6,
  RedBalance = 7,
  GreenBalance = 8,
  BlueBalance = 9,
  Gain = 10,
  Pan = 11,
  Tilt = 12,
  Roll = 13,
  Zoom = 14,
  Exposure = 15,
  Iris = 16,
  Focus = 17,
  Count = 18
}

export enum MikanVideoSourceType {
  MONO = 0,
  STEREO = 1
}

export class MikanVideoSourceValues extends MikanComponentValues {
  intrinsics_ptr: PolymorphicObject = new PolymorphicObject();
  intrinsics_type: MikanIntrinsicsType = MikanIntrinsicsType.INVALID;
  is_frame_mirrored: boolean = false;
  is_buffer_mirrored: boolean = false;
  video_frame_queue_size: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'intrinsics_ptr', type: 'PolymorphicObject' },
    { name: 'intrinsics_type', type: 'enum:MikanIntrinsicsType' },
    { name: 'is_frame_mirrored', type: 'boolean' },
    { name: 'is_buffer_mirrored', type: 'boolean' },
    { name: 'video_frame_queue_size', type: 'int32' }
  ];
}

export class MikanUSBVideoSourceValues extends MikanVideoSourceValues {
  current_friendly_name: string = '';
  current_device_path: string = '';
  video_mode: string = '';
  video_resolution: string = '';
  video_fps: string = '';
  video_format: string = '';
  video_settings: number[] = [];
  video_resolutions: string[] = [];
  video_frame_rates: string[] = [];
  video_formats: string[] = [];
  brightness_valid: boolean = false;
  brightness_fraction: number = 0;
  contrast_valid: boolean = false;
  contrast_fraction: number = 0;
  hue_valid: boolean = false;
  hue_fraction: number = 0;
  saturation_valid: boolean = false;
  saturation_fraction: number = 0;
  sharpness_valid: boolean = false;
  sharpness_fraction: number = 0;
  gamma_valid: boolean = false;
  gamma_fraction: number = 0;
  white_balance_valid: boolean = false;
  white_balance_fraction: number = 0;
  red_balance_valid: boolean = false;
  red_balance_fraction: number = 0;
  green_balance_valid: boolean = false;
  green_balance_fraction: number = 0;
  blue_balance_valid: boolean = false;
  blue_balance_fraction: number = 0;
  gain_valid: boolean = false;
  gain_fraction: number = 0;
  pan_valid: boolean = false;
  pan_fraction: number = 0;
  tilt_valid: boolean = false;
  tilt_fraction: number = 0;
  roll_valid: boolean = false;
  roll_fraction: number = 0;
  zoom_valid: boolean = false;
  zoom_fraction: number = 0;
  exposure_valid: boolean = false;
  exposure_fraction: number = 0;
  iris_valid: boolean = false;
  iris_fraction: number = 0;
  focus_valid: boolean = false;
  focus_fraction: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'current_friendly_name', type: 'string' },
    { name: 'current_device_path', type: 'string' },
    { name: 'video_mode', type: 'string' },
    { name: 'video_resolution', type: 'string' },
    { name: 'video_fps', type: 'string' },
    { name: 'video_format', type: 'string' },
    { name: 'video_settings', type: 'float', isArray: true },
    { name: 'video_resolutions', type: 'string', isArray: true },
    { name: 'video_frame_rates', type: 'string', isArray: true },
    { name: 'video_formats', type: 'string', isArray: true },
    { name: 'brightness_valid', type: 'boolean' },
    { name: 'brightness_fraction', type: 'float' },
    { name: 'contrast_valid', type: 'boolean' },
    { name: 'contrast_fraction', type: 'float' },
    { name: 'hue_valid', type: 'boolean' },
    { name: 'hue_fraction', type: 'float' },
    { name: 'saturation_valid', type: 'boolean' },
    { name: 'saturation_fraction', type: 'float' },
    { name: 'sharpness_valid', type: 'boolean' },
    { name: 'sharpness_fraction', type: 'float' },
    { name: 'gamma_valid', type: 'boolean' },
    { name: 'gamma_fraction', type: 'float' },
    { name: 'white_balance_valid', type: 'boolean' },
    { name: 'white_balance_fraction', type: 'float' },
    { name: 'red_balance_valid', type: 'boolean' },
    { name: 'red_balance_fraction', type: 'float' },
    { name: 'green_balance_valid', type: 'boolean' },
    { name: 'green_balance_fraction', type: 'float' },
    { name: 'blue_balance_valid', type: 'boolean' },
    { name: 'blue_balance_fraction', type: 'float' },
    { name: 'gain_valid', type: 'boolean' },
    { name: 'gain_fraction', type: 'float' },
    { name: 'pan_valid', type: 'boolean' },
    { name: 'pan_fraction', type: 'float' },
    { name: 'tilt_valid', type: 'boolean' },
    { name: 'tilt_fraction', type: 'float' },
    { name: 'roll_valid', type: 'boolean' },
    { name: 'roll_fraction', type: 'float' },
    { name: 'zoom_valid', type: 'boolean' },
    { name: 'zoom_fraction', type: 'float' },
    { name: 'exposure_valid', type: 'boolean' },
    { name: 'exposure_fraction', type: 'float' },
    { name: 'iris_valid', type: 'boolean' },
    { name: 'iris_fraction', type: 'float' },
    { name: 'focus_valid', type: 'boolean' },
    { name: 'focus_fraction', type: 'float' }
  ];
}

export class MikanNetworkVideoSourceValues extends MikanVideoSourceValues {
  protocol: string = '';
  ip_address: string = '';
  port: number = 0;
  path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'protocol', type: 'string' },
    { name: 'ip_address', type: 'string' },
    { name: 'port', type: 'int32' },
    { name: 'path', type: 'string' }
  ];
}

export class MikanDistortionCoefficients {
  k1: number = 0;
  k2: number = 0;
  k3: number = 0;
  k4: number = 0;
  k5: number = 0;
  k6: number = 0;
  p1: number = 0;
  p2: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'k1', type: 'double' },
    { name: 'k2', type: 'double' },
    { name: 'k3', type: 'double' },
    { name: 'k4', type: 'double' },
    { name: 'k5', type: 'double' },
    { name: 'k6', type: 'double' },
    { name: 'p1', type: 'double' },
    { name: 'p2', type: 'double' }
  ];
}

export class MikanBaseIntrinsics extends PolymorphicStruct {
  pixel_width: number = 0;
  pixel_height: number = 0;
  aspect_ratio: number = 0;
  hfov: number = 0;
  vfov: number = 0;
  znear: number = 0;
  zfar: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'pixel_width', type: 'double' },
    { name: 'pixel_height', type: 'double' },
    { name: 'aspect_ratio', type: 'double' },
    { name: 'hfov', type: 'double' },
    { name: 'vfov', type: 'double' },
    { name: 'znear', type: 'double' },
    { name: 'zfar', type: 'double' }
  ];
}

export class MikanMonoIntrinsics extends MikanBaseIntrinsics {
  distortion_coefficients: MikanDistortionCoefficients = new MikanDistortionCoefficients();
  distorted_camera_matrix: MikanMatrix3d = new MikanMatrix3d();
  undistorted_camera_matrix: MikanMatrix3d = new MikanMatrix3d();

  static __serializationMetadata: SerializationField[] = [
    { name: 'distortion_coefficients', type: 'MikanDistortionCoefficients' },
    { name: 'distorted_camera_matrix', type: 'MikanMatrix3d' },
    { name: 'undistorted_camera_matrix', type: 'MikanMatrix3d' }
  ];
}

export class MikanStereoIntrinsics extends MikanBaseIntrinsics {
  left_distortion_coefficients: MikanDistortionCoefficients = new MikanDistortionCoefficients();
  left_camera_matrix: MikanMatrix3d = new MikanMatrix3d();
  right_distortion_coefficients: MikanDistortionCoefficients = new MikanDistortionCoefficients();
  right_camera_matrix: MikanMatrix3d = new MikanMatrix3d();
  left_rectification_rotation: MikanMatrix3d = new MikanMatrix3d();
  right_rectification_rotation: MikanMatrix3d = new MikanMatrix3d();
  left_rectification_projection: MikanMatrix4x3d = new MikanMatrix4x3d();
  right_rectification_projection: MikanMatrix4x3d = new MikanMatrix4x3d();
  rotation_between_cameras: MikanMatrix3d = new MikanMatrix3d();
  translation_between_cameras: MikanVector3d = new MikanVector3d();
  essential_matrix: MikanMatrix3d = new MikanMatrix3d();
  fundamental_matrix: MikanMatrix3d = new MikanMatrix3d();
  reprojection_matrix: MikanMatrix4d = new MikanMatrix4d();

  static __serializationMetadata: SerializationField[] = [
    { name: 'left_distortion_coefficients', type: 'MikanDistortionCoefficients' },
    { name: 'left_camera_matrix', type: 'MikanMatrix3d' },
    { name: 'right_distortion_coefficients', type: 'MikanDistortionCoefficients' },
    { name: 'right_camera_matrix', type: 'MikanMatrix3d' },
    { name: 'left_rectification_rotation', type: 'MikanMatrix3d' },
    { name: 'right_rectification_rotation', type: 'MikanMatrix3d' },
    { name: 'left_rectification_projection', type: 'MikanMatrix4x3d' },
    { name: 'right_rectification_projection', type: 'MikanMatrix4x3d' },
    { name: 'rotation_between_cameras', type: 'MikanMatrix3d' },
    { name: 'translation_between_cameras', type: 'MikanVector3d' },
    { name: 'essential_matrix', type: 'MikanMatrix3d' },
    { name: 'fundamental_matrix', type: 'MikanMatrix3d' },
    { name: 'reprojection_matrix', type: 'MikanMatrix4d' }
  ];
}

export class MikanVideoSourceIntrinsics {
  intrinsics_ptr: PolymorphicObject = new PolymorphicObject();
  intrinsics_type: MikanIntrinsicsType = MikanIntrinsicsType.INVALID;

  static __serializationMetadata: SerializationField[] = [
    { name: 'intrinsics_ptr', type: 'PolymorphicObject' },
    { name: 'intrinsics_type', type: 'enum:MikanIntrinsicsType' }
  ];
}

export class MikanARKitVideoSourceValues extends MikanVideoSourceValues {
  base_port: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'base_port', type: 'int32' }
  ];
}

export class MikanFileVideoSourceValues extends MikanVideoSourceValues {
  media_path: string = '';
  marker_media_path: string = '';
  pose_track_path: string = '';
  marker_pose_track_path: string = '';
  loop: boolean = false;
  playback_state: number = 0;
  playback_time: number = 0;
  duration_seconds: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'media_path', type: 'string' },
    { name: 'marker_media_path', type: 'string' },
    { name: 'pose_track_path', type: 'string' },
    { name: 'marker_pose_track_path', type: 'string' },
    { name: 'loop', type: 'boolean' },
    { name: 'playback_state', type: 'int32' },
    { name: 'playback_time', type: 'float' },
    { name: 'duration_seconds', type: 'float' }
  ];
}

export class MikanUSBVideoSourceSystemValues extends MikanSystemValues {
  usb_device_map: Record<string, string> = {};

  static __serializationMetadata: SerializationField[] = [
    { name: 'usb_device_map', type: 'Map', isMap: true, keyType: 'string', valueType: 'string' }
  ];
}

