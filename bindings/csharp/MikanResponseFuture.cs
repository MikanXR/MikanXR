using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MikanXR
{
	public class MikanResponseFuture
	{		
		private MikanRequestManager _ownerRequestManager = null;
		private int _requestId = (int)MikanConstants.InvalidMikanID;
		private Task<MikanResponse> _future = null;

		public MikanResponseFuture()
		{
		}

		public MikanResponseFuture(MikanAPIResult result)
		{
			// Immediately set the result on the future
			_future = Task.FromResult(makeSimpleMikanResponse(result));
		}

		public MikanResponseFuture(
			MikanRequestManager owner,
			int requestId,
			TaskCompletionSource<MikanResponse> promise)
		{
			_ownerRequestManager= owner;
			_requestId= requestId;
			_future= promise.Task;
		}

		public MikanResponseFuture(MikanResponseFuture other)
		{
			_ownerRequestManager = other._ownerRequestManager;
			_requestId = other._requestId;
			_future = other._future;
		}

		public bool IsValid()
		{ 
			return _future != null; 
		}

		public bool IsCompleted()
		{
			return IsValid() && _future.IsCompleted;
		}

		// Non-Blocking Response Fetch
		// Return true if the response is ready, false otherwise
		public bool TryFetchResponse(out MikanResponse response)
		{
			response = null;

			if (IsCompleted())
			{
				response = _future.Result;
				return true;
			}

			return false;
		}

		// Blocking Response Fetch
		// Return the response if it is ready, or a timeout response if the timeout is reached
		public MikanResponse FetchResponse(double timeoutMilliseconds= 1000)
		{
			if (IsValid())
			{
				if (timeoutMilliseconds > 0)
				{
					// Wait for a fixed timeout
					_future.Wait(TimeSpan.FromMilliseconds(timeoutMilliseconds));

					if (_future.IsCompleted && !_future.IsFaulted)
					{
						return _future.Result;
					}
					else
					{
						// Timeout reached, cancel the request
						if (_ownerRequestManager != null &&
							_requestId != (int)MikanConstants.InvalidMikanID)
						{
							_ownerRequestManager.CancelRequest(_requestId);
						}

						// Return a timeout response instead
						return makeSimpleMikanResponse(MikanAPIResult.Timeout);
					}
				}
				else
				{
					// Wait indefinitely
					_future.Wait();

					// Return the result once the task succeeds or fails
					return _future.Result;
				}
			}
			else
			{
				return makeSimpleMikanResponse(MikanAPIResult.Uninitialized);
			}
		}

		// Blocking Response Await
		// Returns once the response has been received or the timeout is reached
		public void AwaitResponse(double timeoutMilliseconds= 1000)
		{
			// Drop the response on the floor
			FetchResponse(timeoutMilliseconds);
		}

		static MikanResponse makeSimpleMikanResponse(MikanAPIResult result)
		{
			var response = new MikanResponse();
			response.responseTypeName = nameof(MikanResponse);
			response.requestId = (int)MikanConstants.InvalidMikanID;
			response.resultCode = result;

			return response;
		}

	}
}
