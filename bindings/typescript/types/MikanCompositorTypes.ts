// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanCompositorComponentValues extends MikanComponentValues {
  static readonly k_componentClassName = 'CompositorComponent';
  static readonly k_ownerSystemName = 'CompositorObjectSystem';
  owner_scene_id: number = -1;
  compositor_graph_path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'owner_scene_id', type: 'int32' },
    { name: 'compositor_graph_path', type: 'string' }
  ];
}

