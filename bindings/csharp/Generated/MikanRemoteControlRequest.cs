// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
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
		public MikanAppStageInfo app_stage_info;

		public MikanAppStageInfoResponse()
		{
			responseTypeName = "MikanAppStageInfoResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("app_stage_info", MikanJson.WriteObject(this.app_stage_info));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.app_stage_info = MikanJson.ReadObject(MikanJson.Field(json, "app_stage_info"), this.app_stage_info ?? new MikanAppStageInfo());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "app_stage_info": value = this.app_stage_info; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "app_stage_info": return MikanJson.TryAssign(value, ref this.app_stage_info);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRemoteControlCommand : MikanRequest
	{
		public string command;
		public List<string> parameters;

		public MikanRemoteControlCommand()
		{
			requestTypeName = "MikanRemoteControlCommand";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("command", MikanJson.WriteString(this.command));
			json.Add("parameters", MikanJson.WriteList(this.parameters, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.command = MikanJson.ReadString(MikanJson.Field(json, "command"));
			this.parameters = MikanJson.ReadList(MikanJson.Field(json, "parameters"), this.parameters, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "command": value = this.command; return true;
				case "parameters": value = this.parameters; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "command": return MikanJson.TryAssign(value, ref this.command);
				case "parameters": return MikanJson.TryAssign(value, ref this.parameters);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRemoteControlCommandResult : MikanResponse
	{
		public List<string> results;

		public MikanRemoteControlCommandResult()
		{
			responseTypeName = "MikanRemoteControlCommandResult";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("results", MikanJson.WriteList(this.results, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.results = MikanJson.ReadList(MikanJson.Field(json, "results"), this.results, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "results": value = this.results; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "results": return MikanJson.TryAssign(value, ref this.results);
			}
			return base.TrySetField(fieldName, value);
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
		public string app_state_name;

		public PushAppStage()
		{
			requestTypeName = "PushAppStage";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("app_state_name", MikanJson.WriteString(this.app_state_name));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.app_state_name = MikanJson.ReadString(MikanJson.Field(json, "app_state_name"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "app_state_name": value = this.app_state_name; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "app_state_name": return MikanJson.TryAssign(value, ref this.app_state_name);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
