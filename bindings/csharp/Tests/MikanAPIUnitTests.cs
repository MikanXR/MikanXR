using System;
using System.Collections.Generic;
using System.Diagnostics;
using Newtonsoft.Json.Linq;

namespace MikanXR
{
	public class MikanAPIUnitTests
	{
		public bool RunAllTests()
		{
			Console.WriteLine("[MikanAPIUnitTests]");
			bool success = true;
			success &= TestRegistryCreatesKnownResponses();
			success &= TestRegistryCreatesKnownEvents();
			success &= TestTypeNameOnDerivedTypes();
			success &= TestRequestsNameKnownResponses();
			success &= TestRequestEnvelope();
			success &= TestResponseRoundTrip();
			success &= TestErrorResponseResultCode();
			success &= TestEventNotificationRoundTrip();
			Console.WriteLine($"  MikanAPIUnitTests module - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// MikanRequestManager creates responses through the type registry, so known response
		// types have to be resolvable by name
		private bool TestRegistryCreatesKnownResponses()
		{
			const string testName = "type registry creates known responses";
			bool success = true;

			try
			{
				// Core base type
				success = MikanTypeRegistry.Create("MikanResponse") is MikanResponse;
				Debug.Assert(success, "MikanResponse not created by the type registry");

				// A concrete derived response type
				success &= MikanTypeRegistry.Create("MikanRemoteControlCommandResult") is MikanRemoteControlCommandResult;
				Debug.Assert(success, "MikanRemoteControlCommandResult not created by the type registry");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// MikanEventManager creates events through the type registry, so known event types have
		// to be resolvable by name and unknown ones must not be
		private bool TestRegistryCreatesKnownEvents()
		{
			const string testName = "type registry creates known events";
			bool success = true;

			try
			{
				// Core base type
				success = MikanTypeRegistry.Create("MikanEvent") is MikanEvent;
				Debug.Assert(success, "MikanEvent not created by the type registry");

				// A concrete derived event type
				success &= MikanTypeRegistry.Create("MikanDisconnectedEvent") is MikanDisconnectedEvent;
				Debug.Assert(success, "MikanDisconnectedEvent not created by the type registry");

				// Unknown type must NOT be created
				success &= MikanTypeRegistry.Create("ThisTypeDoesNotExist") == null;
				Debug.Assert(success, "Unknown type unexpectedly created by the type registry");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// The wire routes requests, responses and events by the type name each stamps into
		// itself on construction, which has to be the name the registry creates it by
		private bool TestTypeNameOnDerivedTypes()
		{
			const string testName = "type name on derived types";
			bool success = true;

			try
			{
				success = (new MikanRequest().requestTypeName == "MikanRequest");
				Debug.Assert(success, "MikanRequest type name mismatch");

				success &= (new MikanResponse().responseTypeName == "MikanResponse");
				Debug.Assert(success, "MikanResponse type name mismatch");

				success &= (new MikanEvent().eventTypeName == "MikanEvent");
				Debug.Assert(success, "MikanEvent type name mismatch");

				// Derived request type name must match the concrete class, not the base
				success &= (new MikanRemoteControlCommand().requestTypeName == "MikanRemoteControlCommand");
				Debug.Assert(success, "MikanRemoteControlCommand type name mismatch");

				foreach (string typeName in MikanTypeRegistry.GetTypeNames())
				{
					string stampedName = null;
					MikanSerializableObject instance = MikanTypeRegistry.Create(typeName);
					if (instance is MikanRequest request)
						stampedName = request.requestTypeName;
					else if (instance is MikanResponse response)
						stampedName = response.responseTypeName;
					else if (instance is MikanEvent mikanEvent)
						stampedName = mikanEvent.eventTypeName;

					if (stampedName != null && stampedName != typeName)
					{
						Console.WriteLine($"    {typeName} stamps the type name {stampedName}");
						success = false;
					}
				}
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// Every request names a response the type registry can create, since a result carries no
		// type name and is read into whatever the request names
		private bool TestRequestsNameKnownResponses()
		{
			const string testName = "requests name known response types";
			bool success = true;

			try
			{
				success = new MikanRequest().ResponseTypeName == nameof(MikanResponse);
				Debug.Assert(success, "MikanRequest does not default to the plain MikanResponse");

				success &= new GetAppStageInfo().ResponseTypeName == nameof(MikanAppStageInfoResponse);
				Debug.Assert(success, "GetAppStageInfo does not name MikanAppStageInfoResponse");

				foreach (string typeName in MikanTypeRegistry.GetTypeNames())
				{
					if (MikanTypeRegistry.Create(typeName) is MikanRequest request &&
						!(MikanTypeRegistry.Create(request.ResponseTypeName) is MikanResponse))
					{
						Console.WriteLine($"    {typeName} names the unknown response type {request.ResponseTypeName}");
						success = false;
					}
				}
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// A request goes out as a JSON-RPC request whose method is the request's type name, whose
		// params are the struct's own fields and whose id is the request id
		private bool TestRequestEnvelope()
		{
			const string testName = "request is a JSON-RPC request whose params are the struct's fields";
			bool success = true;

			try
			{
				var transport = new FakeTransport();
				var requestManager = new MikanRequestManager(IgnoreLog);
				requestManager.Initialize(transport);

				var cmd = new MikanRemoteControlCommand();
				cmd.parameters = new List<string> { "param1", "param2" };
				cmd.command = "test_command";
				requestManager.SendRequest(cmd);

				JObject frame = JObject.Parse(transport.LastSentFrame);

				success = (string)frame["jsonrpc"] == "2.0";
				Debug.Assert(success, "jsonrpc is not 2.0");

				success &= (string)frame["method"] == "MikanRemoteControlCommand";
				Debug.Assert(success, "method is not the request's type name");

				success &= frame["id"].Type == JTokenType.Integer && (int)frame["id"] == cmd.requestId;
				Debug.Assert(success, "id is not the request id");

				var parameters = frame["params"] as JObject;
				success &= parameters != null && (string)parameters["command"] == "test_command";
				Debug.Assert(success, "params does not carry the request's fields");

				success &= parameters != null &&
					parameters.Property("requestTypeName") == null &&
					parameters.Property("requestId") == null;
				Debug.Assert(success, "params carries the envelope members");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// A result is read into the response type the request names, and an empty result
		// completes a request answered with the plain MikanResponse
		private bool TestResponseRoundTrip()
		{
			const string testName = "response result round-trip into the request's response type";
			bool success = true;

			try
			{
				var transport = new FakeTransport();
				var requestManager = new MikanRequestManager(IgnoreLog);
				requestManager.Initialize(transport);

				var request = new GetAppStageInfo();
				MikanResponseFuture future = requestManager.SendRequest(request);

				var expected = new MikanAppStageInfoResponse();
				expected.app_stage_info = new MikanAppStageInfo() { app_state_name = "MainMenu" };
				transport.Respond(MakeResultFrame(request.requestId, JsonSerializer.serializeToJson(expected)));

				success = future.TryFetchResponse(out MikanResponse response);
				Debug.Assert(success, "response not delivered");

				var typedResponse = response as MikanAppStageInfoResponse;
				success &= typedResponse != null;
				Debug.Assert(success, "response not read into MikanAppStageInfoResponse");

				if (typedResponse != null)
				{
					success &= typedResponse.app_stage_info.app_state_name == "MainMenu";
					Debug.Assert(success, "result field lost in the round-trip");

					success &= typedResponse.requestId == request.requestId;
					Debug.Assert(success, "requestId not taken from the envelope id");

					success &= typedResponse.resultCode == MikanAPIResult.Success;
					Debug.Assert(success, "resultCode of a result is not Success");

					success &= typedResponse.responseTypeName == nameof(MikanAppStageInfoResponse);
					Debug.Assert(success, "responseTypeName not stamped");
				}

				var plainRequest = new PopAppStage();
				MikanResponseFuture plainFuture = requestManager.SendRequest(plainRequest);
				transport.Respond(MakeResultFrame(plainRequest.requestId, new JObject()));

				success &= plainFuture.TryFetchResponse(out MikanResponse plainResponse) &&
					plainResponse.GetType() == typeof(MikanResponse) &&
					plainResponse.resultCode == MikanAPIResult.Success &&
					plainResponse.requestId == plainRequest.requestId;
				Debug.Assert(success, "empty result not read into the plain MikanResponse");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// An error answers with the plain MikanResponse, whose result code is the error data's
		// MikanAPIResult name
		private bool TestErrorResponseResultCode()
		{
			const string testName = "error response carries the MikanAPIResult name";
			bool success = true;

			try
			{
				var transport = new FakeTransport();
				var requestManager = new MikanRequestManager(IgnoreLog);
				requestManager.Initialize(transport);

				var request = new GetAppStageInfo();
				MikanResponseFuture future = requestManager.SendRequest(request);
				transport.Respond(
					"{\"jsonrpc\":\"2.0\",\"id\":" + request.requestId + "," +
					"\"error\":{\"code\":-32601,\"message\":\"UnknownFunction\"," +
					"\"data\":{\"resultCode\":\"UnknownFunction\"}}}");

				success = future.TryFetchResponse(out MikanResponse response) &&
					response.GetType() == typeof(MikanResponse) &&
					response.resultCode == MikanAPIResult.UnknownFunction &&
					response.requestId == request.requestId;
				Debug.Assert(success, "error not read into a plain MikanResponse with its result code");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// An event is a notification typed by its method, and the disconnect string still
		// becomes MikanDisconnectedEvent
		private bool TestEventNotificationRoundTrip()
		{
			const string testName = "event is a notification typed by its method";
			bool success = true;

			try
			{
				var transport = new FakeTransport();
				var eventManager = new MikanEventManager(IgnoreLog);
				eventManager.Initialize(transport);

				var expected = new MikanConnectedEvent();
				expected.serverVersion = new MikanClientAPIVersion() { version = 1 };
				expected.minClientVersion = new MikanClientAPIVersion() { version = 1 };
				expected.isClientCompatible = true;
				expected.httpPort = 8090;

				var frame = new JObject();
				frame.Add("jsonrpc", "2.0");
				frame.Add("method", nameof(MikanConnectedEvent));
				frame.Add("params", JsonSerializer.serializeToJson(expected));
				transport.QueueEvent(frame.ToString());

				success = eventManager.FetchNextEvent(out MikanEvent mikanEvent) == MikanAPIResult.Success;
				Debug.Assert(success, "event not fetched");

				var connectedEvent = mikanEvent as MikanConnectedEvent;
				success &= connectedEvent != null &&
					connectedEvent.eventTypeName == nameof(MikanConnectedEvent) &&
					connectedEvent.httpPort == 8090 &&
					connectedEvent.isClientCompatible;
				Debug.Assert(success, "notification not read into MikanConnectedEvent");

				transport.QueueEvent("disconnect:1000:bye");
				success &= eventManager.FetchNextEvent(out MikanEvent disconnectEvent) == MikanAPIResult.Success &&
					disconnectEvent is MikanDisconnectedEvent disconnected &&
					disconnected.code == MikanDisconnectCode.Normal &&
					disconnected.reason == "bye";
				Debug.Assert(success, "disconnect string not read into MikanDisconnectedEvent");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		private static string MakeResultFrame(int requestId, JObject result)
		{
			var frame = new JObject();
			frame.Add("jsonrpc", "2.0");
			frame.Add("id", requestId);
			frame.Add("result", result);

			return frame.ToString();
		}

		private static void IgnoreLog(MikanLogLevel logLevel, string logMessage)
		{
		}

		// Records what the managers send and hands back whatever a test feeds it, in place of
		// a connection to the editor
		private class FakeTransport : IMikanTransport
		{
			private MikanTextResponseHandler _textResponseHandler;
			private readonly Queue<string> _events = new Queue<string>();

			public string LastSentFrame { get; private set; }
			public string ClientName => "FakeTransport";

			public void Respond(string responseFrame)
			{
				_textResponseHandler?.Invoke(responseFrame);
			}

			public void QueueEvent(string eventFrame)
			{
				_events.Enqueue(eventFrame);
			}

			public MikanAPIResult Initialize(string clientName, MikanLogLevel minLogLevel, MikanLogCallback logCallback)
			{
				return MikanAPIResult.Success;
			}

			public bool GetIsInitialized() { return true; }
			public MikanAPIResult Shutdown() { return MikanAPIResult.Success; }
			public void SetResponseHandler(MikanTextResponseHandler textHandler) { _textResponseHandler = textHandler; }
			public MikanAPIResult Connect(string host, string port) { return MikanAPIResult.Success; }
			public MikanAPIResult Disconnect() { return MikanAPIResult.Success; }
			public bool GetIsConnected() { return true; }
			public void Dispose() { }

			public MikanAPIResult SendRequestJson(string utf8RequestJson)
			{
				LastSentFrame = utf8RequestJson;
				return MikanAPIResult.Success;
			}

			public MikanAPIResult FetchNextEvent(out string utf8EventString)
			{
				if (_events.Count > 0)
				{
					utf8EventString = _events.Dequeue();
					return MikanAPIResult.Success;
				}

				utf8EventString = null;
				return MikanAPIResult.NoData;
			}
		}
	}
}
