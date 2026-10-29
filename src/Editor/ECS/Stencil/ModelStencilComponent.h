#pragma once

#include "AssetFwd.h"
#include "ColliderQuery.h"
#include "ComponentFwd.h"
#include "ISelectionOutlineSource.h"
#include "MikanStencilTypes.h"
#include "MikanVideoSourceTypes.h"
#include "MikanRendererFwd.h"
#include "StencilComponent.h"
#include "Transform.h"

#include <memory>
#include <string>
#include <vector>

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/quaternion_float.hpp"

class ModelStencilDefinition : public StencilComponentDefinition
{
public:
	ModelStencilDefinition();
	ModelStencilDefinition(MikanStencilID stencilId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_modelStencilObjPathPropertyId;
	// Read-only and derived from the loaded model, so the component notifies it once a model_path
	// change has reloaded the model
	static const std::string k_geometryUrlPropertyId;
	bool hasModelPath() const;
	const std::filesystem::path getModelPath() const;
	void setModelPath(const std::filesystem::path& path, bool bForceDirty= false);

	// The recorded take a stencil was fused from, by its video source and the
	// session whose world the mesh lives in. While that source stays aligned for
	// that session, a new alignment re-places the stencil. Definition only, not a
	// reflected property: placement is an editor concern and a descriptor would be
	// a wire change.
	static const std::string k_alignedVideoSourceIdPropertyId;
	static const std::string k_alignedSessionIdPropertyId;
	inline MikanVideoSourceID getAlignedVideoSourceId() const { return m_alignedVideoSourceId; }
	inline const std::string& getAlignedSessionId() const { return m_alignedSessionId; }
	void setAlignedTake(MikanVideoSourceID videoSourceId, const std::string& sessionId);

private:
	AssetReferenceConfigPtr m_modelAssetRefConfig;
	MikanVideoSourceID m_alignedVideoSourceId= INVALID_MIKAN_ID;
	std::string m_alignedSessionId;
};

class ModelStencilComponent : public StencilComponent, public ISelectionOutlineSource
{
public:
	ModelStencilComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "ModelStencilComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline ModelStencilDefinitionPtr getModelStencilDefinition() const
	{
		return std::static_pointer_cast<ModelStencilDefinition>(m_definition);
	}
	inline const std::vector<StaticMeshComponentPtr>& getTriangulatedMeshes() const { return m_triMeshComponents; }
	inline MikanRenderModelResourcePtr getRenderModelResource() const { return m_modelResource; }
	inline const std::vector<IMkStaticMeshInstancePtr>& getWireframeMeshes() const { return m_wireframeMeshes; }
	inline const std::vector<MeshColliderComponentPtr>& getColliderComponents() const { return m_colliderComponents; }

	void setModelPath(const std::filesystem::path& path);
	void disposeMeshComponents();
	void rebuildMeshComponents();

	// Selection Events
	void onTransformGizmoBound();
	void onTransformGizmoUnbound();

	// -- ISelectionOutlineSource ----
	virtual void getSelectionOutlineRenderables(std::vector<IMkSceneRenderableConstPtr>& outRenderables) const override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_alignStencilFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	void alignStencil();

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	void onStencilDefinitionMarkedDirty(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

protected:
	AssetReferencePtr m_modelAssetRef;
	MikanRenderModelResourcePtr m_modelResource;
	SelectionComponentWeakPtr m_selectionComponentWeakPtr;
	std::vector<IMkStaticMeshInstancePtr> m_wireframeMeshes;
	std::vector<TransformComponentPtr> m_meshComponents;
	std::vector<StaticMeshComponentPtr> m_triMeshComponents;
	std::vector<MeshColliderComponentPtr> m_colliderComponents;
	bool m_bIsTransformGizmoBound= false;
};
