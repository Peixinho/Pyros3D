//============================================================================
// Name        : WriteBinary.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Write Binary File
//============================================================================

#ifndef BINARYFILE_H
#define	BINARYFILE_H

#include <fstream>
#include <vector>
#include <iostream>
#include <string>

using namespace std;

namespace p3d {

	class BinaryFile {

		public:

			BinaryFile() {}
			
			virtual ~BinaryFile() {}

			void Open(const char* file, const char &o)
			{
				File = file;
				option = o;

				switch(o)
				{
					case 'r':
						is = new ifstream(File.c_str(),ios::binary);
					break;
					case 'w':
					default:
						os = new ofstream(File.c_str(),ios::binary);
					break;
				}
			}

			void Close()
			{
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

			void Write(const void* src, const int &size)
			{
				os->write((const char*)src, size);
			}

			void Read(const void* src, const int &size)
			{
				is->read((char*)src, size);
			}

		private:

			char option;
			std::string File;
			ofstream *os;
			ifstream *is;
	};
};
#endif /* BINARYFILE_H */