// This file is auto generated. DO NOT EDIT.

import { MikanRequest } from './MikanAPITypes.js';
import { MikanScriptMessageInfo } from './MikanScriptTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class InvokeScriptTrigger extends MikanRequest {
  script_name: string = '';
  trigger_name: string = '';
  trigger_args: Record<string, string> = {};

  constructor() {
    super();
    this.requestTypeName = 'InvokeScriptTrigger';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'script_name', type: 'string' },
    { name: 'trigger_name', type: 'string' },
    { name: 'trigger_args', type: 'Map', isMap: true, keyType: 'string', valueType: 'string' }
  ];
}

export class SendScriptMessage extends MikanRequest {
  message: MikanScriptMessageInfo = new MikanScriptMessageInfo();

  constructor() {
    super();
    this.requestTypeName = 'SendScriptMessage';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'message', type: 'MikanScriptMessageInfo' }
  ];
}

