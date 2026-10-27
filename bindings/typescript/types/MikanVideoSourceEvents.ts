// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './../MikanAPITypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanVideoSourceAlignmentEvent extends MikanEvent {
  video_source_id: number = -1;
  is_aligned: boolean = false;
  marker_visible: boolean = false;
  is_watching: boolean = false;

  constructor() {
    super();
    this.eventTypeName = 'MikanVideoSourceAlignmentEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'video_source_id', type: 'int32' },
    { name: 'is_aligned', type: 'boolean' },
    { name: 'marker_visible', type: 'boolean' },
    { name: 'is_watching', type: 'boolean' }
  ];
}

export class MikanVideoSourceClosedEvent extends MikanEvent {

  constructor() {
    super();
    this.eventTypeName = 'MikanVideoSourceClosedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanVideoSourceModeChangedEvent extends MikanEvent {

  constructor() {
    super();
    this.eventTypeName = 'MikanVideoSourceModeChangedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanVideoSourceOpenedEvent extends MikanEvent {

  constructor() {
    super();
    this.eventTypeName = 'MikanVideoSourceOpenedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
  ];
}

