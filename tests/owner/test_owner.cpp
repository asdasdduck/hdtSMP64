#include <doctest/doctest.h>

#include "ExternalOwner.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
	class FakeContext : public hdt::IMeshClaimContext
	{
	public:
		hdt::MeshKind getKind() const override { return hdt::MeshKind::Armor; }
		const char* getPhysicsFile() const override { return "test.xml"; }
		RE::Actor* getActor() const override { return nullptr; }
		RE::NiNode* getSkeleton() const override { return nullptr; }
		RE::NiAVObject* getModel() const override { return nullptr; }
		const char* getMergedBoneName(const char*) const override { return nullptr; }
	};

	class FakeOwner;

	class FakeClaim final : public hdt::IMeshClaim
	{
	public:
		explicit FakeClaim(FakeOwner& a_owner) :
			owner(a_owner)
		{}

		void release() override;

		FakeOwner& owner;
	};

	class FakeOwner : public hdt::IMeshOwner
	{
	public:
		explicit FakeOwner(bool a_claims) :
			claims(a_claims)
		{}

		hdt::IMeshClaim* claim(const hdt::IMeshClaimContext&) override
		{
			++claimCalls;
			callOrder.push_back(this);

			if (throwOnClaim) {
				throw 1;
			}

			return claims ? new FakeClaim(*this) : nullptr;
		}

		bool claims = false;
		bool throwOnClaim = false;
		bool throwOnRelease = false;
		int claimCalls = 0;
		int releaseCalls = 0;

		static inline std::vector<FakeOwner*> callOrder;
	};

	void FakeClaim::release()
	{
		++owner.releaseCalls;
		const bool throws = owner.throwOnRelease;
		delete this;

		if (throws) {
			throw 1;
		}
	}

	// Unregisters on scope exit so a failing CHECK cannot leak a registration into the next case.
	class OwnerScope
	{
	public:
		OwnerScope() { FakeOwner::callOrder.clear(); }

		~OwnerScope()
		{
			auto& registry = hdt::ExternalOwnerRegistry::instance();
			for (const auto& owner : m_owners) {
				registry.removeOwner(owner.get());
			}
		}

		FakeOwner& add(bool a_claims, hdt::MeshOwnerInfo a_info = {})
		{
			m_owners.push_back(std::make_unique<FakeOwner>(a_claims));
			hdt::ExternalOwnerRegistry::instance().addOwner(m_owners.back().get(), a_info);
			return *m_owners.back();
		}

	private:
		std::vector<std::unique_ptr<FakeOwner>> m_owners;
	};
}

TEST_CASE("no owners registered gives no claim")
{
	OwnerScope scope;

	CHECK_FALSE(hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext()));
}

TEST_CASE("add ignores null and duplicate owners")
{
	OwnerScope scope;
	auto& registry = hdt::ExternalOwnerRegistry::instance();

	registry.addOwner(nullptr);
	CHECK(registry.ownerNames().empty());

	FakeOwner& owner = scope.add(true, { "gpu", 0 });
	registry.addOwner(&owner, { "again", 5 });
	CHECK(registry.ownerNames() == std::vector<std::string>{ "gpu" });

	CHECK(registry.tryClaim(FakeContext()));
	CHECK(owner.claimCalls == 1);
}

TEST_CASE("the first owner in registration order that claims wins")
{
	OwnerScope scope;
	FakeOwner& first = scope.add(true);
	FakeOwner& second = scope.add(true);

	CHECK(hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext()));
	CHECK(first.claimCalls == 1);
	CHECK(second.claimCalls == 0);
}

TEST_CASE("an owner that declines is skipped and the next one is asked")
{
	OwnerScope scope;
	FakeOwner& declining = scope.add(false);
	FakeOwner& accepting = scope.add(true);

	CHECK(hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext()));
	CHECK(declining.claimCalls == 1);
	CHECK(accepting.claimCalls == 1);
}

TEST_CASE("the winning owner's name is reported")
{
	OwnerScope scope;
	scope.add(false, { "declines", 0 });
	scope.add(true, { "gpu", 0 });

	std::string name;
	CHECK(hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext(), &name));
	CHECK(name == "gpu");
}

TEST_CASE("a claim is released exactly once, by its last copy")
{
	OwnerScope scope;
	FakeOwner& owner = scope.add(true);

	SUBCASE("on destruction")
	{
		{
			const auto claim = hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext());
			REQUIRE(claim);
			CHECK(owner.releaseCalls == 0);
		}

		CHECK(owner.releaseCalls == 1);
	}

	SUBCASE("on reset")
	{
		auto claim = hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext());
		REQUIRE(claim);

		claim.reset();
		CHECK(owner.releaseCalls == 1);

		claim.reset();
		CHECK(owner.releaseCalls == 1);
	}

	SUBCASE("only after every copy is gone")
	{
		auto claim = hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext());
		REQUIRE(claim);

		auto copy = claim;
		claim.reset();
		CHECK(owner.releaseCalls == 0);

		copy.reset();
		CHECK(owner.releaseCalls == 1);
	}

	SUBCASE("on assignment over a live claim")
	{
		auto first = hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext());
		auto second = hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext());
		REQUIRE(first);
		REQUIRE(second);

		first = std::move(second);
		CHECK(owner.releaseCalls == 1);

		first.reset();
		CHECK(owner.releaseCalls == 2);
	}
}

TEST_CASE("a higher priority owner is asked before earlier registered ones")
{
	OwnerScope scope;
	FakeOwner& low = scope.add(false, { "low", 0 });
	FakeOwner& high = scope.add(false, { "high", 10 });

	CHECK_FALSE(hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext()));
	CHECK(FakeOwner::callOrder == std::vector<FakeOwner*>{ &high, &low });
	CHECK(hdt::ExternalOwnerRegistry::instance().ownerNames() == std::vector<std::string>{ "high", "low" });
}

TEST_CASE("equal priorities keep registration order and an empty name becomes unnamed")
{
	OwnerScope scope;
	scope.add(false, { "first", 3 });
	scope.add(false, { "", 3 });
	scope.add(false, { nullptr, 3 });

	CHECK(hdt::ExternalOwnerRegistry::instance().ownerNames() == std::vector<std::string>{ "first", "unnamed", "unnamed" });
}

TEST_CASE("an owner that throws from claim is treated as declining")
{
	OwnerScope scope;
	FakeOwner& throwing = scope.add(true);
	throwing.throwOnClaim = true;
	FakeOwner& accepting = scope.add(true);

	CHECK(hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext()));
	CHECK(throwing.claimCalls == 1);
	CHECK(accepting.claimCalls == 1);
}

TEST_CASE("an exception from release does not escape")
{
	OwnerScope scope;
	FakeOwner& owner = scope.add(true);
	owner.throwOnRelease = true;

	auto claim = hdt::ExternalOwnerRegistry::instance().tryClaim(FakeContext());
	REQUIRE(claim);

	CHECK_NOTHROW(claim.reset());
	CHECK(owner.releaseCalls == 1);
}

TEST_CASE("a removed owner is not asked again but its live claim is still released")
{
	OwnerScope scope;
	auto& registry = hdt::ExternalOwnerRegistry::instance();
	FakeOwner& owner = scope.add(true, { "gpu", 0 });

	auto claim = registry.tryClaim(FakeContext());
	REQUIRE(claim);

	registry.removeOwner(&owner);
	CHECK(registry.ownerNames().empty());
	CHECK_FALSE(registry.tryClaim(FakeContext()));
	CHECK(owner.claimCalls == 1);

	claim.reset();
	CHECK(owner.releaseCalls == 1);
}
