#pragma once

#include "ComponentFwd.h"
#include "NodeFwd.h"
#include "NodeError.h"

#include <memory>
#include <string>
#include <vector>

class NodeEvaluator
{
public:
	NodeEvaluator()= default;

	inline NodeEvaluator& setCurrentGraphicsContext(class IMkGraphicsContext* inGraphicsContext)
	{
		m_currentGraphicsContext= inGraphicsContext;
		return *this;
	}
	inline class IMkGraphicsContext* getCurrentGraphicsContext() const { return m_currentGraphicsContext; }

	inline NodeEvaluator& setDeltaSeconds(float inDeltaSeconds)
	{
		m_deltaSeconds= inDeltaSeconds;
		return *this;
	}
	inline float getDeltaSeconds() const { return m_deltaSeconds; }

	inline void setDisableInputEvaluation(bool bDisable) { m_bDisableInputEvaluation= bDisable; }
	inline bool getIsInputEvaluationDisabled() const { return m_bDisableInputEvaluation; }

	inline void addError(const NodeEvaluationError& error)
	{
		m_errors.push_back(error);
		m_errors.back().severity= eNodeEvaluationSeverity::error;
		m_bHasErrors= true;
	}

	/// Reports something the author should see about a node that still produced a usable
	/// result. Unlike addError this does not stop the flow pin chain, so the nodes after
	/// the one that warned still evaluate.
	inline void addWarning(const NodeEvaluationError& warning)
	{
		m_errors.push_back(warning);
		m_errors.back().severity= eNodeEvaluationSeverity::warning;
	}

	/// True once a fatal diagnostic has been raised. Warnings never set it, which is what
	/// lets evaluateFlowPinChain keep going past one.
	inline bool hasErrors() const { return m_bHasErrors; }
	/// Every diagnostic in emission order, warnings included
	inline const std::vector<NodeEvaluationError>& getErrors() const { return m_errors; }

	bool evaluateFlowPinChain(NodePtr startNode);

protected:
	class IMkGraphicsContext* m_currentGraphicsContext= nullptr;
	float m_deltaSeconds= 0.f;
	CompositorComponentPtr m_compositor;

	NodePtr m_currentNode;
	int m_evaluatedNodeCount= 0;
	std::vector<NodeEvaluationError> m_errors;
	bool m_bHasErrors= false;
	bool m_bDisableInputEvaluation= false;

	static const int kInifiniteLoopThreshold= 1000;
};
