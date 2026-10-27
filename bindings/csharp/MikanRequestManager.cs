using System.Threading.Tasks;
using System.Collections.Generic;
using System;
using Newtonsoft.Json.Linq;

namespace MikanXR
{
	public class MikanRequestManager
	{
		private MikanLogCallback _logCallback;
		private IMikanTransport _transport;
		private int m_nextRequestID= 0;

		private class PendingRequest
		{
			public int requestId;
			// The struct a result is read into, since a result carries no type name
			public string responseTypeName;
			public TaskCompletionSource<MikanResponse> promise;
		};
		private Dictionary<int, PendingRequest> _pendingRequests;

		public MikanRequestManager(MikanLogCallback logCallback)
		{
			_logCallback = logCallback;
			_pendingRequests = new Dictionary<int, PendingRequest>();
		}

		public MikanAPIResult Initialize(IMikanTransport transport)
		{
			_transport = transport;
			_transport.SetResponseHandler(InternalTextResponseCallback);

			return MikanAPIResult.Success;
		}

		void InsertPendingRequest(PendingRequest pendingRequest)
		{
			lock (_pendingRequests)
			{
				_pendingRequests.Add(pendingRequest.requestId, pendingRequest);
			}
		}

		PendingRequest RemovePendingRequest(int requestId)
		{
			PendingRequest pendingRequest= null;

			lock(_pendingRequests)
			{
				if (_pendingRequests.TryGetValue(requestId, out pendingRequest))
				{
					_pendingRequests.Remove(requestId);
				}
			}

			return pendingRequest;
		}

		public MikanResponseFuture AddResponseHandler(
			int requestId,
			MikanAPIResult result,
			string responseTypeName= nameof(MikanResponse))
		{
			TaskCompletionSource<MikanResponse> promise = new TaskCompletionSource<MikanResponse>();
			var future= new MikanResponseFuture(this, requestId, promise);

			if (result == MikanAPIResult.Success)
			{
				PendingRequest pendingRequest = new PendingRequest()
				{
					requestId = requestId,
					responseTypeName = responseTypeName,
					promise = promise
				};

				InsertPendingRequest(pendingRequest);
			}
			else
			{
				MikanResponse response = new MikanResponse()
				{
					responseTypeName = nameof(MikanResponse),
					requestId = requestId,
					resultCode = result
				};

				promise.SetResult(response);
			}

			return future;
		}

		public MikanAPIResult CancelRequest(int requestId)
		{
			PendingRequest existingRequest = RemovePendingRequest(requestId);

			return existingRequest != null ? MikanAPIResult.Success : MikanAPIResult.InvalidParam;
		}

		public MikanResponseFuture SendRequest(MikanRequest request)
		{
			// Stamp the request with the next request id
			request.requestId= m_nextRequestID;
			m_nextRequestID++;

			// The request struct's own fields are the params, since the envelope members are not
			// wire fields, and its type name is the method
			JObject parameters= JsonSerializer.serializeToJson(request);
			string requestFrame= MikanJsonRpc.FormatRequest(request.requestTypeName, parameters, request.requestId);

			// Send the request frame to Mikan
			MikanAPIResult result = _transport.SendRequestJson(requestFrame);

			// Create a request handler
			return AddResponseHandler(request.requestId, result, request.ResponseTypeName);
		}

		private void InternalTextResponseCallback(string utf8ResponseString)
		{
			// The request id is read from the frame itself rather than handed in beside it, so
			// a transport only has to deliver the frame. A response that fails to parse still
			// completes its request, as malformed, rather than leaving it to time out.
			JObject frame= MikanJsonRpc.TryParse(utf8ResponseString);
			if (MikanJsonRpc.Classify(frame) != MikanJsonRpcFrameKind.Response)
			{
				_logCallback(MikanLogLevel.Error, "Not a response: " + utf8ResponseString);
				return;
			}

			// Requests are only ever sent with integer ids, so any other id answers nothing this
			// client sent, such as the null id of a reply to an unreadable frame
			JToken idToken= frame["id"];
			if (idToken.Type != JTokenType.Integer)
			{
				_logCallback(MikanLogLevel.Error, "Received a response to no request of ours: " + utf8ResponseString);
				return;
			}

			int requestId = (int)idToken;
			PendingRequest pendingRequest = RemovePendingRequest(requestId);
			if (pendingRequest == null)
			{
				_logCallback(MikanLogLevel.Error, $"Invalid pending request id({requestId}) for text response");
				return;
			}

			MikanResponse response = ParseResponse(utf8ResponseString, frame, pendingRequest.responseTypeName);
			if (response == null)
			{
				response = new MikanResponse()
				{
					resultCode = MikanAPIResult.MalformedResponse
				};
			}

			response.requestId= requestId;
			pendingRequest.promise.SetResult(response);
		}

		private MikanResponse ParseResponse(string utf8ResponseString, JObject frame, string responseTypeName)
		{
			// An error answers with the plain response whatever the request expected
			if (frame["error"] is JObject error)
			{
				return new MikanResponse()
				{
					resultCode = GetErrorResultCode(error, utf8ResponseString)
				};
			}

			if (!(frame["result"] is JObject result))
			{
				_logCallback(MikanLogLevel.Error, "Response result is not an object: " + utf8ResponseString);
				return null;
			}

			// The type registry is the reflection-free factory. The response's constructor stamps
			// its type name.
			if (!(MikanTypeRegistry.Create(responseTypeName) is MikanResponse response))
			{
				_logCallback(MikanLogLevel.Error, "Unknown response type: " + responseTypeName);
				return null;
			}

			if (!JsonDeserializer.deserializeFromJson(result, response))
			{
				_logCallback(
					MikanLogLevel.Error,
					$"Failed to deserialize {responseTypeName} from JSON string: {utf8ResponseString}");
				return null;
			}

			response.resultCode= MikanAPIResult.Success;

			return response;
		}

		// An error's data carries the MikanAPIResult name, which is what fills in the response
		private MikanAPIResult GetErrorResultCode(JObject error, string utf8ResponseString)
		{
			if (error["data"] is JObject data && data["resultCode"] is JToken resultCodeToken)
			{
				try
				{
					return MikanAPIResultJson.FromJson(resultCodeToken);
				}
				catch (Exception)
				{
					// Falls through to the warning below
				}
			}

			_logCallback(MikanLogLevel.Warning, "Error carries no known resultCode: " + utf8ResponseString);
			return MikanAPIResult.GeneralError;
		}
	}
}
