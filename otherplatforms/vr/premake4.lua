------------------------------------------------------------------
-- premake 4 Pyros3D solution
------------------------------------------------------------------
solution "VR_Pyros3D"

    newoption {
       trigger     = "x32",
       description = "Build for 32bit - Default Option"
    }

    newoption {
       trigger     = "x64",
       description = "Build for 64bit"
    }

    if _OPTIONS["framework"]=="sdl2" then
        framework = "_SDL2";
        libsToLink = { "SDL2", "SDL2_mixer" }
        excludes { "**/SFML/**", "**/SDL/**" }
    end

    if _OPTIONS["framework"]=="sdl" then
        framework = "_SDL";
        libsToLink = { "SDL", "SDL_mixer" }
        excludes { "**/SFML/**", "**/SDL2/**" }
    end

    if _OPTIONS["framework"]=="sfml" or not _OPTIONS["framework"] then
        if os.get() == "macosx" then
            libsToLink = { "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework" }
        else
            libsToLink = { "sfml-audio", "sfml-graphics", "sfml-window", "sfml-system" }
            libsToLinkDebug = { "sfml-audio-d", "sfml-graphics-d", "sfml-window-d", "sfml-system-d" }
        end
        framework = "_SFML";
        excludes { "**/SDL2/**", "**/SDL/**" }
    end

    buildArch = "x32"
    if _OPTIONS["x64"] then
        buildArch = "x64"
    end

    if _OPTIONS["GLES2"] then
        libsToLinkGL = { "GLESv2" }
        defines({"GLES2"})
    else
        if os.get() == "linux" then
            libsToLinkGL = { "GL", "GLU", "GLEW" }
        end
        if os.get() == "windows" then
            libsToLinkGL = { "opengl32", "glu32", "glew32s" }
        end
        if os.get() == "macosx" then
            libsToLinkGL = { "OpenGL.framework", "GLEW.framework" }
        end
        defines({"GLEW_STATIC"})
    end

    ------------------------------------------------------------------
    -- setup common settings
    ------------------------------------------------------------------
    configurations { "Debug", "Release" }
    platforms { buildArch }
    location "build"

    rootdir = "../../"

    project ("VR_ShootingRange")
        kind "ConsoleApp"
        language "C++"
        files { "**.h", "**.cpp" }

		if framework == "SDL" then
			excludes { "**/SFML/**" }
			excludes { "**/SDL2/**" }
		else 
			if framework == "SDL2" then
				excludes { "**/SDL/**" }
				excludes { "**/SFML/**" }
			else
				excludes { "**/SDL/**" }
				excludes { "**/SDL2/**" }
			end
		end
		
        includedirs { "../../include/", "../../src/", "src/" }
	
        defines({framework});

		if os.get() == "windows" and _OPTIONS["bin"]=="shared" then
			defines({"_IMPORT"})
		else
        	defines({"GLEW_STATIC"})
        end

        defines({"UNICODE"})
        
        if os.get() == "windows" then
            defines({"HAVE_STRUCT_TIMESPEC"})
        end

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("bin/debug/examples/VR_ShootingRange")

            if os.get() == "linux" then
                links { "PyrosEngined", "openvr_api",libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end
            
            if os.get() == "windows" then
                links { "PyrosEngined", "openvr_api", libsToLinkGL, libsToLinkDebug, "BulletDynamics_Debug", "BulletCollision_Debug", "LinearMath_Debug", "freetype26d", "pthreadVC2" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { "PyrosEngined", "openvr_api", libsToLinkGL, "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Symbols" }

        configuration "Release"

            targetdir ("bin/release/examples/VR_ShootingRange")

            if os.get() == "linux" then
                links { "PyrosEngine", "openvr_api", libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end

            if os.get() == "windows" then
                links { "PyrosEngine", "openvr_api", libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype26", "pthreadVC2" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { "PyrosEngine", "openvr_api", libsToLinkGL, "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Optimize" }
