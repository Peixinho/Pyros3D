//============================================================================
// Name        : Resources.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Resources
//============================================================================

#include <Pyros3D/Resources/Resources.h>

// MISSING TEXTURE

namespace p3d {

const unsigned char MISSING_TEXTURE[] = {
0x44,0x44,0x53,0x20,0x7c,0x00,0x00,0x00,0x07,0x10,0x0a,0x00,0x20,0x00,0x00,0x00,
0x20,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,
0x04,0x00,0x00,0x00,0x44,0x58,0x54,0x35,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x40,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdb,0xef,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0xff,0xa0,0xfa,0xff,0xff,0xff,0xff,
0x00,0x05,0xff,0xff,0xff,0xff,0xff,0xff,0x60,0xfc,0x3c,0xf7,0x50,0x50,0x05,0x05,
0x00,0x05,0x3f,0xf0,0x03,0x00,0x00,0x00,0x60,0xfc,0x3c,0xf7,0xf4,0xf1,0xff,0xff,
0x00,0x05,0x07,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xa6,0xf4,0xab,0xaa,0xaa,0xaa
};

};
