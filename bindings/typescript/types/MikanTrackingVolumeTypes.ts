// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import { MikanMatrix4f, MikanVector3f } from './MikanMathTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanTrackingRuntime {
  INVALID = -1,
  SteamVR = 0
}

export enum MikanTrackingSpace {
  INVALID = -1,
  StageSpace = 0,
  VRSpace = 1
}

export enum MikanTrackingVolumeType {
  INVALID = -1,
  marker = 0,
  vr = 1
}

export class MikanTrackingVolumeComponentValues extends MikanComponentValues {
  origin_marker_id: number = -1;

  static __serializationMetadata: SerializationField[] = [
    { name: 'origin_marker_id', type: 'int32' }
  ];
}

export class MikanMarkerTrackingVolumeComponentValues extends MikanTrackingVolumeComponentValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanVRTrackingVolumeComponentValues extends MikanTrackingVolumeComponentValues {
  tracking_runtime: MikanTrackingRuntime = MikanTrackingRuntime.INVALID;
  charuco_mount_id: number = -1;
  charuco_mount_offset_mm: MikanVector3f = new MikanVector3f();
  utility_marker_id: number = -1;
  tracking_mount_ids: number[] = [];
  vr_space_to_stage_space: MikanMatrix4f = new MikanMatrix4f();
  display_tracking_space: MikanTrackingSpace = MikanTrackingSpace.INVALID;

  static __serializationMetadata: SerializationField[] = [
    { name: 'tracking_runtime', type: 'enum:MikanTrackingRuntime' },
    { name: 'charuco_mount_id', type: 'int32' },
    { name: 'charuco_mount_offset_mm', type: 'MikanVector3f' },
    { name: 'utility_marker_id', type: 'int32' },
    { name: 'tracking_mount_ids', type: 'int32', isArray: true },
    { name: 'vr_space_to_stage_space', type: 'MikanMatrix4f' },
    { name: 'display_tracking_space', type: 'enum:MikanTrackingSpace' }
  ];
}

