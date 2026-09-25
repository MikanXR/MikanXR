// This file is auto generated. DO NOT EDIT.

import { MikanRequest } from './MikanAPITypes.js';
import { MikanClientInfo } from './MikanClientTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class DisposeClientRequest extends MikanRequest {
  clientId: string = '';

  constructor() {
    super();
    this.requestTypeName = 'DisposeClientRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'clientId', type: 'string' }
  ];
}

export class InitClientRequest extends MikanRequest {
  clientInfo: MikanClientInfo = new MikanClientInfo();

  constructor() {
    super();
    this.requestTypeName = 'InitClientRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'clientInfo', type: 'MikanClientInfo' }
  ];
}

