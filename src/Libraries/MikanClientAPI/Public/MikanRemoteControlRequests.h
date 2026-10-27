#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanRemoteControlTypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanRemoteControlRequests.rfkh.h"
#endif

// Remote Control Request Types
// ------

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanRemoteControlRequest")) PushAppStage : public MikanRequest
{
	PushAppStage(){MIKAN_REQUEST_TYPE_INFO_INIT(PushAppStage)}

	FIELD() Serialization::String app_state_name;

#ifdef MIKANAPI_REFLECTION_ENABLED
	PushAppStage_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanRemoteControlRequest")) PopAppStage : public MikanRequest
{
	PopAppStage(){MIKAN_REQUEST_TYPE_INFO_INIT(PopAppStage)}

#ifdef MIKANAPI_REFLECTION_ENABLED
	PopAppStage_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanRemoteControlRequest"),
						Serialization::ResponseType("MikanAppStageInfoResponse")) GetAppStageInfo : public MikanRequest
{
	GetAppStageInfo(){MIKAN_REQUEST_TYPE_INFO_INIT(GetAppStageInfo)}

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetAppStageInfo_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanRemoteControlRequest"),
						Serialization::ResponseType("MikanRemoteControlCommandResult")) MikanRemoteControlCommand
	: public MikanRequest
{
	MikanRemoteControlCommand(){MIKAN_REQUEST_TYPE_INFO_INIT(MikanRemoteControlCommand)}

	FIELD() Serialization::String command;

	FIELD() Serialization::List<Serialization::String> parameters;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanRemoteControlCommand_GENERATED
#endif
};

// Remote Control Response Types
// ------

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanRemoteControlRequest")) MikanAppStageInfoResponse
	: public MikanResponse
{
	MikanAppStageInfoResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanAppStageInfoResponse)}

	FIELD() MikanAppStageInfo app_stage_info;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanAppStageInfoResponse_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanRemoteControlRequest")) MikanRemoteControlCommandResult
	: public MikanResponse
{
	MikanRemoteControlCommandResult(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanRemoteControlCommandResult)}

	FIELD() Serialization::List<Serialization::String> results;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanRemoteControlCommandResult_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanRemoteControlRequests_GENERATED
#endif