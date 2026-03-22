#include "config.h"
#include "Hooks.h"
#include "XmlReader.h"
#include "hdtSkyrimPhysicsWorld.h"

#ifdef CUDA
#	include "hdtSkinnedMesh/hdtCudaInterface.h"
#endif

namespace hdt
{
	int g_logLevel;

	static void solver(const pugi::xml_node& solverNode)
	{
		for (auto node : solverNode.children()) {
			std::string name = node.name();
			if (name == "numIterations") {
				SkyrimPhysicsWorld::get()->getSolverInfo().m_numIterations = btClamped(XMLReader::readInt(node), 4, 128);
			}
			// This has been dead code for years. Todo: Remove references to this in all the configs/menus.
			//else if (name == "groupIterations") {
			//	ConstraintGroup::MaxIterations = btClamped(XMLReader::readInt(node), 0, 4096);
			//} else if (name == "groupEnableMLCP") {
			//	ConstraintGroup::EnableMLCP = XMLReader::readBool(node);
			//}
			else if (name == "erp") {
				SkyrimPhysicsWorld::get()->getSolverInfo().m_erp = btClamped(XMLReader::readFloat(node), 0.01f, 1.0f);
			} else if (name == "min-fps") {
				SkyrimPhysicsWorld::get()->min_fps = (btClamped(XMLReader::readInt(node), 1, 300));
				SkyrimPhysicsWorld::get()->m_timeTick = 1.0f / SkyrimPhysicsWorld::get()->min_fps;
			} else if (name == "maxSubSteps") {
				SkyrimPhysicsWorld::get()->m_maxSubSteps = btClamped(XMLReader::readInt(node), 1, 60);
			} else {
				logger::warn("Unknown config : {}", name);
			}
		}
	}

	static void wind(const pugi::xml_node& windNode)
	{
		for (auto node : windNode.children()) {
			std::string name = node.name();
			if (name == "windStrength") {
				SkyrimPhysicsWorld::get()->m_windStrength = btClamped(XMLReader::readFloat(node), 0.f, 1000.f);
			} else if (name == "enabled") {
				SkyrimPhysicsWorld::get()->m_enableWind = XMLReader::readBool(node);
			} else if (name == "distanceForNoWind") {
				SkyrimPhysicsWorld::get()->m_distanceForNoWind = btClamped(XMLReader::readFloat(node), 0.f, 10000.f);
			} else if (name == "distanceForMaxWind") {
				SkyrimPhysicsWorld::get()->m_distanceForMaxWind = btClamped(XMLReader::readFloat(node), 0.f, 10000.f);
			} else {
				logger::warn("Unknown config : {}", name);
			}
		}
	}

	static void smp(const pugi::xml_node& smpNode)
	{
		for (auto node : smpNode.children()) {
			std::string name = node.name();
			if (name == "logLevel") {
				g_logLevel = XMLReader::readInt(node);
			} else if (name == "backupNodeByName") {
				// Parse the string return value from reader.readText(); so we can have single strings instead of the group, example text -> "Virtual Hands, Virtual Body, Virtual Belly"... said text in a array like so -> { "Virtual Hands", "Virtual Body", "Virtual Belly"

				std::stringstream ss(XMLReader::readText(node));
				std::string item;

				while (std::getline(ss, item, ',')) {
					// Remove leading space
					if (!item.empty() && item[0] == ' ') {
						item.erase(0, 1);
					}

					Hooks::BipedAnimHooks::BackupNodes.push_back(item);
				}
			} else if (name == "enableNPCFaceParts") {
				ActorManager::instance()->m_skinNPCFaceParts = XMLReader::readBool(node);
			} else if (name == "disableSMPHairWhenWigEquipped") {
				ActorManager::instance()->m_disableSMPHairWhenWigEquipped = XMLReader::readBool(node);
			} else if (name == "clampRotations") {
				SkyrimPhysicsWorld::get()->m_clampRotations = XMLReader::readBool(node);
			} else if (name == "rotationSpeedLimit") {
				SkyrimPhysicsWorld::get()->m_rotationSpeedLimit = XMLReader::readFloat(node);
			} else if (name == "unclampedResets") {
				SkyrimPhysicsWorld::get()->m_unclampedResets = XMLReader::readBool(node);
			} else if (name == "unclampedResetAngle") {
				SkyrimPhysicsWorld::get()->m_unclampedResetAngle = XMLReader::readFloat(node);
			} else if (name == "percentageOfFrameTime") {
				SkyrimPhysicsWorld::get()->m_percentageOfFrameTime = std::clamp(XMLReader::readInt(node) * 10, 1, 1000);
			} else if (name == "useRealTime") {
				SkyrimPhysicsWorld::get()->m_useRealTime = XMLReader::readBool(node);
			}
#ifdef CUDA
			else if (name == "enableCuda") {
				CudaInterface::enableCuda = XMLReader::readBool(node);
			} else if (name == "cudaDevice") {
				int device = XMLReader::readInt(node);
				if (device >= 0 && device < CudaInterface::instance()->deviceCount()) {
					CudaInterface::currentDevice = device;
				}
			}
#else
			else if (name == "enableCuda") {
				if (XMLReader::readBool(node)) {
					logger::warn("CUDA isn't built into this version.");
				}
			} else if (name == "cudaDevice") {
			}
#endif
			else if (name == "minCullingDistance") {
				ActorManager::instance()->m_minCullingDistance = XMLReader::readFloat(node);
			} else if (name == "maximumActiveSkeletons") {
				ActorManager::instance()->m_maxActiveSkeletons = XMLReader::readInt(node);
			} else if (name == "autoAdjustMaxSkeletons") {
				ActorManager::instance()->m_autoAdjustMaxSkeletons = XMLReader::readBool(node);
			} else if (name == "sampleSize") {
				SkyrimPhysicsWorld::get()->m_sampleSize = std::max(XMLReader::readInt(node), 1);
			}

			else if (name == "disable1stPersonViewPhysics") {
				ActorManager::instance()->m_disable1stPersonViewPhysics = XMLReader::readBool(node);
			} else {
				logger::warn("Unknown config : {}", name);
			}
		}
	}

	static void config(const pugi::xml_node& configNode)
	{
		for (auto node : configNode.children()) {
			std::string name = node.name();
			if (name == "solver") {
				solver(node);
			} else if (name == "wind") {
				wind(node);
			} else if (name == "smp") {
				smp(node);
			} else {
				logger::warn("Unknown config : {}", name);
			}
		}
	}

	void loadConfig()
	{
		auto bytes = readAllFile2("data/skse/plugins/hdtSkinnedMeshConfigs/configs.xml");
		if (bytes.empty()) {
			return;
		}

		// Store original locale
		char saved_locale[32];
		strcpy_s(saved_locale, std::setlocale(LC_NUMERIC, nullptr));

		// Set locale to en_US
		std::setlocale(LC_NUMERIC, "en_US");

		try {
			XMLReader reader((uint8_t*)bytes.data(), bytes.size());
			auto root = reader.root();
			if (std::string(root.name()) != "configs")
				return;

			config(root);
		} catch (const std::string& err) {
			logger::error("xml parse error - {}", err.c_str());
		}

		// Restore original locale
		std::setlocale(LC_NUMERIC, saved_locale);
	}
}
