import { MikanResponse } from './MikanAPITypes.js';

export class MikanResponseFuture {
  private promise: Promise<MikanResponse>;
  private requestId: number;

  constructor(promise: Promise<MikanResponse>, requestId: number) {
    this.promise = promise;
    this.requestId = requestId;
  }

  public getRequestId(): number {
    return this.requestId;
  }

  public isValid(): boolean {
    return this.promise !== null;
  }

  public async getResponse(): Promise<MikanResponse> {
    return this.promise;
  }

  public async await(): Promise<MikanResponse> {
    return this.promise;
  }

  public then<TResult1 = MikanResponse, TResult2 = never>(
    onfulfilled?: ((value: MikanResponse) => TResult1 | PromiseLike<TResult1>) | null,
    onrejected?: ((reason: any) => TResult2 | PromiseLike<TResult2>) | null
  ): Promise<TResult1 | TResult2> {
    return this.promise.then(onfulfilled, onrejected);
  }

  public catch<TResult = never>(
    onrejected?: ((reason: any) => TResult | PromiseLike<TResult>) | null
  ): Promise<MikanResponse | TResult> {
    return this.promise.catch(onrejected);
  }
}
