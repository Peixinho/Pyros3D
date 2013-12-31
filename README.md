Pyros3D

Compilation Process:

	you'll need to use premake4 to build your makefiles (linux), xcode Project (Mac OSX) or VS Project (Win32)

	Dependencies:
	
		SFML 2.1
		FreeType2 (using 2.5.2)
		GLEW
		BulletPhysics 2.8 (using bullet-2.81-rev2613)
		Assimp 3.0 (using 3.0.1264)
		AngelScript (not used for now, but its on the compilation process)
		

// Linux fix for Freetype2
sudo ln -s /usr/include/freetype2/freetype /usr/include/freetype
