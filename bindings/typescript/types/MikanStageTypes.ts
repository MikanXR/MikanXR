// This file is auto generated. DO NOT EDIT.

import { MikanVector3f } from './MikanMathTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanStageTrackingVolume {
  StaticMarker = 0,
  SteamVR = 1
}

export class MikanStageComponentValues extends MikanTransformComponentValues {
  static readonly k_componentClassName = 'StageComponent';
  static readonly k_ownerSystemName = 'StageObjectSystem';
  tracking_volume_id: number = -1;
  stage_bounds_min: MikanVector3f = new MikanVector3f();
  stage_bounds_max: MikanVector3f = new MikanVector3f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'tracking_volume_id', type: 'int32' },
    { name: 'stage_bounds_min', type: 'MikanVector3f' },
    { name: 'stage_bounds_max', type: 'MikanVector3f' }
  ];
}

