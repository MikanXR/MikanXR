// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanAPIResult
	{
		Success= 0,
		GeneralError= 1,
		Uninitialized= 2,
		NullParam= 3,
		InvalidParam= 4,
		RequestFailed= 5,
		NotConnected= 6,
		AlreadyConnected= 7,
		SocketError= 8,
		Timeout= 9,
		Canceled= 10,
		NoData= 11,
		BufferTooSmall= 12,
		UnknownClient= 13,
		UnknownFunction= 14,
		MalformedParameters= 15,
		MalformedResponse= 16,
		NoVideoSource= 100,
		NoVideoSourceAssignedTracker= 101,
		InvalidDeviceId= 102,
		InvalidStencilID= 103,
		InvalidAnchorID= 104,
		InvalidCameraID= 105,
		InvalidSceneID= 106,
		InvalidStageID= 106,
		InvalidShapeID= 107,
	};

	internal static class MikanAPIResultJson
	{
		public static JToken ToJson(MikanAPIResult value)
		{
			switch (value)
			{
				case MikanAPIResult.Success: return MikanJson.WriteString("Success");
				case MikanAPIResult.GeneralError: return MikanJson.WriteString("GeneralError");
				case MikanAPIResult.Uninitialized: return MikanJson.WriteString("Uninitialized");
				case MikanAPIResult.NullParam: return MikanJson.WriteString("NullParam");
				case MikanAPIResult.InvalidParam: return MikanJson.WriteString("InvalidParam");
				case MikanAPIResult.RequestFailed: return MikanJson.WriteString("RequestFailed");
				case MikanAPIResult.NotConnected: return MikanJson.WriteString("NotConnected");
				case MikanAPIResult.AlreadyConnected: return MikanJson.WriteString("AlreadyConnected");
				case MikanAPIResult.SocketError: return MikanJson.WriteString("SocketError");
				case MikanAPIResult.Timeout: return MikanJson.WriteString("Timeout");
				case MikanAPIResult.Canceled: return MikanJson.WriteString("Canceled");
				case MikanAPIResult.NoData: return MikanJson.WriteString("NoData");
				case MikanAPIResult.BufferTooSmall: return MikanJson.WriteString("BufferTooSmall");
				case MikanAPIResult.UnknownClient: return MikanJson.WriteString("UnknownClient");
				case MikanAPIResult.UnknownFunction: return MikanJson.WriteString("UnknownFunction");
				case MikanAPIResult.MalformedParameters: return MikanJson.WriteString("MalformedParameters");
				case MikanAPIResult.MalformedResponse: return MikanJson.WriteString("MalformedResponse");
				case MikanAPIResult.NoVideoSource: return MikanJson.WriteString("NoVideoSource");
				case MikanAPIResult.NoVideoSourceAssignedTracker: return MikanJson.WriteString("NoVideoSourceAssignedTracker");
				case MikanAPIResult.InvalidDeviceId: return MikanJson.WriteString("InvalidDeviceId");
				case MikanAPIResult.InvalidStencilID: return MikanJson.WriteString("InvalidStencilID");
				case MikanAPIResult.InvalidAnchorID: return MikanJson.WriteString("InvalidAnchorID");
				case MikanAPIResult.InvalidCameraID: return MikanJson.WriteString("InvalidCameraID");
				case MikanAPIResult.InvalidSceneID: return MikanJson.WriteString("InvalidSceneID");
				case MikanAPIResult.InvalidShapeID: return MikanJson.WriteString("InvalidShapeID");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanAPIResult has no wire name for " + (long)value);
		}

		public static MikanAPIResult FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanAPIResult)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Success": return MikanAPIResult.Success;
				case "GeneralError": return MikanAPIResult.GeneralError;
				case "Uninitialized": return MikanAPIResult.Uninitialized;
				case "NullParam": return MikanAPIResult.NullParam;
				case "InvalidParam": return MikanAPIResult.InvalidParam;
				case "RequestFailed": return MikanAPIResult.RequestFailed;
				case "NotConnected": return MikanAPIResult.NotConnected;
				case "AlreadyConnected": return MikanAPIResult.AlreadyConnected;
				case "SocketError": return MikanAPIResult.SocketError;
				case "Timeout": return MikanAPIResult.Timeout;
				case "Canceled": return MikanAPIResult.Canceled;
				case "NoData": return MikanAPIResult.NoData;
				case "BufferTooSmall": return MikanAPIResult.BufferTooSmall;
				case "UnknownClient": return MikanAPIResult.UnknownClient;
				case "UnknownFunction": return MikanAPIResult.UnknownFunction;
				case "MalformedParameters": return MikanAPIResult.MalformedParameters;
				case "MalformedResponse": return MikanAPIResult.MalformedResponse;
				case "NoVideoSource": return MikanAPIResult.NoVideoSource;
				case "NoVideoSourceAssignedTracker": return MikanAPIResult.NoVideoSourceAssignedTracker;
				case "InvalidDeviceId": return MikanAPIResult.InvalidDeviceId;
				case "InvalidStencilID": return MikanAPIResult.InvalidStencilID;
				case "InvalidAnchorID": return MikanAPIResult.InvalidAnchorID;
				case "InvalidCameraID": return MikanAPIResult.InvalidCameraID;
				case "InvalidSceneID": return MikanAPIResult.InvalidSceneID;
				case "InvalidStageID": return MikanAPIResult.InvalidStageID;
				case "InvalidShapeID": return MikanAPIResult.InvalidShapeID;
			}
			throw new Exception("MikanAPIResult has no value named " + name);
		}
	};

}
