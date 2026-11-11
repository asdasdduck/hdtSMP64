#include "PluginInterfaceImpl.h"
#include "ActorManager.h"
#include "ExternalOwner.h"
#include "dhdtPapyrusFunctions.h"

hdt::PluginInterfaceImpl hdt::g_pluginInterface;

void hdt::PluginInterfaceImpl::addListener(IPreStepListener* l)
{
	if (l) {
		m_preStepDispatcher.AddEventSink(l);
	}
}

void hdt::PluginInterfaceImpl::removeListener(IPreStepListener* l)
{
	if (l) {
		m_preStepDispatcher.RemoveEventSink(l);
	}
}

void hdt::PluginInterfaceImpl::addListener(IPostStepListener* l)
{
	if (l) {
		m_postStepDispatcher.AddEventSink(l);
	}
}

void hdt::PluginInterfaceImpl::removeListener(IPostStepListener* l)
{
	if (l) {
		m_postStepDispatcher.RemoveEventSink(l);
	}
}

void hdt::PluginInterfaceImpl::addMeshOwner(IMeshOwner* owner, const MeshOwnerInfo& info)
{
	ExternalOwnerRegistry::instance().addOwner(owner, info);
}

void hdt::PluginInterfaceImpl::removeMeshOwner(IMeshOwner* owner)
{
	// Waits out a claim or release the Papyrus VM thread is running on this owner, so the plugin can unload it safely.
	const auto lock = ActorManager::instance()->lockGuard();
	for (const std::uint32_t formId : ExternalOwnerRegistry::instance().removeOwner(owner)) {
		// The actor's meshes lost their claim without a system; rebuild them so hdtSMP64 or another owner takes over.
		SKSE::GetTaskInterface()->AddTask([formId] {
			if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formId)) {
				papyrus::impl::ResetPhysicsImpl(actor, false);
			}
		});
	}
}

void hdt::PluginInterfaceImpl::relinquishMesh(IMeshOwner* owner, std::uint64_t claimToken)
{
	const auto relinquished = ExternalOwnerRegistry::instance().relinquish(owner, claimToken);
	if (!relinquished || relinquished->actorFormId == 0) {
		return;
	}

	SKSE::GetTaskInterface()->AddTask([formId = relinquished->actorFormId] {
		auto& registry = ExternalOwnerRegistry::instance();
		// An earlier task for this actor may already have rebuilt it with every waiting skip.
		if (!registry.beginRelinquishedReload(formId)) {
			return;
		}

		// Ends the bracket even if the reload throws, or the owner stays skipped for this actor.
		struct Bracket
		{
			ExternalOwnerRegistry& registry;
			std::uint32_t formId;
			~Bracket() { registry.endRelinquishedReload(formId); }
		} bracket{ registry, formId };

		if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formId)) {
			papyrus::impl::ResetPhysicsImpl(actor, false);
		}
	});
}

void hdt::PluginInterfaceImpl::reloadActorPhysics(RE::Actor* actor)
{
	if (!actor) {
		return;
	}

	SKSE::GetTaskInterface()->AddTask([handle = RE::ActorHandle(actor)] {
		if (auto a = handle.get()) {
			papyrus::impl::ResetPhysicsImpl(a.get(), false);
		}
	});
}

void hdt::PluginInterfaceImpl::onPostPostLoad()
{
	// Send ourselves to any plugin that registered during the PostLoad event
	if (m_skseMessagingInterface) {
		m_skseMessagingInterface->Dispatch(PluginInterface::MSG_STARTUP, static_cast<PluginInterface*>(this), 0, nullptr);
	}
}

void hdt::PluginInterfaceImpl::init(const SKSE::LoadInterface* skse)
{
	//We need to have our SKSE plugin handle and the messaging interface in order to reach our plugins later
	if (skse) {
		m_sksePluginHandle = skse->GetPluginHandle();
		m_skseMessagingInterface = reinterpret_cast<SKSE::MessagingInterface*>(skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	}
	if (!m_skseMessagingInterface) {
		logger::warn("Failed to get a messaging interface. Plugins will not work.");
	}
}
