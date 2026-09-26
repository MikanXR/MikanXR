// This file is auto generated. DO NOT EDIT.

import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanVideoSourceIntrinsics, MikanVideoSourceType } from './MikanVideoSourceTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class GetVideoSourceIntrinsics extends MikanRequest {
  video_source_id: number = -1;

  constructor() {
    super();
    this.requestTypeName = 'GetVideoSourceIntrinsics';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' }
  ];
}

export class GetVideoSourceMode extends MikanRequest {
  video_source_id: number = -1;

  constructor() {
    super();
    this.requestTypeName = 'GetVideoSourceMode';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' }
  ];
}

export class MikanVideoSourceIntrinsicsResponse extends MikanResponse {
  intrinsics: MikanVideoSourceIntrinsics = new MikanVideoSourceIntrinsics();

  constructor() {
    super();
    this.responseTypeName = 'MikanVideoSourceIntrinsicsResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'intrinsics', type: 'MikanVideoSourceIntrinsics' }
  ];
}

export class MikanVideoSourceModeResponse extends MikanResponse {
  video_source_type: MikanVideoSourceType = MikanVideoSourceType.MONO;
  video_source_api: string = '';
  device_path: string = '';
  video_mode_name: string = '';
  resolution_x: number = 0;
  resolution_y: number = 0;
  frame_rate: number = 0;

  constructor() {
    super();
    this.responseTypeName = 'MikanVideoSourceModeResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_type', type: 'enum:MikanVideoSourceType' },
    { name: 'video_source_api', type: 'string' },
    { name: 'device_path', type: 'string' },
    { name: 'video_mode_name', type: 'string' },
    { name: 'resolution_x', type: 'int32' },
    { name: 'resolution_y', type: 'int32' },
    { name: 'frame_rate', type: 'float' }
  ];
}

export class SetUSBVideoSourceDevice extends MikanRequest {
  video_source_id: number = -1;
  device_path: string = '';

  constructor() {
    super();
    this.requestTypeName = 'SetUSBVideoSourceDevice';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' },
    { name: 'device_path', type: 'string' }
  ];
}

export class SetUSBVideoSourceFormat extends MikanRequest {
  video_source_id: number = -1;
  format: string = '';

  constructor() {
    super();
    this.requestTypeName = 'SetUSBVideoSourceFormat';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' },
    { name: 'format', type: 'string' }
  ];
}

export class SetUSBVideoSourceFrameRate extends MikanRequest {
  video_source_id: number = -1;
  frame_rate: string = '';

  constructor() {
    super();
    this.requestTypeName = 'SetUSBVideoSourceFrameRate';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' },
    { name: 'frame_rate', type: 'string' }
  ];
}

export class SetUSBVideoSourceResolution extends MikanRequest {
  video_source_id: number = -1;
  resolution: string = '';

  constructor() {
    super();
    this.requestTypeName = 'SetUSBVideoSourceResolution';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' },
    { name: 'resolution', type: 'string' }
  ];
}

