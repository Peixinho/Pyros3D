//============================================================================
// Name        : WriteBinary.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Write Binary File
//============================================================================

#ifndef BINARYFILE_H
#define	BINARYFILE_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/File/File.h>

using namespace std;

namespace p3d {

	class PYROS3D_API BinaryFile {

		public:

			BinaryFile() {}
			
			virtual ~BinaryFile() {}

			bool Open(const char* filename, const char &o)
			{
				option = o;
				file = new File();
				bool opened = false;
				switch(o)
				{
					case 'r':
						opened = file->Open(filename,false);
					break;
					case 'w':
					default:
						opened = file->Open(filename,true);
					break;
				}

				memory = false;
				
				return opened;
			}

			void OpenFromMemory(uchar* data, const uint32 size)
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
				{
					data.clear();
				} else {
					file->Close();
					delete file;
				}
			}

			void Write(const void* src, const uint32 size)
			{
				if (!memory)
					file->Write((const char*)src, size);
			}

			void Read(const void* src, const uint32 size)
			{
				if (!memory)
					file->Read((char*)src, size);

				else {
					memcpy((char*)src, &data[positionStream], sizeof(uchar)*size);
					positionStream += size * sizeof(uchar);
				}
			}

		private:

			// From File
			char option;
			File* file;

			// From Memory
			bool memory;
			std::vector<uchar> data;
			uint32 size;
			uint32 positionStream;
	};
};
#endif /* BINARYFILE_H */
