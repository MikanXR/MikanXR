#pragma once

#include "AssetReference.h"
#include "LocText.h"

// The camera pose sidecar a phone writes next to a recorded movie or photo
class PoseTrackAssetReference : public AssetReference
{
public:
	PoseTrackAssetReference()= default;

	inline static const std::string k_assetClassName= "PoseTrackAssetReference";
	virtual std::string getClassName() const override { return k_assetClassName; }
	virtual std::string getAssetTypeName() const override { return "PoseTrack"; }
	virtual const char* editorGetIcon() const override { return ICON_FK_LOCATION_ARROW; }
};

class PoseTrackAssetReferenceFactory : public TypedAssetReferenceFactory<PoseTrackAssetReference, AssetReferenceConfig>
{
public:
	PoseTrackAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "PoseTrack"; }
	virtual char const* getFileDialogTitle() const { return locText("assets.loadPoseTrackDialogTitle"); }
	virtual char const* const* getFilterPatterns() const { return getPoseTrackFilterPatterns(); }
	virtual int getFilterPatternCount() const { return getPoseTrackFilterPatternCount(); }
	virtual char const* getFilterDescription() const { return locText("assets.poseTrackFilterDescription"); }

	inline static const char* k_sidecarSuffix= ".pose.json";
	static char const* const* getPoseTrackFilterPatterns()
	{
		static const char* filterItems[1]= {"*.pose.json"};
		return filterItems;
	}
	static int getPoseTrackFilterPatternCount() { return 1; }
};
