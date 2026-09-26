// This file is auto generated. DO NOT EDIT.

import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanAppStageInfo } from './MikanRemoteControlTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class GetAppStageInfo extends MikanRequest {

  constructor() {
    super();
    this.requestTypeName = 'GetAppStageInfo';
  }

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanAppStageInfoResponse extends MikanResponse {
  app_stage_info: MikanAppStageInfo = new MikanAppStageInfo();

  constructor() {
    super();
    this.responseTypeName = 'MikanAppStageInfoResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'app_stage_info', type: 'MikanAppStageInfo' }
  ];
}

export class MikanRemoteControlCommand extends MikanRequest {
  command: string = '';
  parameters: string[] = [];

  constructor() {
    super();
    this.requestTypeName = 'MikanRemoteControlCommand';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'command', type: 'string' },
    { name: 'parameters', type: 'string', isArray: true }
  ];
}

export class MikanRemoteControlCommandResult extends MikanResponse {
  results: string[] = [];

  constructor() {
    super();
    this.responseTypeName = 'MikanRemoteControlCommandResult';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'results', type: 'string', isArray: true }
  ];
}

export class PopAppStage extends MikanRequest {

  constructor() {
    super();
    this.requestTypeName = 'PopAppStage';
  }

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class PushAppStage extends MikanRequest {
  app_state_name: string = '';

  constructor() {
    super();
    this.requestTypeName = 'PushAppStage';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'app_state_name', type: 'string' }
  ];
}

