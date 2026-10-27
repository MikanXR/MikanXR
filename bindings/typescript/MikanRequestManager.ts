import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanAPIResult } from './types/index.js';
import { MikanResponseFuture } from './MikanResponseFuture.js';
import {
  serializeToJson,
  deserializeFromJson,
  TypeRegistry
} from './Serialization/index.js';
import {
  MikanJsonRpcFrameKind,
  classifyJsonRpcFrame,
  formatJsonRpcRequest,
  parseJsonRpcFrame
} from './MikanJsonRpc.js';

interface PendingRequest {
  requestId: number;
  // The class a result is read into, since a result carries no type name
  responseTypeName: string;
  resolve: (response: MikanResponse) => void;
  reject: (error: Error) => void;
}

function makeResponse(requestId: number, resultCode: MikanAPIResult): MikanResponse {
  const response = new MikanResponse();
  response.requestId = requestId;
  response.resultCode = resultCode;
  return response;
}

export class MikanRequestManager {
  private nextRequestId: number = 0;
  private pendingRequests: Map<number, PendingRequest> = new Map();
  private sendFunction: ((message: string) => void) | null = null;

  public setSendFunction(sendFn: ((message: string) => void) | null): void {
    this.sendFunction = sendFn;
  }

  public sendRequest(request: MikanRequest): MikanResponseFuture {
    // Validate that request has required fields
    if (!request.requestTypeName) {
      throw new Error('Request must have requestTypeName');
    }

    // Stamp with next request id
    request.requestId = this.nextRequestId++;

    const responseTypeName =
      (request.constructor as typeof MikanRequest).responseTypeName ?? MikanRequest.responseTypeName;

    // Create promise for response
    const promise = new Promise<MikanResponse>((resolve, reject) => {
      if (!this.sendFunction) {
        resolve(makeResponse(request.requestId, MikanAPIResult.NotConnected));
        return;
      }

      // Store pending request
      this.pendingRequests.set(request.requestId, {
        requestId: request.requestId,
        responseTypeName,
        resolve,
        reject
      });

      try {
        // The request class's own fields are the params, since the envelope members are not
        // wire fields, and its type name is the method
        const requestFrame = formatJsonRpcRequest(
          request.requestTypeName, serializeToJson(request), request.requestId);

        this.sendFunction(requestFrame);
      } catch (error) {
        this.pendingRequests.delete(request.requestId);
        reject(error);
      }
    });

    return new MikanResponseFuture(promise, request.requestId);
  }

  public handleResponse(responseJson: string): void {
    const frame = parseJsonRpcFrame(responseJson);
    if (classifyJsonRpcFrame(frame) !== MikanJsonRpcFrameKind.Response) {
      console.error('Not a response:', responseJson);
      return;
    }

    // Requests are only ever sent with integer ids, so any other id answers nothing this
    // client sent, such as the null id of a reply to an unreadable frame
    const requestId = frame.id;
    if (!Number.isInteger(requestId)) {
      console.error('Received a response to no request of ours:', responseJson);
      return;
    }

    const pendingRequest = this.pendingRequests.get(requestId);
    if (!pendingRequest) {
      console.error(`Invalid pending request id(${requestId}) for response`);
      return;
    }
    this.pendingRequests.delete(requestId);

    // A response that fails to parse still completes its request, as malformed, rather than
    // leaving it to time out
    const response =
      this.parseResponse(frame, pendingRequest.responseTypeName, responseJson) ??
      makeResponse(requestId, MikanAPIResult.MalformedResponse);
    response.requestId = requestId;

    pendingRequest.resolve(response);
  }

  private parseResponse(frame: any, responseTypeName: string, responseJson: string): MikanResponse | null {
    // An error answers with the plain response whatever the request expected
    if ('error' in frame) {
      const response = new MikanResponse();
      response.resultCode = this.getErrorResultCode(frame.error, responseJson);
      return response;
    }

    if (frame.result === null || typeof frame.result !== 'object') {
      console.error('Response result is not an object:', responseJson);
      return null;
    }

    // The response's constructor stamps its type name
    const responseType = TypeRegistry.get(responseTypeName);
    if (!responseType) {
      console.error(`Unknown response type: ${responseTypeName}`);
      return null;
    }

    const response = new responseType() as MikanResponse;
    if (!deserializeFromJson(frame.result, response, responseType)) {
      console.error(`Failed to deserialize ${responseTypeName} from:`, responseJson);
      return null;
    }

    response.resultCode = MikanAPIResult.Success;
    return response;
  }

  // An error's data carries the MikanAPIResult name, which is what fills in the response
  private getErrorResultCode(error: any, responseJson: string): MikanAPIResult {
    const resultCodeName = error?.data?.resultCode;
    if (typeof resultCodeName === 'string') {
      const resultCode = (MikanAPIResult as any)[resultCodeName];
      if (typeof resultCode === 'number') {
        return resultCode as MikanAPIResult;
      }
    }

    console.warn('Error carries no known resultCode:', responseJson);
    return MikanAPIResult.GeneralError;
  }

  public cancelRequest(requestId: number): MikanAPIResult {
    const pendingRequest = this.pendingRequests.get(requestId);

    if (pendingRequest) {
      this.pendingRequests.delete(requestId);
      pendingRequest.reject(new Error('Request cancelled'));
      return MikanAPIResult.Success;
    }

    return MikanAPIResult.InvalidParam;
  }

  public cancelAllRequests(): void {
    for (const [requestId, pendingRequest] of this.pendingRequests) {
      pendingRequest.reject(new Error('All requests cancelled'));
    }
    this.pendingRequests.clear();
  }
}
