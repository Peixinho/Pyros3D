//============================================================================
// Name        : CRC32.hpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : CRC32
//============================================================================

#pragma once
namespace Fishy3D
{
	class CRC32
	{
	private:
		unsigned long LookupTable[256];
		unsigned long Reflect(unsigned long Reflect, char Char);
	public:
		static CRC32 Instance;

		CRC32();
		unsigned long CRC(const unsigned char *Data, unsigned long DataLength);
	};
};
