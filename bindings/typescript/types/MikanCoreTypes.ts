// This file is auto generated. DO NOT EDIT.

import { MikanClientGraphicsApi, MikanColorBufferType, MikanDepthBufferType, MikanShadowBufferType } from './MikanCoreConstants.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanClientAPIVersion {
  version: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'version', type: 'int32' }
  ];
}

export class MikanRenderTargetDescriptor {
  color_buffer_type: MikanColorBufferType = MikanColorBufferType.NOCOLOR;
  depth_buffer_type: MikanDepthBufferType = MikanDepthBufferType.NOCOLOR;
  shadow_buffer_type: MikanShadowBufferType = MikanShadowBufferType.NOSHADOW;
  width: number = 0;
  height: number = 0;
  aux_width: number = 0;
  aux_height: number = 0;
  graphicsAPI: MikanClientGraphicsApi = MikanClientGraphicsApi.UNKNOWN;

  static __serializationMetadata: SerializationField[] = [
    { name: 'color_buffer_type', type: 'enum:MikanColorBufferType' },
    { name: 'depth_buffer_type', type: 'enum:MikanDepthBufferType' },
    { name: 'shadow_buffer_type', type: 'enum:MikanShadowBufferType' },
    { name: 'width', type: 'uint32' },
    { name: 'height', type: 'uint32' },
    { name: 'aux_width', type: 'uint32' },
    { name: 'aux_height', type: 'uint32' },
    { name: 'graphicsAPI', type: 'enum:MikanClientGraphicsApi' }
  ];
}

