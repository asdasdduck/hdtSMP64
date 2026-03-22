#include "XmlReader.h"

namespace hdt
{
	static float convertFloat(const std::string& str)
	{
		std::string s = str;
		size_t pos = s.find(',');
		if (pos != std::string::npos)
			s.replace(pos, 1, ".");

		errno = 0;
		float ret = strtof(s.c_str(), nullptr);
		if (errno != 0)
			throw std::string("not a float value");
		return ret;
	}

	static int convertInt(const std::string& str)
	{
		char* end;

		int radix = 10;
		if (!str.compare(0, 2, "0x"))
			radix = 16;
		else if (str.length() > 1 && str[0] == '0')
			radix = 8;

		int ret = strtol(str.c_str(), &end, radix);
		if (end != str.c_str() + str.length())
			throw std::string("not a int value");
		return ret;
	}

	static bool convertBool(const std::string& str)
	{
		if (str == "true" || str == "1")
			return true;
		if (str == "false" || str == "0")
			return false;
		throw std::string("not a boolean");
	}

	XMLReader::XMLReader(BYTE* data, size_t count)
	{
		pugi::xml_parse_result result = m_doc.load_buffer(data, count);
		if (!result)
			throw std::string("XML parse error: ") + result.description();
	}

	pugi::xml_node XMLReader::root() const
	{
		return m_doc.document_element();
	}

	bool XMLReader::hasAttribute(const pugi::xml_node& node, const char* name)
	{
		return !node.attribute(name).empty();
	}

	std::string XMLReader::getAttribute(const pugi::xml_node& node, const char* name)
	{
		pugi::xml_attribute attr = node.attribute(name);
		if (attr.empty())
			throw std::string("missing attribute : ") + name;
		return attr.as_string();
	}

	std::string XMLReader::getAttribute(const pugi::xml_node& node, const char* name, const std::string& def)
	{
		pugi::xml_attribute attr = node.attribute(name);
		if (attr.empty())
			return def;
		return attr.as_string();
	}

	float XMLReader::getAttributeAsFloat(const pugi::xml_node& node, const char* name)
	{
		return convertFloat(getAttribute(node, name));
	}

	int XMLReader::getAttributeAsInt(const pugi::xml_node& node, const char* name)
	{
		return convertInt(getAttribute(node, name));
	}

	bool XMLReader::getAttributeAsBool(const pugi::xml_node& node, const char* name)
	{
		return convertBool(getAttribute(node, name));
	}

	std::string XMLReader::readText(const pugi::xml_node& node)
	{
		return node.text().as_string();
	}

	float XMLReader::readFloat(const pugi::xml_node& node)
	{
		return convertFloat(node.text().as_string());
	}

	int XMLReader::readInt(const pugi::xml_node& node)
	{
		return convertInt(node.text().as_string());
	}

	bool XMLReader::readBool(const pugi::xml_node& node)
	{
		return convertBool(node.text().as_string());
	}

	btVector3 XMLReader::readVector3(const pugi::xml_node& node)
	{
		float x = getAttributeAsFloat(node, "x");
		float y = getAttributeAsFloat(node, "y");
		float z = getAttributeAsFloat(node, "z");
		return btVector3(x, y, z);
	}

	btQuaternion XMLReader::readQuaternion(const pugi::xml_node& node)
	{
		float x = getAttributeAsFloat(node, "x");
		float y = getAttributeAsFloat(node, "y");
		float z = getAttributeAsFloat(node, "z");
		float w = getAttributeAsFloat(node, "w");
		btQuaternion q(x, y, z, w);
		if (btFuzzyZero(q.length2()))
			q = btQuaternion::getIdentity();
		else
			q.normalize();
		return q;
	}

	btQuaternion XMLReader::readAxisAngle(const pugi::xml_node& node)
	{
		float x = getAttributeAsFloat(node, "x");
		float y = getAttributeAsFloat(node, "y");
		float z = getAttributeAsFloat(node, "z");
		float w = getAttributeAsFloat(node, "angle");
		btQuaternion q;
		btVector3 axis(x, y, z);
		if (axis.fuzzyZero()) {
			axis.setX(1);
			w = 0;
		} else
			axis.normalize();
		q.setRotation(axis, w);
		return q;
	}

	btTransform XMLReader::readTransform(const pugi::xml_node& node)
	{
		btTransform ret(btTransform::getIdentity());
		for (auto child : node.children()) {
			std::string name = child.name();
			if (name == "basis")
				ret.setRotation(readQuaternion(child));
			else if (name == "basis-axis-angle")
				ret.setRotation(readAxisAngle(child));
			else if (name == "origin")
				ret.setOrigin(readVector3(child));
		}
		return ret;
	}
}
