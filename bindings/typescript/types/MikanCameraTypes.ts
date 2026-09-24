// This file is auto generated. DO NOT EDIT.

import { MikanQuatd, MikanVector3d } from './MikanMathTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanCameraFrameSyncMode {
  Auto = 0,
  VideoFrame = 1,
  FreeRunning = 2
}

export class MikanCameraComponentValues extends MikanTransformComponentValues {
  stage_id: number = -1;
  tracking_mount_id: number = -1;
  video_source_id: number = -1;
  light_environment_id: number = -1;
  tracking_frame_delay: number = 0;
  frame_sync_mode: MikanCameraFrameSyncMode = MikanCameraFrameSyncMode.Auto;
  pose_driven_per_frame: boolean = false;
  client_color_render_scale: number = 1;
  client_aux_render_scale: number = 1;
  aperture_orientation_offset: MikanQuatd = new MikanQuatd();
  aperture_position_offset: MikanVector3d = new MikanVector3d();
  has_valid_aperture_offset: boolean = false;

  static __serializationMetadata: SerializationField[] = [
    { name: 'stage_id', type: 'int32' },
    { name: 'tracking_mount_id', type: 'int32' },
    { name: 'video_source_id', type: 'int32' },
    { name: 'light_environment_id', type: 'int32' },
    { name: 'tracking_frame_delay', type: 'int32' },
    { name: 'frame_sync_mode', type: 'enum:MikanCameraFrameSyncMode' },
    { name: 'pose_driven_per_frame', type: 'boolean' },
    { name: 'client_color_render_scale', type: 'float' },
    { name: 'client_aux_render_scale', type: 'float' },
    { name: 'aperture_orientation_offset', type: 'MikanQuatd' },
    { name: 'aperture_position_offset', type: 'MikanVector3d' },
    { name: 'has_valid_aperture_offset', type: 'boolean' }
  ];
}

