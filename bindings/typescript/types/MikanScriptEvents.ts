// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './../MikanAPITypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanScriptMessagePostedEvent extends MikanEvent {
  message: string = '';

  constructor() {
    super();
    this.eventTypeName = 'MikanScriptMessagePostedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'message', type: 'string' }
  ];
}

