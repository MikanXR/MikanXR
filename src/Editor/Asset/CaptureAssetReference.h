#pragma once

#include "AssetReference.h"
#include "LocText.h"

// A capture archive a file video source plays: one shot's media, marker
// reference, and tracks in a single file, named by kind (.stillphoto,
// .stillcamera, .trackcamera)
class CaptureAssetReference : public AssetReference
{
public:
	CaptureAssetReference()= default;

	inline static const std::string k_assetClassName= "CaptureAssetReference";
	virtual std::string getClassName() const override { return k_assetClassName; }
	virtual std::string getAssetTypeName() const override { return "Capture"; }
	virtual const char* editorGetIcon() const override { return ICON_FK_ARCHIVE; }

protected:
	// The plate image, else the movie's first frame, else the marker image
	virtual void rebuildPreview() override;
};

class CaptureAssetReferenceFactory : public TypedAssetReferenceFactory<CaptureAssetReference, AssetReferenceConfig>
{
public:
	CaptureAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "Capture"; }
	virtual char const* getFileDialogTitle() const { return locText("assets.loadCaptureDialogTitle"); }
	virtual char const* const* getFilterPatterns() const { return getCaptureFilterPatterns(); }
	virtual int getFilterPatternCount() const { return getCaptureFilterPatternCount(); }
	virtual char const* getFilterDescription() const { return locText("assets.captureFilterDescription"); }

	static std::string getDefaultCapturePath();
	static char const* const* getCaptureFilterPatterns()
	{
		static const char* filterItems[3]= {"*.stillphoto", "*.stillcamera", "*.trackcamera"};
		return filterItems;
	}
	static int getCaptureFilterPatternCount() { return 3; }
};
