// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './../MikanAPITypes.js';
import { MikanDMXData } from './MikanLightTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanLightDMXDataChangedEvent extends MikanEvent {
  dmx_data: MikanDMXData = new MikanDMXData();

  constructor() {
    super();
    this.eventTypeName = 'MikanLightDMXDataChangedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'dmx_data', type: 'MikanDMXData' }
  ];
}

