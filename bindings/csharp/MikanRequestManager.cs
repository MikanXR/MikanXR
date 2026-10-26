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

		public MikanResponseFuture AddResponseHandler(int requestId, MikanAPIResult result)
		{
			TaskCompletionSource<MikanResponse> promise = new TaskCompletionSource<MikanResponse>();
			var future= new MikanResponseFuture(this, requestId, promise);

			if (result == MikanAPIResult.Success)
			{
				PendingRequest pendingRequest = new PendingRequest()
				{
					requestId = requestId,
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

			// Serialize the request to a Json string
			string jsonRequestString= JsonSerializer.serializeToJsonString(request);

			// Send the request string to Mikan
			MikanAPIResult result = _transport.SendRequestJson(jsonRequestString);

			// Create a request handler
			return AddResponseHandler(request.requestId, result);
		}

		private void InternalTextResponseCallback(string utf8ResponseString)
		{
			// The request id is read from the message itself rather than handed in beside it, so
			// a transport only has to deliver the frame. A response that fails to parse still
			// completes its request, as malformed, rather than leaving it to time out.
			JObject root;
			try
			{
				root = JObject.Parse(utf8ResponseString);
			}
			catch (Exception e)
			{
				_logCallback(MikanLogLevel.Error, $"Malformed response JSON: {e.Message}");
				return;
			}

			if (!root.TryGetValue("requestId", out JToken requestIdElement) || requestIdElement.Type != JTokenType.Integer)
			{
				_logCallback(MikanLogLevel.Error, "Response carries no requestId: " + utf8ResponseString);
				return;
			}

			int requestId = (int)requestIdElement;
			PendingRequest pendingRequest = RemovePendingRequest(requestId);
			if (pendingRequest == null)
			{
				_logCallback(MikanLogLevel.Error, $"Invalid pending request id({requestId}) for text response");
				return;
			}

			MikanResponse response = ParseResponseString(utf8ResponseString, root);
			if (response == null)
			{
				response = new MikanResponse()
				{
					responseTypeName = nameof(MikanResponse),
					requestId = requestId,
					resultCode = MikanAPIResult.MalformedResponse
				};
			}

			pendingRequest.promise.SetResult(response);
		}

		private MikanResponse ParseResponseString(string utf8ResponseString, JObject root)
		{
			MikanResponse response = null;

			// Check if the key "responseType" exists
			if (root.TryGetValue("responseTypeName", out JToken responseTypeNameElement))
			{
				// Check if the value of "responseType" is a string
				if (responseTypeNameElement.Type == JTokenType.String)
				{
					// Get the string value of "responseType"
					string responseTypeName = (string)responseTypeNameElement;

					// Attempt to create the response object by class name
					if (MikanTypeRegistry.Create(responseTypeName) is MikanResponse responseObject)
					{
						// Deserialize the response object from the already parsed JSON
						if (JsonDeserializer.deserializeFromJson(root, responseObject))
						{
							response = responseObject;
						}
						else
						{
							_logCallback(
								MikanLogLevel.Error,
								"Failed to deserialize response object from JSON string: " + utf8ResponseString);
						}
					}
					else
					{
						_logCallback(MikanLogLevel.Error,
							"Unknown response type: " + responseTypeName +
							" (className: " + responseTypeName + ")");
					}
				}
				else
				{
					_logCallback(MikanLogLevel.Error, "responseTypes are not of expected types.");
				}
			}
			else
			{
				_logCallback(MikanLogLevel.Error, "responseType keys not found.");
			}

			return response;
		}
	}
}