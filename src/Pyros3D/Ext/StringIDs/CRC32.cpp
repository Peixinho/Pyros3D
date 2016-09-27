//============================================================================
// Name        : CRC32.cpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : CRC32
//============================================================================

#include <Pyros3D/Ext/StringIDs/CRC32.hpp>
#include <iostream>
namespace p3d
{
	CRC32 CRC32::Instance;

	CRC32::CRC32()
	{
		unsigned long Poly = 0x04C11DB7;

		unsigned crc;

		for (unsigned long i = 0; i <= 0xFF; i++)
		{
			unsigned long &Value = LookupTable[i];

			//			Value = Reflect(i, 8) << 24;
			//
			//			for(uchar j = 0; j < 8; j++)
			//			{
			//				Value = (Value << 1) ^ (Value & (1 << 31) ? Poly : 0);
			//			};
			//
			//			Value = Reflect(Value, 32);

			crc = i << 24;
			for (int j = 0; j < 8; j++) {
				if (crc & 0x80000000)
					crc = (crc << 1) ^ Poly;
				else
					crc = crc << 1;
			}
			Value = crc;
		};

	};

	unsigned long CRC32::Reflect(unsigned long Reflect, char Char)
	{
		unsigned long Value = 0;

		for (unsigned long i = 1; i < (unsigned char)(Char + 1); i++)
		{
			if (Reflect & 1)
			{
				Value |= (1 << (Char - i));
			}

			Reflect >>= 1;
		};

		return Value;
	};

	unsigned long CRC32::CRC(const unsigned char *Data, unsigned long DataLength)
	{
		unsigned long OutCRC = 0xFFFFFFFF;

		while (DataLength--)
			OutCRC = (OutCRC >> 8) ^ LookupTable[(OutCRC & 0xFF) ^ *Data++];

		return OutCRC ^ 0xFFFFFFFF;
	};
};
