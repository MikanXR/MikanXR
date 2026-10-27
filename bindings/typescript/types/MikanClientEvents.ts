// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './../MikanAPITypes.js';
import { MikanDisconnectCode } from './MikanCoreConstants.js';
import { MikanClientAPIVersion } from './MikanCoreTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanConnectedEvent extends MikanEvent {
  serverVersion: MikanClientAPIVersion = new MikanClientAPIVersion();
  minClientVersion: MikanClientAPIVersion = new MikanClientAPIVersion();
  isClientCompatible: boolean = false;
  httpPort: number = 0;

  constructor() {
    super();
    this.eventTypeName = 'MikanConnectedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'serverVersion', type: 'MikanClientAPIVersion' },
    { name: 'minClientVersion', type: 'MikanClientAPIVersion' },
    { name: 'isClientCompatible', type: 'boolean' },
    { name: 'httpPort', type: 'int32' }
  ];
}

export class MikanDisconnectedEvent extends MikanEvent {
  code: MikanDisconnectCode = MikanDisconnectCode.Normal;
  reason: string = '';

  constructor() {
    super();
    this.eventTypeName = 'MikanDisconnectedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'code', type: 'enum:MikanDisconnectCode' },
    { name: 'reason', type: 'string' }
  ];
}

