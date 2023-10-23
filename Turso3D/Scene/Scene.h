#pragma once

#include <Turso3D/Renderer/LightEnvironment.h>
#include <Turso3D/Renderer/Octree.h>
#include <Turso3D/Scene/Node.h>

namespace Turso3D
{
	class Scene
	{
	public:
		Scene();

		// Destroy child nodes recursively, leaving the scene empty.
		void Clear();

		// Return the scene's root node.
		Node* GetRoot() { return &root; }
		// Return the scene's octree.
		Octree* GetOctree() { return &octree; }
		// Return environment lighting
		LightEnvironment* GetEnvironmentLighting() { return &lighting; }

	private:
		// The root node.
		Node root;
		// The octree used for rendering drawables.
		Octree octree;
		// The scene environment lighting
		LightEnvironment lighting;
	};
}
