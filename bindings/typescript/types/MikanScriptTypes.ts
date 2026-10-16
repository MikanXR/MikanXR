// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanScriptComponentValues extends MikanComponentValues {
  static readonly k_componentClassName = 'ScriptComponent';
  static readonly k_ownerSystemName = 'ScriptObjectSystem';
  script_path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'script_path', type: 'string' }
  ];
}

export class MikanScriptMessageInfo {
  content: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'content', type: 'string' }
  ];
}

