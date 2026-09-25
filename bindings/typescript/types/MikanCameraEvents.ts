// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './MikanAPITypes.js';
import { MikanVector2d, MikanVector2i, MikanVector3f } from './MikanMathTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanCameraNewFrameEvent extends MikanEvent {
  camera_id: number = -1;
  camera_forward: MikanVector3f = new MikanVector3f();
  camera_up: MikanVector3f = new MikanVector3f();
  camera_position: MikanVector3f = new MikanVector3f();
  pixel_size: MikanVector2i = new MikanVector2i();
  aux_pixel_size: MikanVector2i = new MikanVector2i();
  focal_length: MikanVector2d = new MikanVector2d();
  principal_point: MikanVector2d = new MikanVector2d();
  z_bounds: MikanVector2d = new MikanVector2d();
  frame: bigint = 0n;

  constructor() {
    super();
    this.eventTypeName = 'MikanCameraNewFrameEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'camera_forward', type: 'MikanVector3f' },
    { name: 'camera_up', type: 'MikanVector3f' },
    { name: 'camera_position', type: 'MikanVector3f' },
    { name: 'pixel_size', type: 'MikanVector2i' },
    { name: 'aux_pixel_size', type: 'MikanVector2i' },
    { name: 'focal_length', type: 'MikanVector2d' },
    { name: 'principal_point', type: 'MikanVector2d' },
    { name: 'z_bounds', type: 'MikanVector2d' },
    { name: 'frame', type: 'int64' }
  ];
}

export class MikanCameraNewPropertiesEvent extends MikanEvent {
  camera_id: number = -1;
  camera_forward: MikanVector3f = new MikanVector3f();
  camera_up: MikanVector3f = new MikanVector3f();
  camera_position: MikanVector3f = new MikanVector3f();
  pixel_size: MikanVector2i = new MikanVector2i();
  aux_pixel_size: MikanVector2i = new MikanVector2i();
  focal_length: MikanVector2d = new MikanVector2d();
  principal_point: MikanVector2d = new MikanVector2d();
  z_bounds: MikanVector2d = new MikanVector2d();

  constructor() {
    super();
    this.eventTypeName = 'MikanCameraNewPropertiesEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'camera_forward', type: 'MikanVector3f' },
    { name: 'camera_up', type: 'MikanVector3f' },
    { name: 'camera_position', type: 'MikanVector3f' },
    { name: 'pixel_size', type: 'MikanVector2i' },
    { name: 'aux_pixel_size', type: 'MikanVector2i' },
    { name: 'focal_length', type: 'MikanVector2d' },
    { name: 'principal_point', type: 'MikanVector2d' },
    { name: 'z_bounds', type: 'MikanVector2d' }
  ];
}

