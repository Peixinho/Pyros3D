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
          { "sdl", "SDL 2.0" }
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
        description = "Log Output",
        allowed = {
            { "none", "No log - Default" },
            { "console", "Log to Console"},
            { "file", "Log to File"}
        }
    }

    if _OPTIONS["framework"]=="sdl" then
        framework = "_SDL";
        libsToLink = { "SDL2", "SDL2_image" }
        excludes { "**/SFML/**" }
    end

    if _OPTIONS["framework"]=="sfml" or not _OPTIONS["framework"] then
        framework = "_SFML";
        libsToLink = { "sfml-graphics", "sfml-window", "sfml-system" }
        excludes { "**/SDL/**" }
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

        defines({"UNICODE", "GLEW_STATIC"})

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
        files { demoPath.."/**.h", demoPath.."/**.cpp", demoPath.."/../WindowManagers/**.cpp", demoPath.."/../WindowManagers/**.h" }
        includedirs { "include/", "src/" }

        defines({"UNICODE", "GLEW_STATIC"})
        defines({framework});

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("bin/debug/examples/"..demoName)

            if os.get() == "linux" then
                links { libName.."d", "GL", "GLU", "GLEW", libsToLink, "freeimage", "BulletCollision", "BulletDynamics", "BulletMultiThreaded", "LinearMath", "MiniCL", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end
            
            if os.get() == "windows" then
                links { libName.."d", "opengl32", "glu32", "glew", libsToLink, "freeimage", "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName.."d", "freeimage", "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "GLEW.framework", "freetype.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletMultiThreaded.framework", "BulletSoftBody.framework", "LinearMath.framework", "MiniCL.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Symbols" }

        configuration "Release"

            targetdir ("bin/release/examples/"..demoName)

            if os.get() == "linux" then
                links { libName, "GL", "GLU", "GLEW", libsToLink, "freeimage", "BulletCollision", "BulletDynamics", "BulletMultiThreaded", "LinearMath", "MiniCL", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L/usr/local/lib -Wl,-rpath,../../../../libs" }
            end

            if os.get() == "windows" then
                links { libName, "opengl32", "glu32", "glew", libsToLink, "freeimage", "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName, "freeimage", "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "GLEW.framework", "freetype.framework", "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletMultiThreaded.framework", "BulletSoftBody.framework", "LinearMath.framework", "MiniCL.framework" }
                libdirs { rootdir.."/libs" }
            end

            flags { "Optimize" }
end;

if _OPTIONS["examples"] then
    BuildDemo("examples/RotatingCube", "RotatingCube");
    BuildDemo("examples/RotatingTexturedCube", "RotatingTexturedCube");
    BuildDemo("examples/RotatingCubeWithLighting", "RotatingCubeWithLighting");
    BuildDemo("examples/RotatingCubeWithLightingAndShadow", "RotatingCubeWithLightingAndShadow");
    BuildDemo("examples/SimplePhysics", "SimplePhysics");
    BuildDemo("examples/TextRendering", "TextRendering");
    BuildDemo("examples/CustomMaterial", "CustomMaterial");
    BuildDemo("examples/PickingWithPainterMethod", "PickingWithPainterMethod");
    BuildDemo("examples/SkeletonAnimation", "SkeletonAnimation");
    BuildDemo("examples/RacingGame", "RacingGame");
end