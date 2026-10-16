// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanTextureSourceValues extends MikanComponentValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanCEFTextureSourceValues extends MikanTextureSourceValues {
  static readonly k_componentClassName = 'CEFTextureSourceComponent';
  static readonly k_ownerSystemName = 'CEFTextureSourceSystem';
  url: string = '';
  width: number = 1280;
  height: number = 720;

  static __serializationMetadata: SerializationField[] = [
    { name: 'url', type: 'string' },
    { name: 'width', type: 'int32' },
    { name: 'height', type: 'int32' }
  ];
}

export class MikanClientTextureSourceValues extends MikanTextureSourceValues {
  static readonly k_componentClassName = 'ClientTextureSourceComponent';
  static readonly k_ownerSystemName = 'ClientTextureSourceSystem';
  client_source: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'client_source', type: 'string' }
  ];
}

export class MikanSpoutTextureSourceValues extends MikanTextureSourceValues {
  static readonly k_componentClassName = 'SpoutTextureSourceComponent';
  static readonly k_ownerSystemName = 'SpoutTextureSourceSystem';
  spout_source: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'spout_source', type: 'string' }
  ];
}

