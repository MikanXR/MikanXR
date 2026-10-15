#include "CaptureFlow.h"

namespace
{
const char* k_stepNames[(int)eCaptureFlowStep::COUNT]= {"idle",      "marker_image", "plate_image", "ready_to_record",
														"recording", "packing"};
}

const char* CaptureFlowUtils::stepToString(eCaptureFlowStep step)
{
	return step >= eCaptureFlowStep::idle && step < eCaptureFlowStep::COUNT ? k_stepNames[(int)step] : "";
}

const std::vector<eCaptureFlowStep>& CaptureFlow::stepsForKind(eCaptureKind kind)
{
	static const std::vector<eCaptureFlowStep> s_stillPhoto= {eCaptureFlowStep::markerImage,
															  eCaptureFlowStep::plateImage};
	static const std::vector<eCaptureFlowStep> s_stillCamera= {
		eCaptureFlowStep::markerImage, eCaptureFlowStep::plateImage, eCaptureFlowStep::readyToRecord,
		eCaptureFlowStep::recording};
	static const std::vector<eCaptureFlowStep> s_trackCamera= {eCaptureFlowStep::readyToRecord,
															   eCaptureFlowStep::recording};
	static const std::vector<eCaptureFlowStep> s_none;

	switch (kind)
	{
	case eCaptureKind::stillphoto:
		return s_stillPhoto;
	case eCaptureKind::stillcamera:
		return s_stillCamera;
	case eCaptureKind::trackcamera:
		return s_trackCamera;
	default:
		return s_none;
	}
}

bool CaptureFlow::isMovieStep() const
{
	return m_step == eCaptureFlowStep::readyToRecord || m_step == eCaptureFlowStep::recording;
}

bool CaptureFlow::start(eCaptureKind kind)
{
	if (isActive() || stepsForKind(kind).empty())
		return false;

	m_kind= kind;
	m_stepIndex= 0;
	m_step= stepsForKind(kind)[0];
	return true;
}

bool CaptureFlow::advance()
{
	if (!isActive() || m_step == eCaptureFlowStep::packing)
		return false;

	const std::vector<eCaptureFlowStep>& steps= stepsForKind(m_kind);
	++m_stepIndex;
	m_step= m_stepIndex < steps.size() ? steps[m_stepIndex] : eCaptureFlowStep::packing;
	return true;
}

void CaptureFlow::complete()
{
	m_kind= eCaptureKind::INVALID;
	m_step= eCaptureFlowStep::idle;
	m_stepIndex= 0;
}
