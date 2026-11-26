// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanOutputComponentValues extends MikanComponentValues {
  enabled: boolean = true;

  static __serializationMetadata: SerializationField[] = [
    { name: 'enabled', type: 'boolean' }
  ];
}

export class MikanRTPOutputComponentValues extends MikanOutputComponentValues {
  static readonly k_componentClassName = 'RTPOutputComponent';
  static readonly k_ownerSystemName = 'RTPOutputSystem';
  rtp_output_host: string = '';
  rtp_output_port: number = 5004;
  rtp_output_height: number = 720;
  rtp_output_bitrate_kbps: number = 4000;

  static __serializationMetadata: SerializationField[] = [
    { name: 'rtp_output_host', type: 'string' },
    { name: 'rtp_output_port', type: 'int32' },
    { name: 'rtp_output_height', type: 'int32' },
    { name: 'rtp_output_bitrate_kbps', type: 'int32' }
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

