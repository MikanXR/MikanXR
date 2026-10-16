// This file is auto generated. DO NOT EDIT.

import { MikanSystemValues } from './MikanPropertyTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanVRDeviceApi {
  INVALID = 0,
  STEAM_VR = 1
}

export enum MikanVRDeviceType {
  INVALID = 0,
  HMD = 1,
  CONTROLLER = 2,
  TRACKER = 3
}

export class MikanVRDeviceComponentValues extends MikanTransformComponentValues {
  static readonly k_componentClassName = 'VRDeviceComponent';
  static readonly k_ownerSystemName = 'VRObjectSystem';
  vr_device_api: MikanVRDeviceApi = MikanVRDeviceApi.INVALID;
  vr_device_type: MikanVRDeviceType = MikanVRDeviceType.INVALID;
  vr_device_index: number = -1;
  vr_device_path: string = '';
  socket_names: string[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'vr_device_api', type: 'enum:MikanVRDeviceApi' },
    { name: 'vr_device_type', type: 'enum:MikanVRDeviceType' },
    { name: 'vr_device_index', type: 'int32' },
    { name: 'vr_device_path', type: 'string' },
    { name: 'socket_names', type: 'string', isArray: true }
  ];
}

export class MikanVRObjectSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'VRObjectSystem';
  vr_device_path_list: string[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'vr_device_path_list', type: 'string', isArray: true }
  ];
}

