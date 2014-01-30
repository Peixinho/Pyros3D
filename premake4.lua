------------------------------------------------------------------
-- premake 4 Pyros3D solution
------------------------------------------------------------------
solution "Pyros3D"
 
    -- Make Directories
    os.mkdir("bin");
    os.mkdir("build");
    os.mkdir("include");
    os.mkdir("libs");

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
        kind "StaticLib"
        --kind "SharedLib"
        --kind "ConsoleApp"
        language "C++"
        files { "src/**.h", "src/**.cpp" }
        
        includedirs { "include/" }

        defines({"UNICODE", "GLEW_STATIC"})

        -- Log Options
        defines({"LOG_DISABLE"}) 
        --| defines({"LOG_TO_FILE"}) | defines({"LOG_TO_CONSOLE"})
                
        configuration "Debug"

            targetname(libName.."d")
            defines({"_DEBUG"})

            if os.get() == "linux" then
                links { "GL", "GLU", "GLEW", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "BulletMultiThreaded", "LinearMath", "MiniCL", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -Wl,-rpath,../libs" }
            end
            
            if os.get() == "windows" then
                links { "opengl32", "glu32", "glew", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "freetype.framework", "GLEW.framework", "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletMultiThreaded.framework", "BulletSoftBody.framework", "LinearMath.framework", "MiniCL.framework" }
            end

            flags { "Symbols" }

        configuration "Release"

            if os.get() == "linux" then
                links { "GL", "GLU", "GLEW", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "BulletMultiThreaded", "LinearMath", "MiniCL", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -Wl,-rpath,../libs" }
            end

            if os.get() == "windows" then
                links { "opengl32", "glu32", "glew", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "freetype.framework", "GLEW.framework", "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletMultiThreaded.framework", "BulletSoftBody.framework", "LinearMath.framework", "MiniCL.framework" }
            end

            flags { "Optimize" }

function BuildDemo(demoPath, demoName)

    project (demoName)
        kind "ConsoleApp"
        language "C++"
        files { demoPath.."/**.h", demoPath.."/**.cpp" }
        includedirs { "include/", "src/" }

        defines({"UNICODE", "GLEW_STATIC"})
        defines({"LOG_DISABLE"})

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("bin/debug/examples/"..demoName)

            if os.get() == "linux" then
                links { libName.."d", "GL", "GLU", "GLEW", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "BulletMultiThreaded", "LinearMath", "MiniCL", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -Wl,-rpath,../../../../libs" }
            end
            
            if os.get() == "windows" then
                links { libName.."d", "opengl32", "glu32", "glew", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName.."d","OpenGL.framework", "Cocoa.framework", "Carbon.framework", "GLEW.framework", "freetype.framework", "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletMultiThreaded.framework", "BulletSoftBody.framework", "LinearMath.framework", "MiniCL.framework" }
            end

            flags { "Symbols" }

        configuration "Release"

            targetdir ("bin/release/examples/"..demoName)

            if os.get() == "linux" then
                links { libName, "GL", "GLU", "GLEW", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "BulletMultiThreaded", "LinearMath", "MiniCL", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -Wl,-rpath,../../../../libs" }
            end

            if os.get() == "windows" then
                links { libName, "opengl32", "glu32", "glew", "sfml-system", "sfml-window", "sfml-graphics", "BulletCollision", "BulletDynamics", "LinearMath", "freetype", "pthread" }
                libdirs { rootdir.."/libs" }
            end

            if os.get() == "macosx" then
                links { libName, "OpenGL.framework", "Cocoa.framework", "Carbon.framework", "GLEW.framework", "freetype.framework", "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework", "BulletCollision.framework", "BulletDynamics.framework", "BulletMultiThreaded.framework", "BulletSoftBody.framework", "LinearMath.framework", "MiniCL.framework" }
            end

            flags { "Optimize" }
end;

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
