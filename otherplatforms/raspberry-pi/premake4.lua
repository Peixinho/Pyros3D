------------------------------------------------------------------
-- premake 4 Pyros3D solution
------------------------------------------------------------------
solution "Pyros3D"

    newoption {
       trigger     = "shared",
       description = "Ouput Shared Library"
    }

	newoption {
       trigger     = "static",
       description = "Ouput Static Library - Default Option"
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

    framework = "_SDL2";
    libsToLink = { "SDL2", "SDL2_mixer" }
    excludes { "**/SFML/**", "**/SDL/**" }

    buildArch = "native"

    libsToLinkGL = { "GLESv2" }

    ------------------------------------------------------------------
    -- setup common settings
    ------------------------------------------------------------------
    configurations { "Debug", "Release" }
    platforms { buildArch }
    location "build"

    rootdir = "../../"
    libName = "PyrosEngine"

    project "PyrosEngine"
        targetdir "../../libs"
        
        if _OPTIONS["shared"] then
            kind "SharedLib"
        else
            kind "StaticLib"
        end

        language "C++"
        files { "../../src/**.h", "../../src/**.cpp", "../../include/Pyros3D/**.h" }
        includedirs { "../../include/", "/usr/local/include/SDL2", "/opt/vc/include/", "/usr/include/freetype2", "/usr/include/bullet" }

      	defines({"GLES2", "UNICODE", "LODEPNG", framework })
        
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

    project "AssimpImporter"
        targetdir "../../bin/tools"
        kind "ConsoleApp"
        language "C++"
        files { "../../tools/AssimpImporter/src/**.h", "../../tools/AssimpImporter/src/**.cpp" }
        includedirs { "../../include/", "/usr/local/include/SDL2", "/opt/vc/include/", "/usr/include/freetype2", "/usr/include/bullet" }

        defines({"UNICODE"})

        defines({"LOG_DISABLE"}) 
                
       configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("../../bin/tools/")

            links { libName.."d", libsToLinkGL, libsToLink, "assimp", "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "z" }
            linkoptions { "-L../libs -L/opt/vc/lib -L/usr/local/lib -Wl,-rpath,../../../../libs" }

            flags { "Symbols" }

        configuration "Release"
 
            targetdir ("../../bin/tools/")

            links { libName, libsToLinkGL, libsToLink, "assimp", "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
            linkoptions { "-L../libs -L/opt/vc/lib -L/usr/local/lib -Wl,-rpath,../../../../libs" }

            flags { "Optimize" }

function BuildDemo(demoPath, demoName)

    project (demoName)
        kind "ConsoleApp"
        language "C++"
        files { "../../"..demoPath.."/**.h", "../../"..demoPath.."/**.cpp", "../../"..demoPath.."/../WindowManagers/SDL2/**.cpp", "../../"..demoPath.."/../WindowManagers/**.h", "../../"..demoPath.."/../MainProgram.cpp" }
	includedirs { "../../include/", "/usr/local/include/SDL2", "/opt/vc/include/", "/usr/include/freetype2", "/usr/include/bullet" }
	
        defines({framework, "GLES2", "DEMO_NAME="..demoName, "_"..demoName, "UNICODE"})

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("../../bin/")

            links { libName.."d", libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "z" }
            linkoptions { "-L../../libs -L/opt/vc/lib -L/usr/local/lib -Wl,-rpath,../../../../libs" }

            flags { "Symbols" }

        configuration "Release"

            targetdir ("../../bin/")

	    links { libName, libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
            linkoptions { "-L../../libs -L/opt/vc/lib -L/usr/local/lib -Wl,-rpath,../../../../libs" }

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
