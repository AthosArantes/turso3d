#pragma once

#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Renderer/StaticModel.h>
#include <Turso3D/Scene/SpatialNode.h>
#include <vector>
#include <memory>

namespace Turso3D
{
	class AnimatedModel;
	class AnimatedModelDrawable;
	class Animation;
	class AnimationState;
	class Bone;
	class UniformBuffer;
	struct ModelBone;

	// Animated model drawable.
	class AnimatedModelDrawable : public StaticModelDrawable
	{
		friend class AnimatedModel;

	public:
		enum Flag
		{
			FLAG_ANIMATION_ORDER_DIRTY = 0x1,
			FLAG_ANIMATION_DIRTY = 0x2,
			FLAG_SKINNING_DIRTY = 0x4,
			FLAG_SKINNING_BUFFER_DIRTY = 0x8,
			FLAG_BONE_BOUNDING_BOX_DIRTY = 0x10,
			FLAG_IN_ANIMATION_UPDATE = 0x20
		};

	public:
		// Construct.
		AnimatedModelDrawable();

		// Recalculate the world space bounding box.
		void OnWorldBoundingBoxUpdate() const override;
		// Do animation processing before octree reinsertion, if should update without regard to visibility.
		// Called by Octree in worker threads. Must be opted-in by setting NF_OCTREE_UPDATE_CALL flag.
		void OnOctreeUpdate(unsigned short frameNumber) override;
		// Prepare object for rendering.
		// Reset framenumber and calculate distance from camera, check for LOD level changes, and update animation / skinning if necessary.
		// Called by Renderer in worker threads.
		// Return false if should not render.
		bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
		// Update GPU resources and set uniforms for rendering.
		// Called by Renderer when geometry type is not static.
		void OnRender(ShaderProgram* program, size_t geomIndex) override;
		// Perform ray test on self and add possible hit to the result vector.
		void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;
		// Add debug geometry to be rendered.
		void OnRenderDebug(DebugRenderer* debug) override;

		// Set bounding box and skinning dirty and queue octree reinsertion when any of the bones move.
		void OnBoneTransformChanged();
		// Set animation order dirty when animation state changes layer order and queue octree reinsertion.
		// Note: bounding box will only be dirtied once animation actually updates.
		void OnAnimationOrderChanged();
		// Set animation dirty when animation state changes time position or weight and queue octree reinsertion.
		// Note: bounding box will only be dirtied once animation actually updates.
		void OnAnimationChanged();

		// Create bone scene nodes based on the model.
		// If compatible bones already exist in the scene hierarchy, they are taken into use instead of creating new.
		void CreateBones();
		// Remove existing bones.
		void RemoveBones();

		// Apply animation states and recalculate bounding box.
		void UpdateAnimation();
		// Update skin matrices for rendering.
		void UpdateSkinning();

		// Mark bone transforms dirty.
		// Do in an optimized manner if bone has no attached objects.
		void SetBoneTransformsDirty();

		// Set root bone.
		void SetRootBone(Bone* bone);
		// Return the root bone.
		Bone* RootBone() const { return rootBone; }

		// Return the internal dirty status flags.
		unsigned AnimatedModelFlags() const { return animatedModelFlags; }

	protected:
		// Combined bounding box of the bones in model space, used for quick updates when only the node moves without animation
		mutable BoundingBox boneBoundingBox;
		// Internal dirty status flags.
		mutable unsigned animatedModelFlags;

		// Root bone node reference.
		Bone* rootBone;
		// Model bone references in the node hierarchy
		std::vector<Bone*> bones;

		// Skinning matrices.
		std::unique_ptr<Matrix3x4[]> skinMatrices;
		// Skinning uniform buffer.
		std::unique_ptr<UniformBuffer> skinMatrixBuffer;
		// Animation states.
		std::vector<std::shared_ptr<AnimationState>> animationStates;
	};

	// ==========================================================================================
	// Bone scene node for AnimatedModel skinning.
	class Bone : public SpatialNode
	{
		friend class AnimatedModelDrawable;

	public:
		// Construct.
		Bone();
		// Destruct.
		~Bone();

		// Set the animated model drawable associated with.
		// When the bone moves, the model's skinning is dirtied.
		void SetDrawable(AnimatedModelDrawable* drawable);
		// Set animation enabled.
		// Default is enabled, when disabled the bone can be programmatically controlled.
		void SetAnimationEnabled(bool enable);
		// Count number of child bones.
		// Called by AnimatedModel once the skeleton has been fully created.
		void CountChildBones();

		// Set bone parent space transform without dirtying the hierarchy.
		void SetTransformSilent(const Vector3& position, const Quaternion& rotation, const Vector3& scale);

		// Return the animated model drawable.
		AnimatedModelDrawable* GetDrawable() const { return drawable; }
		// Return whether animation is enabled.
		bool AnimationEnabled() const { return animationEnabled; }

		// Return amount of child bones.
		// This is used to check whether bone has attached objects and its dirtying cannot be handled in an optimized way.
		size_t NumChildBones() const { return numChildBones; }

	protected:
		// Handle the transform matrix changing.
		void OnTransformChanged() override;

	private:
		// Animated model drawable associated with.
		AnimatedModelDrawable* drawable;
		// Animation enabled flag.
		bool animationEnabled;
		// Amount of child bones.
		size_t numChildBones;
	};

	// Scene node that renders a skeletally animated (skinned) model.
	class AnimatedModel : public StaticModel
	{
	public:
		// Construct.
		AnimatedModel();
		// Destruct.
		~AnimatedModel();

		// Return derived drawable.
		AnimatedModelDrawable* GetDrawable() const { return static_cast<AnimatedModelDrawable*>(drawable); }

		// Set the model resource and create / acquire bone scene nodes.
		void SetModel(std::shared_ptr<Model> model);

		// Add an animation and return the created animation state.
		AnimationState* AddAnimationState(const std::shared_ptr<Animation>& animation);
		// Remove an animation by animation pointer.
		void RemoveAnimationState(Animation* animation);
		// Remove an animation by animation name hash.
		void RemoveAnimationState(StringHash animationNameHash);
		// Remove an animation by AnimationState pointer.
		void RemoveAnimationState(AnimationState* state);
		// Remove an animation by state index.
		void RemoveAnimationState(size_t index);
		// Remove all animations.
		void RemoveAllAnimationStates();

		// Return the root bone.
		Bone* RootBone() const { return GetDrawable()->RootBone(); }

		// Return animation state by index.
		AnimationState* GetAnimationState(size_t index) const;
		// Return animation state by animation pointer.
		AnimationState* FindAnimationState(Animation* animation) const;
		// Return animation state by animation name hash.
		AnimationState* FindAnimationState(StringHash animationNameHash) const;

		// Return all animation states.
		const std::vector<std::shared_ptr<AnimationState>>& AnimationStates() const { return GetDrawable()->animationStates; }

	protected:
		// Search for an octree from the scene root and add self to it.
		void OnSceneSet(Scene* newScene, Scene* oldScene) override;
		// Handle the transform matrix changing.
		// Queue octree reinsertion and skinning update for the drawable.
		void OnTransformChanged() override;
	};
}
