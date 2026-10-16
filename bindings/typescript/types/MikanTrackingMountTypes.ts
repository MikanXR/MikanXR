// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanTrackingMountComponentValues extends MikanComponentValues {
  static readonly k_componentClassName = 'TrackingMountComponent';
  static readonly k_ownerSystemName = 'TrackingMountObjectSystem';
  device_path: string = '';
  socket_name: string = '';
  available_socket_names: string[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'device_path', type: 'string' },
    { name: 'socket_name', type: 'string' },
    { name: 'available_socket_names', type: 'string', isArray: true }
  ];
}

