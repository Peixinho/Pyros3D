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
          { "sfml", "SFML 2.1 - Default" },
          { "sdl2", "SDL 2.0" },
          { "sdl", "SDL 1.x" }
       }
    }

    newoption {
        trigger = "bin",
        value = "output",
        description = "Choose a output binary file",
        allowed = {
            { "static", "Static Library - Default" },
            { "shared", "Shared Library" }
        }
    }

    newoption {
       trigger     = "examples",
       description = "Build Demos Examples"
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
        libsToLink = { "SDL2" }
        excludes { "**/SFML/**", "**/SDL/**" }
    end

    if _OPTIONS["framework"]=="sdl" then
        framework = "_SDL";
        libsToLink = { "SDL" }
        excludes { "**/SFML/**", "**/SDL2/**" }
    end

    if _OPTIONS["framework"]=="sfml" or not _OPTIONS["framework"] then
        framework = "_SFML";
        libsToLink = { "sfml-graphics", "sfml-window", "sfml-system" }
        excludes { "**/SDL2/**", "**/SDL/**" }
    end

    ------------------------------------------------------------------
    -- setup common settings
    ------------------------------------------------------------------
    configurations { "Debug", "Release" }
    platforms { "x32" }
    location "build"

    rootdir = "."
    libName = "PyrosEngine"

    project "PyrosEngine"
        targetdir "libs"
        
        if _OPTIONS["bin"]=="shared" then
            kind "SharedLib"
        else
            kind "StaticLib"
        end

        language "C++"
        files { "src/**.h", "src/**.cpp" }
        
        includedirs { "include/" }

		if os.get() == "windows" and _OPTIONS["bin"]=="shared" then
			defines({"_EXPORT"})
		else
        	defines({"GLEW_STATIC"})
        end

        defines({"UNICODE"})

        if _OPTIONS["log"]=="console" then
            defines({"LOG_TO_CONSOLE"})
        else
            if _OPTIONS["log"]=="file" then
                defines({"LOG_TO_FILE"})
            else
                defines({"LOG_DISABLE"}) 
            end
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

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("bin/debug/examples/"..demoName)

            if os.get() == "linux" then
                links { libName.."d", "GL", "GLU", "GLEW", libsToLink, "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end
            
            if os.get() == "windows" then
                links { libName.."d", "opengl32", "glu32", "glew", libsToLink, "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName.."d", "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "GLEW.framework", "freetype.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletSoftBody.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Symbols" }

        configuration "Release"

            targetdir ("bin/release/examples/"..demoName)

            if os.get() == "linux" then
                links { libName, "GL", "GLU", "GLEW", libsToLink, "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end

            if os.get() == "windows" then
                links { libName, "opengl32", "glu32", "glew", libsToLink, "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName, "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "GLEW.framework", "freetype.framework", "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletSoftBody.framework", "LinearMath.framework" }
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
    BuildDemo("examples/SkeletonAnimation", "SkeletonAnimation");
    BuildDemo("examples/DeferredRendering", "DeferredRendering");
    BuildDemo("examples/RacingGame", "RacingGame");
end
