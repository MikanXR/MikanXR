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
			success &= TestRequestNoTypeIdInJson();
			success &= TestResponseRoundTrip();
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

		// Verifies the serialized JSON has requestTypeName present and
		// requestTypeId absent — confirming the wire-format change is complete.
		private bool TestRequestNoTypeIdInJson()
		{
			const string testName = "request JSON has typeName but no typeId";
			bool success = true;

			try
			{
				var cmd = new MikanRemoteControlCommand();
				cmd.requestId = 1;
				cmd.parameters = new List<string> { "param1", "param2" };
				cmd.command = "test_command";

				string jsonString = JsonSerializer.serializeToJsonString(cmd);

				JObject j = JObject.Parse(jsonString);

				// requestTypeName must be present (needed for server routing)
				success = j.Property("requestTypeName") != null;
				Debug.Assert(success, "requestTypeName missing from serialized request JSON");

				success &= ((string)j["requestTypeName"] == "MikanRemoteControlCommand");
				Debug.Assert(success, "requestTypeName has wrong value in serialized JSON");

				success &= j.Property("requestTypeId") == null;
				Debug.Assert(success, "requestTypeId present in serialized request JSON");
			}
			catch (Exception e)
			{
				Console.WriteLine($"    {testName} - EXCEPTION: {e.Message}");
				success = false;
			}

			Console.WriteLine($"    {testName} - {(success ? "PASSED" : "FAILED")}");
			return success;
		}

		// Simulates the client-side parse path: serialize a response to JSON, then create it
		// through the type registry by the name it carries and read it back.
		private bool TestResponseRoundTrip()
		{
			const string testName = "response JSON round-trip via type registry";
			bool success = true;

			try
			{
				// Simulate a server-produced response JSON
				var expected = new MikanResponse
				{
					requestId = 99,
					resultCode = MikanAPIResult.Success
				};

				string jsonString = JsonSerializer.serializeToJsonString(expected);

				// Simulate client parsing: extract responseTypeName from JSON
				JObject j = JObject.Parse(jsonString);
				string responseTypeName = (string)j["responseTypeName"];

				success = !string.IsNullOrEmpty(responseTypeName);
				Debug.Assert(success, "responseTypeName missing from serialized JSON");

				// Create the concrete type by name
				var actual = MikanTypeRegistry.Create(responseTypeName) as MikanResponse;
				success &= actual != null;
				Debug.Assert(success, $"Type '{responseTypeName}' not created by the type registry");

				bool bDeserialize = actual != null && JsonDeserializer.deserializeFromJson(j, actual);

				success &= bDeserialize;
				Debug.Assert(success, "Failed to deserialize response from JSON");

				// Verify field values survived the round-trip
				if (bDeserialize)
				{
					success &= (actual.requestId == expected.requestId);
					Debug.Assert(success, $"requestId mismatch: expected {expected.requestId}, got {actual.requestId}");

					success &= (actual.resultCode == expected.resultCode);
					Debug.Assert(success, $"resultCode mismatch: expected {expected.resultCode}, got {actual.resultCode}");

					success &= (actual.responseTypeName == expected.responseTypeName);
					Debug.Assert(success, $"responseTypeName mismatch: expected {expected.responseTypeName}, got {actual.responseTypeName}");
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
	}
}
