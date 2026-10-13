#pragma once

#include "AssetReference.h"
#include "LocText.h"

// One of the two binary depth sidecars a phone writes next to a recorded movie:
// <stem>.depth.bin (uint16 depth records) or <stem>.conf.bin (uint8 confidence
// records). Their layout is declared by the pose track beside them.
class DepthTrackAssetReference : public AssetReference
{
public:
	DepthTrackAssetReference()= default;

	inline static const std::string k_assetClassName= "DepthTrackAssetReference";
	virtual std::string getClassName() const override { return k_assetClassName; }
	virtual std::string getAssetTypeName() const override { return "DepthTrack"; }
	virtual const char* editorGetIcon() const override { return ICON_FK_CUBE; }
};

class DepthTrackAssetReferenceFactory
	: public TypedAssetReferenceFactory<DepthTrackAssetReference, AssetReferenceConfig>
{
public:
	DepthTrackAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "DepthTrack"; }
	virtual char const* getFileDialogTitle() const { return locText("assets.loadDepthTrackDialogTitle"); }
	virtual char const* const* getFilterPatterns() const { return getDepthTrackFilterPatterns(); }
	virtual int getFilterPatternCount() const { return getDepthTrackFilterPatternCount(); }
	virtual char const* getFilterDescription() const { return locText("assets.depthTrackFilterDescription"); }

	inline static const char* k_depthSidecarSuffix= ".depth.bin";
	inline static const char* k_confidenceSidecarSuffix= ".conf.bin";
	static char const* const* getDepthTrackFilterPatterns()
	{
		static const char* filterItems[2]= {"*.depth.bin", "*.conf.bin"};
		return filterItems;
	}
	static int getDepthTrackFilterPatternCount() { return 2; }
};
