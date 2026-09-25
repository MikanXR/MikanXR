#pragma once

#include "NodeFwd.h"

#include <string>
#include <vector>

enum eNodeEvaluationErrorCode
{
	NONE= -1,

	invalidNode,
	missingInput,
	missingOutput,
	materialError,
	evaluationError,
	infiniteLoop
};

/// How much a diagnostic costs the graph. An error stops the flow pin chain where it
/// was raised; a warning reports something an author should know about a node that
/// still produced a usable result, and evaluation carries on past it.
enum class eNodeEvaluationSeverity
{
	error,
	warning
};

struct NodeEvaluationError
{
	NodeEvaluationError()= default;
	NodeEvaluationError(eNodeEvaluationErrorCode inErrorCode, const std::string& inErrorMessage,
						class Node* inErrorNode= nullptr, class NodePin* inErrorPin= nullptr,
						eNodeEvaluationSeverity inSeverity= eNodeEvaluationSeverity::error);

	eNodeEvaluationErrorCode errorCode= NONE;
	eNodeEvaluationSeverity severity= eNodeEvaluationSeverity::error;
	std::string errorMessage;
	t_node_id errorNodeId= -1;
	t_node_pin_id errorPinId= -1;
};