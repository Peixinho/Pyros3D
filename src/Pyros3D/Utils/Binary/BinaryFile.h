//============================================================================
// Name        : WriteBinary.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Write Binary File
//============================================================================

#ifndef BINARYFILE_H
#define	BINARYFILE_H

#include "../../Other/Export.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <string>

using namespace std;

namespace p3d {

	class PYROS3D_API BinaryFile {

		public:

			BinaryFile() {}
			
			virtual ~BinaryFile() {}

			void Open(const char* file, const char &o)
			{
				option = o;
				
				switch(o)
				{
					case 'r':
						is = new ifstream(file,ios::binary);
					break;
					case 'w':
					default:
						os = new ofstream(file,ios::binary);
					break;
				}

				memory = false;
			}

			void OpenFromMemory(uchar* data, const uint32 &size)
			{
				// Using Memory
				memory = true;

				// Resize Vector
				this->data.resize(size*sizeof(uchar));

				// Copy Contents
				memcpy(&this->data[0],data,sizeof(uchar)*size);

				// Set Initial Position
				positionStream = 0;

				// Save Size
				this->size = size;
			}

			void Close()
			{
				if (memory)
					data.clear();

				else
					switch(option)
					{
						case 'r':
							is->close();
							delete is;
						break;
						case 'w':
						default:
							os->close();
							delete os;
						break;
					}
			}

			void Write(const void* src, const uint32 &size)
			{
				if (!memory)
					os->write((const char*)src, size);
			}

			void Read(const void* src, const uint32 &size)
			{
				if (!memory)
					is->read((char*)src, size);

				else {
					memcpy((char*)src, &data[positionStream], sizeof(uchar)*size);
					positionStream += size * sizeof(uchar);
				}
			}

		private:

			// From File
			char option;
			ofstream *os;
			ifstream *is;

			// From Memory
			bool memory;
			std::vector<uchar> data;
			uint32 size;
			uint32 positionStream;
	};
};
#endif /* BINARYFILE_H */
