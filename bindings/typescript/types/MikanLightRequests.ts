// This file is auto generated. DO NOT EDIT.

import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanDMXData } from './MikanLightTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class GetDMXData extends MikanRequest {
  dmx_universe_ids: number[] = [];

  constructor() {
    super();
    this.requestTypeName = 'GetDMXData';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'dmx_universe_ids', type: 'int32', isArray: true }
  ];
}

export class MikanDMXDataResponse extends MikanResponse {
  dmx_data: MikanDMXData = new MikanDMXData();

  constructor() {
    super();
    this.responseTypeName = 'MikanDMXDataResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'dmx_data', type: 'MikanDMXData' }
  ];
}

export class SetLightDMXDataSubcription extends MikanRequest {
  light_ids: number[] = [];
  subscribe: boolean = false;

  constructor() {
    super();
    this.requestTypeName = 'SetLightDMXDataSubcription';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'light_ids', type: 'int32', isArray: true },
    { name: 'subscribe', type: 'boolean' }
  ];
}

