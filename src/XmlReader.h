#pragma once

#include <pugixml.hpp>

#include "hdtSkinnedMesh/hdtBulletHelper.h"

namespace hdt
{
	class XMLReader
	{
		pugi::xml_document m_doc;

	public:
		XMLReader(BYTE* data, size_t count);

		pugi::xml_node root() const;

		static bool hasAttribute(const pugi::xml_node& node, const char* name);
		static std::string getAttribute(const pugi::xml_node& node, const char* name);
		static std::string getAttribute(const pugi::xml_node& node, const char* name, const std::string& def);
		static float getAttributeAsFloat(const pugi::xml_node& node, const char* name);
		static int getAttributeAsInt(const pugi::xml_node& node, const char* name);
		static bool getAttributeAsBool(const pugi::xml_node& node, const char* name);

		static std::string readText(const pugi::xml_node& node);
		static float readFloat(const pugi::xml_node& node);
		static int readInt(const pugi::xml_node& node);
		static bool readBool(const pugi::xml_node& node);

		static btVector3 readVector3(const pugi::xml_node& node);
		static btQuaternion readQuaternion(const pugi::xml_node& node);
		static btQuaternion readAxisAngle(const pugi::xml_node& node);
		static btTransform readTransform(const pugi::xml_node& node);
	};
}
