#pragma once

#include "PluginOwnerAPI.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace hdt
{
	// @brief Process-wide list of mesh owners, asked in priority order whenever hdtSMP64 is about to create a
	// system for an armor or head part.
	class ExternalOwnerRegistry
	{
	public:
		static ExternalOwnerRegistry& instance();

		// Ignores null and already registered owners. Owners with a higher priority are asked first; equal
		// priorities keep registration order.
		void addOwner(IMeshOwner* owner, const MeshOwnerInfo& info = {});

		// The owner is not asked again. Its live claims stay valid and are released as usual.
		void removeOwner(IMeshOwner* owner);

		// The first owner in priority order that returns a claim object wins; null when none does. Destroying the
		// last copy of the returned pointer calls IMeshClaim::release.
		// @param ownerName Receives the winning owner's name, for logging. May be null.
		std::shared_ptr<IMeshClaim> tryClaim(const IMeshClaimContext& context, std::string* ownerName = nullptr);

		// Registered owner names in the order they are asked, for diagnostics and tests.
		std::vector<std::string> ownerNames() const;

	private:
		struct Entry
		{
			IMeshOwner* owner = nullptr;
			std::string name;
			std::int32_t priority = 0;
		};

		// Owner callbacks run outside m_mutex, on a copy of m_owners.
		mutable std::mutex m_mutex;
		std::vector<Entry> m_owners;
	};
}
