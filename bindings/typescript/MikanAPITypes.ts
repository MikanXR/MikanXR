import { MikanAPIResult } from './types/MikanAPITypes.js';

/**
 * The bases of every request, response and event. The JSON-RPC envelope carries their members
 * rather than the struct's own JSON, so these are written here instead of generated: none of
 * their members is a wire field, so none of them carries __serializationMetadata.
 *
 * The generated subclasses stamp their class name into the type name member on construction,
 * and that name is what TypeRegistry creates each one by.
 */
export class MikanRequest {
  // The JSON-RPC method
  requestTypeName: string = '';
  // The JSON-RPC id, stamped by MikanRequestManager when the request is sent
  requestId: number = -1;

  // The name of the response class this request is answered with, which a result is read into
  // since it carries no type name. Each generated request names its own.
  static readonly responseTypeName: string = 'MikanResponse';

  constructor() {
    this.requestTypeName = 'MikanRequest';
  }
}

export class MikanResponse {
  // The class the result was read into
  responseTypeName: string = '';
  // The id of the request this answers
  requestId: number = -1;
  // Success for a result, and the error's data.resultCode for an error
  resultCode: MikanAPIResult = MikanAPIResult.Success;

  constructor() {
    this.responseTypeName = 'MikanResponse';
  }
}

export class MikanEvent {
  // The JSON-RPC method of the notification the event arrived in
  eventTypeName: string = '';

  constructor() {
    this.eventTypeName = 'MikanEvent';
  }
}
