Pyros3D

Compilation Process:

	you'll need to use premake4 to build your makefiles (linux), xcode Project (Mac OSX) or VS Project (Win32)

	Dependencies:
		FreeType2 (using 2.5.2)
		GLEW
		BulletPhysics 2.8 (using bullet-2.81-rev2613)
		
		Interfaces Working for SFML2.1 and SDL 2.0 (No Text Input Working yet)

// Linux fix for Freetype2
sudo ln -s /usr/include/freetype2/freetype /usr/include/freetype
