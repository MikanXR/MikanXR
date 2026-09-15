#include "PlaybackTime.h"

#include <cmath>

void advancePlaybackTime(float& inoutTime, float delta, float duration, bool bLoop, bool& outWrapped, bool& outFinished)
{
	outWrapped= false;
	outFinished= false;

	inoutTime+= delta;
	if (duration <= 0.f || inoutTime < duration)
		return;

	if (bLoop)
	{
		inoutTime= std::fmod(inoutTime, duration);
		outWrapped= true;
	}
	else
	{
		inoutTime= duration;
		outFinished= true;
	}
}
