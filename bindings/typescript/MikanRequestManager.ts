import {
  MikanRequest,
  MikanResponse,
  MikanAPIResult,
} from './types/index.js';
import { MikanResponseFuture } from './MikanResponseFuture.js';
import {
  serializeToJsonString,
  deserializeFromJsonString,
  TypeRegistry
} from './Serialization/index.js';

interface PendingRequest {
  requestId: number;
  resolve: (response: MikanResponse) => void;
  reject: (error: Error) => void;
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

    // Create promise for response
    const promise = new Promise<MikanResponse>((resolve, reject) => {
      if (!this.sendFunction) {
        const errorResponse: MikanResponse = {
          responseTypeName: 'MikanResponse',
          requestId: request.requestId,
          resultCode: MikanAPIResult.NotConnected
        };
        resolve(errorResponse);
        return;
      }

      // Store pending request
      this.pendingRequests.set(request.requestId, {
        requestId: request.requestId,
        resolve,
        reject
      });

      try {
        // Serialize and send — convert bigints to unquoted JSON numbers
        let jsonString = JSON.stringify(request, (key, value) => {
          if (typeof value === 'bigint') {
            return `__BIGINT__${value.toString()}__BIGINT__`;
          }
          return value;
        });
        jsonString = jsonString.replace(/"__BIGINT__(-?\d+)__BIGINT__"/g, '$1');

        this.sendFunction(jsonString);
      } catch (error) {
        this.pendingRequests.delete(request.requestId);
        reject(error);
      }
    });

    return new MikanResponseFuture(promise, request.requestId);
  }

  public handleResponse(responseJson: string): void {
    try {
      const response = this.parseResponse(responseJson);

      if (response) {
        const pendingRequest = this.pendingRequests.get(response.requestId);

        if (pendingRequest) {
          this.pendingRequests.delete(response.requestId);
          pendingRequest.resolve(response);
        }
      }
    } catch (error) {
      console.error('Failed to handle response:', error);
    }
  }

  private parseResponse(responseJson: string): MikanResponse | null {
    try {
      const parsed = JSON.parse(responseJson);

      if (!parsed.responseTypeName) {
        console.error('Response missing responseTypeName');
        return null;
      }

      // Use TypeRegistry for proper deserialization when the response type is known.
      // This correctly handles nested PolymorphicObject fields (e.g. valuesObject).
      const responseType = TypeRegistry.get(parsed.responseTypeName);
      if (responseType) {
        const response = new responseType();
        deserializeFromJsonString(responseJson, response, responseType);
        return response as MikanResponse;
      }

      // Fallback for unknown response types: shallow-copy JSON fields.
      let resultCode = MikanAPIResult.Success;
      if (parsed.resultCode !== undefined) {
        if (typeof parsed.resultCode === 'string') {
          resultCode = (MikanAPIResult as any)[parsed.resultCode];
          if (resultCode === undefined) {
            console.warn(`Unknown resultCode string: ${parsed.resultCode}`);
            resultCode = MikanAPIResult.GeneralError;
          }
        } else {
          resultCode = Number(parsed.resultCode);
        }
      }

      const response: MikanResponse = {
        responseTypeName: parsed.responseTypeName,
        requestId: parsed.requestId,
        resultCode: resultCode
      };

      // Copy any additional properties from specific response types
      Object.assign(response, parsed);

      // Restore the numeric resultCode after Object.assign
      response.resultCode = resultCode;

      return response;
    } catch (error) {
      console.error('Failed to parse response:', error);
      return null;
    }
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
