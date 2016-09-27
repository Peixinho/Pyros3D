//============================================================================
// Name        : StringID.cpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : StringID
//============================================================================

#include <map>
#include <iostream>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Ext/StringIDs/CRC32.hpp>
namespace p3d
{
	std::map<StringID, std::string> StringIDMap;

	StringID MakeStringID(const std::string &Name)
	{
		StringID Hash = CRC32::Instance.CRC((unsigned char*)Name.c_str(), Name.length());

		std::map<StringID, std::string>::iterator it = StringIDMap.find(Hash);

		if (it == StringIDMap.end())
		{
			StringIDMap[Hash] = Name;
		};

		return Hash;
	};

	StringID MakeStringIDFromChar(const uchar* data, const uint32 length)
	{
		StringID Hash = CRC32::Instance.CRC(data, length);

		std::map<StringID, std::string>::iterator it = StringIDMap.find(Hash);

		if (it == StringIDMap.end())
		{
			StringIDMap[Hash] = std::string((char*)data);
		};

		return Hash;
	};

	std::string GetStringIDString(StringID ID)
	{
		std::map<StringID, std::string>::iterator it = StringIDMap.find(ID);

		if (it == StringIDMap.end())
			return std::string();

		return it->second;
	};
};
