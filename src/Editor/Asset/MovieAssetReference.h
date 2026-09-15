#pragma once

#include "AssetReference.h"
#include "LocText.h"

#include <vector>

// A recorded movie file a file video source plays
class MovieAssetReference : public AssetReference
{
public:
	MovieAssetReference()= default;

	inline static const std::string k_assetClassName= "MovieAssetReference";
	virtual std::string getClassName() const override { return k_assetClassName; }
	virtual std::string getAssetTypeName() const override { return "Movie"; }
	virtual const char* editorGetIcon() const override { return ICON_FK_FILM; }

protected:
	// The first decoded frame
	virtual void rebuildPreview() override;
};

class MovieAssetReferenceFactory : public TypedAssetReferenceFactory<MovieAssetReference, AssetReferenceConfig>
{
public:
	MovieAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "Movie"; }
	virtual char const* getFileDialogTitle() const { return locText("assets.loadMovieDialogTitle"); }
	virtual char const* const* getFilterPatterns() const { return getMovieFilterPatterns(); }
	virtual int getFilterPatternCount() const { return getMovieFilterPatternCount(); }
	virtual char const* getFilterDescription() const { return locText("assets.movieFilterDescription"); }

	virtual bool editorCanCreate() const { return true; }

	static std::string getDefaultMoviePath();
	static char const* const* getMovieFilterPatterns()
	{
		static const char* filterItems[5]= {"*.mp4", "*.mov", "*.m4v", "*.mkv", "*.avi"};
		return filterItems;
	}
	static int getMovieFilterPatternCount() { return 5; }
};

// Row metadata for a property that accepts either a movie or a still image,
// since a file video source plays both through one path. It only ever filters
// drops, so the movie reference stands in as the prototype.
class MediaAssetReferenceFactory : public TypedAssetReferenceFactory<MovieAssetReference, AssetReferenceConfig>
{
public:
	MediaAssetReferenceFactory();

	virtual std::string getAssetTypeName() const { return "Media"; }
	virtual char const* getFileDialogTitle() const { return locText("assets.loadMediaDialogTitle"); }
	virtual char const* const* getFilterPatterns() const { return m_filterPatterns.data(); }
	virtual int getFilterPatternCount() const { return (int)m_filterPatterns.size(); }
	virtual char const* getFilterDescription() const { return locText("assets.mediaFilterDescription"); }

private:
	std::vector<char const*> m_filterPatterns;
};
