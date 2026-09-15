#pragma once

#include <memory>
#include <vector>

class AssetReference;
using AssetReferencePtr= std::shared_ptr<AssetReference>;
using AssetReferenceConstPtr= std::shared_ptr<const AssetReference>;

class AssetReferenceConfig;
using AssetReferenceConfigPtr= std::shared_ptr<AssetReferenceConfig>;
using AssetReferenceConfigConstPtr= std::shared_ptr<const AssetReferenceConfig>;

class AssetReferenceFactory;
using AssetReferenceFactoryPtr= std::shared_ptr<AssetReferenceFactory>;
using AssetReferenceFactoryConstPtr= std::shared_ptr<const AssetReferenceFactory>;

class CompositorGraphAssetReference;
using CompositorGraphAssetReferencePtr= std::shared_ptr<CompositorGraphAssetReference>;
using CompositorGraphAssetReferenceConstPtr= std::shared_ptr<const CompositorGraphAssetReference>;

class ShapeGraphAssetReference;
using ShapeGraphAssetReferencePtr= std::shared_ptr<ShapeGraphAssetReference>;
using ShapeGraphAssetReferenceConstPtr= std::shared_ptr<const ShapeGraphAssetReference>;

class MaterialAssetReference;
using MaterialAssetReferencePtr= std::shared_ptr<MaterialAssetReference>;
using MaterialAssetReferenceConstPtr= std::shared_ptr<const MaterialAssetReference>;

class CompositorMaterialAssetReference;
using CompositorMaterialAssetReferencePtr= std::shared_ptr<CompositorMaterialAssetReference>;

class ShapeMaterialAssetReference;
using ShapeMaterialAssetReferencePtr= std::shared_ptr<ShapeMaterialAssetReference>;

class ModelAssetReference;
using ModelAssetReferencePtr= std::shared_ptr<ModelAssetReference>;
using ModelAssetReferenceConstPtr= std::shared_ptr<const ModelAssetReference>;

class ScriptAssetReference;
using ScriptAssetReferencePtr= std::shared_ptr<ScriptAssetReference>;
using ScriptAssetReferenceConstPtr= std::shared_ptr<const ScriptAssetReference>;

class TextureAssetReference;
using TextureAssetReferencePtr= std::shared_ptr<TextureAssetReference>;
using TextureAssetReferenceConstPtr= std::shared_ptr<const TextureAssetReference>;

class MovieAssetReference;
using MovieAssetReferencePtr= std::shared_ptr<MovieAssetReference>;
using MovieAssetReferenceConstPtr= std::shared_ptr<const MovieAssetReference>;

class PoseTrackAssetReference;
using PoseTrackAssetReferencePtr= std::shared_ptr<PoseTrackAssetReference>;
using PoseTrackAssetReferenceConstPtr= std::shared_ptr<const PoseTrackAssetReference>;