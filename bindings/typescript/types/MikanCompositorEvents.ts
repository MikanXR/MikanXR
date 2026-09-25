// This file is auto generated. DO NOT EDIT.

import { MikanEvent } from './MikanAPITypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanCompositorStoppedEvent extends MikanEvent {
  compositor_id: number = -1;
  camera_id: number = -1;

  constructor() {
    super();
    this.eventTypeName = 'MikanCompositorStoppedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'compositor_id', type: 'int32' },
    { name: 'camera_id', type: 'int32' }
  ];
}

export class MikanCompositorStartedEvent extends MikanEvent {
  compositor_id: number = -1;
  camera_id: number = -1;

  constructor() {
    super();
    this.eventTypeName = 'MikanCompositorStartedEvent';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'compositor_id', type: 'int32' },
    { name: 'camera_id', type: 'int32' }
  ];
}

