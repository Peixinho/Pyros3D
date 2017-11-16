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
       trigger     = "jsnative",
       description = "Build Engine to be used from Javascript - Proof of Concept"
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

    if _OPTIONS['jsnative'] then
            excludes { "**/SDL/**" }
        else
            framework = "_SDL";
            libsToLink = { "SDL" }
    end
    
    excludes { "**/SFML/**", "**/SDL2/**" }

    premake.gcc.cc = "emcc";
    premake.gcc.cxx = "em++";

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
        
        if _OPTIONS["jsnative"] then
            kind "ConsoleApp"
        else
            kind "SharedLib"
        end

        language "C++"
        files { "../../src/**.h", "../../src/**.cpp" }

        if _OPTIONS["jsnative"] then
            files { "src/JSNative/JSNative_Wrapper.h", "src/JSNative/JSNative_Wrapper.cpp" }
        end
        
        includedirs { "../../include/", "include/" }

        defines({"UNICODE", "LODEPNG", "GLES2", "EMSCRIPTEN"})

        if _OPTIONS["jsnative"]==nil then
            defines({framework})
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

        if _OPTIONS["jsnative"] then  
        configuration "Debug"

            targetname(libName.."d.js")
            defines({"_DEBUG"})
            flags { "Symbols" }
            targetdir ("bin")

            links { "BulletDynamics", "BulletCollision", "LinearMath", "freetype" }
            linkoptions { "-L../libs", "--post-js ../src/JSNative/glue.js" }

        configuration "Release"

            flags { "Optimize" }
            targetname(libName..".js")
            targetdir ("bin")

            links { "BulletDynamics", "BulletCollision", "LinearMath", "freetype" }
            linkoptions { "-L../libs", "--post-js ../src/JSNative/glue.js" }
        else
            configuration "Debug"

            targetname(libName.."d")
            defines({"_DEBUG"})
            flags { "Symbols" }

        configuration "Release"

            flags { "Optimize" }
            targetname(libName)
        end

function BuildDemo(demoPath, demoName)

    project (demoName)
        kind "ConsoleApp"
        language "C++"
        targetname(demoName..".html")
        files { demoPath.."/**.h", demoPath.."/**.cpp", demoPath.."/../WindowManagers/**.cpp", demoPath.."/../WindowManagers/**.h", demoPath.."/../MainProgram.cpp" }

        excludes { demoPath.."/../WindowManagers/SDL2/**" }
        excludes { demoPath.."/../WindowManagers/SFML/**" }
        
        includedirs { "../../include/", "include/", "../../src/" }
    
        defines({"UNICODE", "GLEW_STATIC", "GLES2", "EMSCRIPTEN"})
        defines({framework});

    defines({"DEMO_NAME="..demoName, "_"..demoName})

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("bin")

            links { libName.."d", "BulletDynamics", "BulletCollision", "LinearMath", "freetype" }
            linkoptions { "-L../libs" }
          
            flags { "Symbols" }
            linkoptions { "--preload-file ../"..demoPath.."/assets@../../"..demoPath.."/assets" }

        configuration "Release"

            targetdir ("bin")

            links { libName, "BulletDynamics", "BulletCollision", "LinearMath", "freetype" }
            linkoptions { "-L../libs" }

            flags { "Optimize" }
            linkoptions { "--preload-file ../"..demoPath.."/assets@../../"..demoPath.."/assets" }
end;

if _OPTIONS["examples"] and _OPTIONS["jsnative"]==nil then
    BuildDemo("../../examples/RotatingCube", "RotatingCube");
    BuildDemo("../../examples/RotatingTexturedCube", "RotatingTexturedCube");
    BuildDemo("../../examples/RotatingTextureAnimatedCube", "RotatingTextureAnimatedCube");
    BuildDemo("../../examples/RotatingCubeWithLighting", "RotatingCubeWithLighting");
    BuildDemo("../../examples/RotatingCubeWithLightingAndShadow", "RotatingCubeWithLightingAndShadow");
    BuildDemo("../../examples/SimplePhysics", "SimplePhysics");
    BuildDemo("../../examples/TextRendering", "TextRendering");
    BuildDemo("../../examples/CustomMaterial", "CustomMaterial");
    BuildDemo("../../examples/PickingPainterMethod", "PickingPainterMethod");
    BuildDemo("../../examples/SkeletonAnimationExample", "SkeletonAnimationExample");
    BuildDemo("../../examples/DepthOfField", "DepthOfField");
    BuildDemo("../../examples/SSAOExample", "SSAOExample");
    BuildDemo("../../examples/DeferredRendering", "DeferredRendering");
    BuildDemo("../../examples/LOD_example", "LOD_example");
    BuildDemo("../../examples/Decals", "Decals");
    BuildDemo("../../examples/IslandDemo", "IslandDemo");
    BuildDemo("../../examples/ParallaxMapping", "ParallaxMapping");
end