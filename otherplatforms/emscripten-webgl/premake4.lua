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
       trigger     = "examples",
       description = "Build Demos Examples"
    }

    framework = "_SDL";
    libsToLink = { "SDL" }
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
        
        kind "SharedLib"

        language "C++"
        files { "../../src/**.h", "../../src/**.cpp" }
        excludes { "../../src/Pyros3D/AssetManager/Assets/Texture/Texture.cpp", "../../src/Pyros3D/AssetManager/Assets/Font/**", "../../src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/**" }
        
        includedirs { "include/" }

        defines({"UNICODE", "GLEW_STATIC", "LOG_DISABLE", "LODEPNG"}) 
                
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
        targetname(demoName..".html")
        files { demoPath.."/**.h", demoPath.."/**.cpp", demoPath.."/../WindowManagers/**.cpp", demoPath.."/../WindowManagers/**.h", demoPath.."/../MainProgram.cpp" }

        excludes { demoPath.."/../WindowManagers/SDL2/**" }
        excludes { demoPath.."/../WindowManagers/SFML/**" }
        
        includedirs { "include/", "../../src/" }
    
        defines({"UNICODE", "GLEW_STATIC"})
        defines({framework});

    defines({"DEMO_NAME="..demoName, "_"..demoName})

        configuration "Debug"

            defines({"_DEBUG"})

            targetdir ("bin/debug/examples/"..demoName)

            links { libName.."d", "freeimage", "BulletCollision", "BulletDynamics", "LinearMath", "freetype" }
            linkoptions { "-L../libs" }
          
            flags { "Symbols" }
            linkoptions { "--preload-file ../"..demoPath.."/assets@../../"..demoPath.."/assets" }

        configuration "Release"

            targetdir ("bin/release/examples/"..demoName)

            links { libName, "freeimage", "BulletCollision", "BulletDynamics", "LinearMath", "freetype" }
            linkoptions { "-L../libs" }

            flags { "Optimize" }
            linkoptions { "--preload-file ../"..demoPath.."/assets@../../"..demoPath.."/assets" }
end;

if _OPTIONS["examples"] then
    BuildDemo("../../examples/RotatingCube", "RotatingCube");
    BuildDemo("../../examples/RotatingTexturedCube", "RotatingTexturedCube");
    BuildDemo("../../examples/RotatingTextureAnimatedCube", "RotatingTextureAnimatedCube");
    BuildDemo("../../examples/RotatingCubeWithLighting", "RotatingCubeWithLighting");
    BuildDemo("../../examples/RotatingCubeWithLightingAndShadow", "RotatingCubeWithLightingAndShadow");
    BuildDemo("../../examples/SimplePhysics", "SimplePhysics");
    BuildDemo("../../examples/TextRendering", "TextRendering");
    BuildDemo("../../examples/CustomMaterial", "CustomMaterial");
    BuildDemo("../../examples/PickingPainterMethod", "PickingPainterMethod");
    BuildDemo("../../examples/SkeletonAnimation", "SkeletonAnimation");
    BuildDemo("../../examples/DeferredRendering", "DeferredRendering");
    BuildDemo("../../examples/RacingGame", "RacingGame");
end
