#pragma once

#include "PluginOwnerAPI.h"

/*Plugins should register for messages from hdtSMP64 via SKSE during SKSE's PostLoad event.
* When ready, hdtSMP64 will send a message of type MSG_STARTUP containing a PluginInterface* as data.
*
* Plugin authors need both this header and PluginOwnerAPI.h.
*
* A plugin MUST verify compatibility with the interface version before calling any other functions.
* The interface version is semantic, meaning that
*	- any incompatible API change increments the MAJOR version
*	- any backwards-compatible new feature increments the MINOR version
*	- a PATCH update does not change anything in this API
*
* A plugin MUST verify compatibility with the Bullet version if it intends to interact with any Bullet object.
* Refer to Bullet documentation for information about their versioning scheme.
*
* If compatible, the plugin may call other functions on the PluginInterface at any time.
* The PluginInterface pointer shall remain valid until shutdown.
*/

template <typename T>
class btAlignedObjectArray;
class btCollisionObject;

namespace hdt
{
	//Sent right before the physics simulation begins updating.
	//Only forces and torques may be applied during this event.
	//The collision objects must in every other regard be treated as read-only.
	struct PreStepEvent
	{
		const btAlignedObjectArray<btCollisionObject*>& objects;
		float timeStep{ 0.0f };
	};

	//Sent right after the physics simulation has finished updating.
	//The collision objects must in every regard be treated as read-only.
	struct PostStepEvent
	{
		const btAlignedObjectArray<btCollisionObject*>& objects;
		float timeStep{ 0.0f };
	};

	using IPreStepListener = RE::BSTEventSink<PreStepEvent>;
	using IPostStepListener = RE::BSTEventSink<PostStepEvent>;

	class PluginInterface
	{
	public:
		enum MessageType : unsigned long
		{
			MSG_STARTUP,
		};

		struct Version
		{
			int major;
			int minor;
			int patch;
		};

		struct VersionInfo
		{
			Version interfaceVersion;
			Version bulletVersion;
		};

	public:
		constexpr static Version INTERFACE_VERSION{ 2, 1, 0 };
		constexpr static Version BULLET_VERSION{ 3, 24, 0 };

	public:
		virtual ~PluginInterface() = default;

		virtual const VersionInfo& getVersionInfo() const = 0;

		virtual void addListener(IPreStepListener*) = 0;
		virtual void removeListener(IPreStepListener*) = 0;

		virtual void addListener(IPostStepListener*) = 0;
		virtual void removeListener(IPostStepListener*) = 0;

		// @brief Registers an owner that is asked to take over simulation of specific meshes, ordered by
		// MeshOwnerInfo::priority and then by registration order. Ignores null and already registered owners.
		virtual void addMeshOwner(IMeshOwner*, const MeshOwnerInfo&) = 0;

		// @brief Unregisters an owner; it is not asked to claim again once this returns. Call it from the game main
		// thread, never from inside claim or release. Its live claims stay valid and are released as usual; to hand
		// their meshes back now, call reloadActorPhysics for their actors.
		virtual void removeMeshOwner(IMeshOwner*) = 0;

		// @brief Re-runs the soft reload for an actor, so every one of its meshes is released and offered to the mesh
		// owners again. An owner hands a mesh back by declining it in claim before calling this.
		// Call it from the game main thread.
		virtual void reloadActorPhysics(RE::Actor*) = 0;
	};
}
