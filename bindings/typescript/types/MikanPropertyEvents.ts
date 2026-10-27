// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './../MikanAPITypes.js';
import { MikanPropertyValue } from './MikanPropertyTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanPropertyUpdateEvent extends MikanEvent {
  propertyValue: MikanPropertyValue = new MikanPropertyValue();

  constructor() {
    super();
    this.eventTypeName = 'MikanPropertyUpdateEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'propertyValue', type: 'MikanPropertyValue' }
  ];
}

