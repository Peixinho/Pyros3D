//============================================================================
// Name        : File.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : File
//============================================================================

#ifndef FILE_H
#define FILE_H

#ifdef _WIN32 
#define _CRT_SECURE_NO_DEPRECATE 
#endif
#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

      class File {
            public:

                  File() {}
                  virtual ~File() {}

                  bool Open(const std::string &filename, bool write = false);
                  void Write(const void* src, const uint32 size);
                  void Read(const void* src, const uint32 size);
                  void Rewind();
                  void Close();
                  const uint32 Size() const;
		  std::vector<uchar> &GetData();

            private:

                  FILE* file;
                  std::vector<uchar> data;
                  uint32 size;
                  uint32 positionStream;
				  bool opened;
      };
};

#endif /* FILE_H */