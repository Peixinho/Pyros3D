------------------------------------------------------------------
-- premake 4 Pyros3D solution
------------------------------------------------------------------
solution "Pyros3D"
 
    -- Make Directories
    os.mkdir("bin");
    os.mkdir("build");
    os.mkdir("include");
    os.mkdir("libs");

	newoption {
       trigger     = "framework",
       value       = "API",
       description = "Choose a particular API for window management",
       allowed = {
          { "sfml", "SFML 2.2 - Default" },
          { "sdl2", "SDL 2.0" },
          { "sdl", "SDL 1.x" }
       }
    }

    newoption {
       trigger     = "GLES2",
       description = "Use GLES2 (for mobile mostly)"
    }

    newoption {
       trigger     = "x32",
       description = "Build for 32bit - Default Option"
    }

    newoption {
       trigger     = "x64",
       description = "Build for 64bit"
    }

    newoption {
       trigger     = "shared",
       description = "Ouput Shared Library - Default Option"
    }

	newoption {
       trigger     = "static",
       description = "Ouput Static Library"
    }

    newoption {
       trigger     = "examples",
       description = "Build Demos Examples"
    }

    newoption {
       trigger     = "lodepng",
       description = "Using Lodepng to load textures (PNG ONLY!)"
    }

    newoption {
        trigger = "log",
        value       = "OUTPUT",
        description = "Log Output",
        allowed = {
            { "none", "No log - Default" },
            { "console", "Log to Console"},
            { "file", "Log to File"}
        }
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

    rootdir = "."
    libName = "PyrosEngine"

    project "PyrosEngine"
        targetdir "libs"
        
        if _OPTIONS["static"] then
            kind "StaticLib"
        else
            kind "SharedLib"
        end

        language "C++"
        files { "src/**.h", "src/**.cpp", "include/Pyros3D/**.h" }
        includedirs { "include/" }

        -- LodePNG
        if not _OPTIONS["lodepng"] then
            excludes { "lopdeng/" }
        end

        -- Windows DLL And Lib Creation
		if os.get() == "windows" and _OPTIONS["bin"]=="shared" then
			defines({"_EXPORT"})
		else
        	   if _OPTIONS["GLES2"] then
                defines({"GLES2"})
            else
                defines({"GLEW_STATIC"})
            end
        end

        defines({ "UNICODE", framework })
        
        if os.get() == "windows" then
            defines({"HAVE_STRUCT_TIMESPEC"})
        end

        if _OPTIONS["log"]=="console" then
            defines({"LOG_TO_CONSOLE"})
        else
            if _OPTIONS["log"]=="file" then
                defines({"LOG_TO_FILE"})
            else
                defines({"LOG_DISABLE"}) 
            end
        end

        if _OPTIONS["lodepng"] then
            defines({"LODEPNG"})
        end

        configuration "Debug"

            targetname(libName.."d")
            defines({"_DEBUG"})
            flags { "Symbols" }

        configuration "Release"

            flags { "Optimize" }
            targetname(libName)

function BuildDemo(demoPath, demoName)

    project (demoName)
        kind "ConsoleApp"
        language "C++"
        files { demoPath.."/**.h", demoPath.."/**.cpp", demoPath.."/../WindowManagers/**.cpp", demoPath.."/../WindowManagers/**.h", demoPath.."/../MainProgram.cpp" }

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
		
        includedirs { "include/", "src/" }
	
        defines({framework});
		defines({"DEMO_NAME="..demoName, "_"..demoName})

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

            debugdir ("bin/debug/examples/"..demoName)

            defines({"_DEBUG"})

            targetdir ("bin/debug/examples/"..demoName)

            if os.get() == "linux" then
                links { libName.."d", libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end
            
            if os.get() == "windows" then
                links { libName.."d", libsToLinkGL, libsToLinkDebug, "BulletDynamics_Debug", "BulletCollision_Debug", "LinearMath_Debug", "freetype26d", "pthreadVC2" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName.."d", libsToLinkGL, "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Symbols" }

        configuration "Release"

            debugdir ("bin/release/examples/"..demoName)

            targetdir ("bin/release/examples/"..demoName)

            if os.get() == "linux" then
                links { libName, libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end

            if os.get() == "windows" then
                links { libName, libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype26", "pthreadVC2" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName, libsToLinkGL, "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Optimize" }
end;

if _OPTIONS["examples"] then
    BuildDemo("examples/RotatingCube", "RotatingCube");
    BuildDemo("examples/RotatingTexturedCube", "RotatingTexturedCube");
	BuildDemo("examples/RotatingTextureAnimatedCube", "RotatingTextureAnimatedCube");
    BuildDemo("examples/RotatingCubeWithLighting", "RotatingCubeWithLighting");
    BuildDemo("examples/RotatingCubeWithLightingAndShadow", "RotatingCubeWithLightingAndShadow");
    BuildDemo("examples/SimplePhysics", "SimplePhysics");
    BuildDemo("examples/TextRendering", "TextRendering");
    BuildDemo("examples/CustomMaterial", "CustomMaterial");
    BuildDemo("examples/PickingPainterMethod", "PickingPainterMethod");
    BuildDemo("examples/SkeletonAnimationExample", "SkeletonAnimationExample");
    BuildDemo("examples/DeferredRendering", "DeferredRendering");
    BuildDemo("examples/LOD_example", "LOD_example");
    BuildDemo("examples/IslandDemo", "IslandDemo");
    BuildDemo("examples/RacingGame", "RacingGame");

	-- ImGui Example only works with SFML for now
	if framework ~= "SDL" or not "SDL2" then
		BuildDemo("examples/ImGuiExample", "ImGuiExample");
	end

end
