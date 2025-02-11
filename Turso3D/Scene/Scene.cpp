#include <Turso3D/Scene/Scene.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/IO/Stream.h>
#include <Turso3D/Scene/SpatialNode.h>

namespace Turso3D
{
	Scene::Scene(WorkQueue* workQueue) :
		octree(workQueue)
	{
		root.SetScene(this);
	}

	void Scene::Clear()
	{
		root.DestroyAllChildren();
	}
}
