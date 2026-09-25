// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetAppStageInfo : MikanRequest
	{

		public GetAppStageInfo()
		{
			requestTypeName = "GetAppStageInfo";
		}
	};

	public class MikanAppStageInfoResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanAppStageInfo app_stage_info;

		public MikanAppStageInfoResponse()
		{
			responseTypeName = "MikanAppStageInfoResponse";
		}
	};

	public class MikanRemoteControlCommand : MikanRequest
	{
		[MikanFieldOrder(0)] public string command;
		[MikanFieldOrder(1)] public List<string> parameters;

		public MikanRemoteControlCommand()
		{
			requestTypeName = "MikanRemoteControlCommand";
		}
	};

	public class MikanRemoteControlCommandResult : MikanResponse
	{
		[MikanFieldOrder(0)] public List<string> results;

		public MikanRemoteControlCommandResult()
		{
			responseTypeName = "MikanRemoteControlCommandResult";
		}
	};

	public class PopAppStage : MikanRequest
	{

		public PopAppStage()
		{
			requestTypeName = "PopAppStage";
		}
	};

	public class PushAppStage : MikanRequest
	{
		[MikanFieldOrder(0)] public string app_state_name;

		public PushAppStage()
		{
			requestTypeName = "PushAppStage";
		}
	};

}
