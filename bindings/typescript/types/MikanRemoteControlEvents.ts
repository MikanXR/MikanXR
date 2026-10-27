// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './../MikanAPITypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanAppStageChangedEvent extends MikanEvent {
  new_app_state_name: string = '';
  old_app_state_name: string = '';

  constructor() {
    super();
    this.eventTypeName = 'MikanAppStageChangedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'new_app_state_name', type: 'string' },
    { name: 'old_app_state_name', type: 'string' }
  ];
}

export class MikanRemoteControlEvent extends MikanEvent {
  remoteControlEvent: string = '';
  parameters: string[] = [];

  constructor() {
    super();
    this.eventTypeName = 'MikanRemoteControlEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'remoteControlEvent', type: 'string' },
    { name: 'parameters', type: 'string', isArray: true }
  ];
}

