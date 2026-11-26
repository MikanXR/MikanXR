// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanOutputComponentValues extends MikanComponentValues {
  enabled: boolean = true;

  static __serializationMetadata: SerializationField[] = [
    { name: 'enabled', type: 'boolean' }
  ];
}

export class MikanSpoutOutputComponentValues extends MikanOutputComponentValues {
  static readonly k_componentClassName = 'SpoutOutputComponent';
  static readonly k_ownerSystemName = 'SpoutOutputSystem';
  spout_output_name: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'spout_output_name', type: 'string' }
  ];
}

