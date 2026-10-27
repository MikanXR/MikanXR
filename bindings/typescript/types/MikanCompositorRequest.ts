// This file is auto generated. DO NOT EDIT.

import { MikanRequest } from './../MikanAPITypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class SetCompositorReturnFeed extends MikanRequest {
  compositor_id: number = -1;
  host: string = '';
  port: number = 0;
  enable: boolean = false;

  constructor() {
    super();
    this.requestTypeName = 'SetCompositorReturnFeed';
  }

  static readonly responseTypeName: string = 'MikanResponse';

  static __serializationMetadata: SerializationField[] = [
    { name: 'compositor_id', type: 'int32' },
    { name: 'host', type: 'string' },
    { name: 'port', type: 'int32' },
    { name: 'enable', type: 'boolean' }
  ];
}

