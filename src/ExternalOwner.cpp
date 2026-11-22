#include "ExternalOwner.h"

#include <algorithm>

namespace hdt
{
	ExternalOwnerRegistry& ExternalOwnerRegistry::instance()
	{
		// Never destroyed: claims held by ActorManager can be released after a function-local static would be gone.
		static ExternalOwnerRegistry* const s_registry = new ExternalOwnerRegistry();
		return *s_registry;
	}

	void ExternalOwnerRegistry::addOwner(IMeshOwner* owner, const MeshOwnerInfo& info)
	{
		if (!owner) {
			return;
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		const auto known = std::find_if(m_owners.begin(), m_owners.end(), [owner](const Entry& e) { return e.owner == owner; });
		if (known != m_owners.end()) {
			return;
		}

		Entry entry{ owner, info.name && *info.name ? info.name : "unnamed", info.priority };
		const auto position = std::find_if(m_owners.begin(), m_owners.end(), [&entry](const Entry& e) { return e.priority < entry.priority; });
		m_owners.insert(position, std::move(entry));
	}

	void ExternalOwnerRegistry::removeOwner(IMeshOwner* owner)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_owners.erase(std::remove_if(m_owners.begin(), m_owners.end(), [owner](const Entry& e) { return e.owner == owner; }), m_owners.end());
	}

	std::shared_ptr<IMeshClaim> ExternalOwnerRegistry::tryClaim(const IMeshClaimContext& context, std::string* ownerName)
	{
		std::vector<Entry> owners;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_owners.empty()) {
				return nullptr;
			}

			owners = m_owners;
		}

		for (const Entry& entry : owners) {
			// A throwing owner counts as declining: the exception must not unwind through the game's event handlers.
			IMeshClaim* claim = nullptr;
			try {
				claim = entry.owner->claim(context);
			} catch (...) {
			}

			if (!claim) {
				continue;
			}

			if (ownerName) {
				*ownerName = entry.name;
			}

			return std::shared_ptr<IMeshClaim>(claim, [](IMeshClaim* c) {
				try {
					c->release();
				} catch (...) {
				}
			});
		}

		return nullptr;
	}

	std::vector<std::string> ExternalOwnerRegistry::ownerNames() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<std::string> names;
		names.reserve(m_owners.size());
		for (const Entry& entry : m_owners) {
			names.push_back(entry.name);
		}

		return names;
	}
}
