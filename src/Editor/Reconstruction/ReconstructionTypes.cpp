#include "ReconstructionTypes.h"

PinholeIntrinsics scaleIntrinsics(const PinholeIntrinsics& intrinsics, int fromWidth, int fromHeight, int toWidth,
								  int toHeight)
{
	const double scaleX= (double)toWidth / (double)fromWidth;
	const double scaleY= (double)toHeight / (double)fromHeight;

	PinholeIntrinsics scaled;
	scaled.fx= intrinsics.fx * scaleX;
	scaled.fy= intrinsics.fy * scaleY;
	scaled.cx= intrinsics.cx * scaleX;
	scaled.cy= intrinsics.cy * scaleY;
	return scaled;
}
