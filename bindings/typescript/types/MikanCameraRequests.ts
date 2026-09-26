// This file is auto generated. DO NOT EDIT.

import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanRenderTargetDescriptor } from './MikanCoreTypes.js';
import { MikanVector2d, MikanVector2i, MikanVector3f } from './MikanMathTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class AllocateCameraRenderTargetTextures extends MikanRequest {
  camera_id: number = -1;
  descriptor: MikanRenderTargetDescriptor = new MikanRenderTargetDescriptor();

  constructor() {
    super();
    this.requestTypeName = 'AllocateCameraRenderTargetTextures';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'descriptor', type: 'MikanRenderTargetDescriptor' }
  ];
}

export class FreeCameraRenderTargetTextures extends MikanRequest {
  camera_id: number = -1;

  constructor() {
    super();
    this.requestTypeName = 'FreeCameraRenderTargetTextures';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' }
  ];
}

export class GetCameraProperties extends MikanRequest {
  camera_id: number = -1;

  constructor() {
    super();
    this.requestTypeName = 'GetCameraProperties';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' }
  ];
}

export class MikanCameraPropertiesResponse extends MikanResponse {
  camera_id: number = -1;
  compositor_running: boolean = false;
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
    this.responseTypeName = 'MikanCameraPropertiesResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'compositor_running', type: 'boolean' },
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

export class PublishCameraRenderTargetTextures extends MikanRequest {
  camera_id: number = -1;
  frame_index: bigint = 0n;

  constructor() {
    super();
    this.requestTypeName = 'PublishCameraRenderTargetTextures';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'frame_index', type: 'int64' }
  ];
}

export class WriteCameraColorRenderTargetTexture extends MikanRequest {
  camera_id: number = -1;
  api_color_texture_ptr: any = null;

  constructor() {
    super();
    this.requestTypeName = 'WriteCameraColorRenderTargetTexture';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'api_color_texture_ptr', type: 'any' }
  ];
}

export class WriteCameraDepthRenderTargetTexture extends MikanRequest {
  camera_id: number = -1;
  api_depth_texture_ptr: any = null;
  z_near: number = 0;
  z_far: number = 0;

  constructor() {
    super();
    this.requestTypeName = 'WriteCameraDepthRenderTargetTexture';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'api_depth_texture_ptr', type: 'any' },
    { name: 'z_near', type: 'float' },
    { name: 'z_far', type: 'float' }
  ];
}

export class WriteCameraShadowRenderTargetTexture extends MikanRequest {
  camera_id: number = -1;
  api_shadow_texture_ptr: any = null;

  constructor() {
    super();
    this.requestTypeName = 'WriteCameraShadowRenderTargetTexture';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'camera_id', type: 'int32' },
    { name: 'api_shadow_texture_ptr', type: 'any' }
  ];
}

