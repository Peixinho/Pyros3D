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
        trigger     = "GLES3",
        description = "Use GLES3 (for mobile mostly)"
     }

    newoption {
        trigger     = "GLES2_DESKTOP",
        description = "Use GLES2 on desktop"
     }

     newoption {
        trigger     = "GLES3_DESKTOP",
        description = "Use GLES3 on desktop"
     }

    newoption {
       trigger     = "x32",
       description = "Build for 32bit - Default Option"
    }

    newoption {
        trigger     = "lua",
        description = "Enable Lua Bindings"
    }

    newoption {
       trigger     = "x64",
       description = "Build for 64bit"
    }

    newoption {
       trigger     = "shared",
       description = "Ouput Shared Library - Works only on *NIX platforms"
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

    if _OPTIONS["framework"]=="sdl2" or _OPTIONS['GLES2_DESKTOP'] or _OPTIONS['GLES3_DESKTOP'] then
        framework = "_SDL2";
        libsToLink = { "SDL2", "SDL2_mixer" }
        foldersToExclude = { "**/SFML/**", "**/SDL/**" }
        if  _OPTIONS['GLES2_DESKTOP'] then
            _OPTIONS['GLES2']=1;
            foldersToExclude = { "**/gles3/**" }
        elseif  _OPTIONS['GLES3_DESKTOP'] then 
            _OPTIONS['GLES3']=1;
            foldersToExclude = { "**/gles2/**" }
        else
            foldersToExclude = { "**/gles2/**", "**/gles3/**" }
        end
    elseif _OPTIONS["framework"]=="sdl" then
        framework = "_SDL";
        libsToLink = { "SDL", "SDL_mixer" }
        foldersToExclude = { "**/SFML/**", "**/SDL2/**", "**/gles2/**", "**/gles3/**" }
    elseif _OPTIONS["framework"]=="sfml" or not _OPTIONS["framework"] then
        if os.get() == "macosx" then
            libsToLink = { "SFML.framework", "sfml-system.framework", "sfml-window.framework", "sfml-graphics.framework" }
        else
            libsToLink = { "sfml-audio", "sfml-graphics", "sfml-window", "sfml-system" }
            libsToLinkDebug = { "sfml-audio-d", "sfml-graphics-d", "sfml-window-d", "sfml-system-d" }
        end
        framework = "_SFML";
        foldersToExclude = { "**/SDL2/**", "**/SDL/**", "**/gles2/**", "**/gles3/**" }
    end

    buildArch = "x32"
    if _OPTIONS["x64"] then
        buildArch = "x64"
    end

    if _OPTIONS["GLES2_DESKTOP"] then
        defines({"GLES2_DESKTOP"})
    end
    if _OPTIONS["GLES3_DESKTOP"] then
        defines({"GLES3_DESKTOP"})
    end

    if _OPTIONS["GLES2"] then
        libsToLinkGL = { "GLESv2" }
        defines({"GLES2"})
    else
        if _OPTIONS["GLES3"] then
            libsToLinkGL = { "GLESv2" }
            defines({"GLES3"})
        else
            if os.get() == "linux" then
                libsToLinkGL = { "GL", "GLU", "GLEW" }
            end
            if os.get() == "windows" then
                libsToLinkGL = { "opengl32", "glu32", "glew32" }
            end
            if os.get() == "macosx" then
                libsToLinkGL = { "OpenGL.framework", "GLEW.framework" }
            end
        end
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
        targetdir "bin"
        
        if _OPTIONS["shared"] then
            kind "SharedLib"
        else
            kind "StaticLib"
        end

        if _OPTIONS["lua"] then
            defines({"LUA_BINDINGS"})
        end

        language "C++"
        files { "src/**.h", "src/**.cpp", "include/Pyros3D/**.h", "src/**.c" }
        excludes { foldersToExclude }

	if os.get() == "linux" then
        includedirs { "include/", "/usr/include/freetype2", "/usr/include/bullet" }
        if framework == "_SDL2" then
            includedirs { "/usr/include/SDL2" }
        end
    	else
	        includedirs { "include/" }
    	end

        -- Windows DLL And Lib Creation
        if os.get() == "windows" and _OPTIONS["shared"] then
            defines({"_EXPORT"})
        else
               if _OPTIONS["GLES2"] then
                defines({"GLES2"})
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
                if _OPTIONS["log"]=="none" then
                    defines({"LOG_DISABLE"}) 
                end
            end
        end

        configuration "Debug"

            targetname(libName.."d")
            defines({"_DEBUG"})
           
            if os.get() == "windows" and _OPTIONS["shared"] then
                links { libsToLinkGL, libsToLinkDebug, "BulletDynamics_Debug", "BulletCollision_Debug", "LinearMath_Debug", "freetype265d", "pthreadVC2" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            if os.get() == "windows" and _OPTIONS["lua"] then
                buildoptions { "/bigobj" }
            end
            
            if os.get() == "linux" and _OPTIONS["lua"] then
                buildoptions { "-ftemplate-depth=2048" }
            end

            flags { "Symbols" }

        configuration "Release"

            targetname(libName)

            if os.get() == "windows" and _OPTIONS["shared"] then
                links { libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype265", "pthreadVC2" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            flags { "Optimize" }

            if os.get() == "windows" and _OPTIONS["lua"] then
                buildoptions { "/bigobj" }
            end
            
            if os.get() == "linux" and _OPTIONS["lua"] then
                buildoptions { "-ftemplate-depth=2048" }
            end

    project "AssimpImporter"
        targetdir "bin"
        kind "ConsoleApp"
        language "C++"
        files { "tools/AssimpImporter/src/**.h", "tools/AssimpImporter/src/**.cpp" }
        
        if os.get() == "linux" then
                includedirs { "include/", "/usr/include/freetype2", "/usr/include/bullet" }
                if framework == "_SDL2" then
                    includedirs { "/usr/include/SDL2" }
                end
        else
                includedirs { "include/" }
        end

        if os.get() == "windows" and _OPTIONS["shared"] then
            defines({"_IMPORT"})
        end

        defines({"UNICODE"})

        if os.get() == "windows" then
            defines({"HAVE_STRUCT_TIMESPEC"})
        end

        -- Log Options
        defines({"LOG_DISABLE"}) 
        --| defines({"LOG_TO_FILE"}) | defines({"LOG_TO_CONSOLE"})
                
       configuration "Debug"

            debugdir ("bin")

            defines({"_DEBUG"})

            targetdir ("bin")

            targetname ("AssimpImporterd")

            if os.get() == "linux" then
                links { libName.."d", libsToLinkGL, libsToLink, "assimp", "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "z" }
                linkoptions { "-L../libs -L../bin -L/usr/local/lib -Wl,-rpath,../libs" }
            end
            
            if os.get() == "windows" then
                links { libName.."d", libsToLinkGL, libsToLinkDebug, "assimp", "BulletDynamics_Debug", "BulletCollision_Debug", "LinearMath_Debug", "freetype265d", "pthreadVC2" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            if os.get() == "macosx" then
                links { libName.."d", libsToLinkGL, "Cocoa.framework", "assimp", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            flags { "Symbols" }

        configuration "Release"
 
            debugdir ("bin")

            targetdir ("bin")

            targetname ("AssimpImporter")

            if os.get() == "linux" then
                links { libName, libsToLinkGL, libsToLink, "assimp", "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L../bin -L/usr/local/lib -Wl,-rpath,../libs" }
            end

            if os.get() == "windows" then
                links { libName, libsToLinkGL, libsToLink, "assimp", "BulletDynamics", "BulletCollision", "LinearMath", "freetype265", "pthreadVC2" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            if os.get() == "macosx" then
                links { libName, libsToLinkGL, "assimp", "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            flags { "Optimize" }

function BuildDemo(demoPath, demoName)

    project (demoName)
        kind "ConsoleApp"
        language "C++"
        files { demoPath.."/**.h", demoPath.."/**.cpp", demoPath.."/../WindowManagers/**.cpp", demoPath.."/../WindowManagers/**.h", demoPath.."/../MainProgram.cpp" }

        if framework == "_SDL" then
            excludes { "**/SFML/**" }
            excludes { "**/SDL2/**" }
        else 
            if framework == "_SDL2" then
                excludes { "**/SDL/**" }
                excludes { "**/SFML/**" }
            else
                excludes { "**/SDL/**" }
                excludes { "**/SDL2/**" }
            end
        end

        if _OPTIONS["lua"] then
            defines({"LUA_BINDINGS"})
        end
        
        if os.get() == "windows" then
            defines({"HAVE_STRUCT_TIMESPEC"})
        end

	if os.get() == "linux" then
        includedirs { "include/", "/usr/include/freetype2", "/usr/include/bullet", "src/" }
        if framework == "_SDL2" then
            includedirs { "/usr/include/SDL2" }
        end
        else
            includedirs { "include/", "src/" }
        end
    
        defines({framework});
        defines({"DEMO_NAME="..demoName, "_"..demoName})

        if os.get() == "windows" and _OPTIONS["shared"] then
            defines({"_IMPORT"})
        end

        defines({"UNICODE"})
        
        if os.get() == "windows" then
            defines({"HAVE_STRUCT_TIMESPEC"})
        end

        configuration "Debug"

            debugdir ("bin")

            defines({"_DEBUG"})

            targetdir ("bin")

            targetname (demoName.."d")

            if os.get() == "linux" then
                links { libName.."d", libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L../bin -L/usr/local/lib -Wl,-rpath,../libs" }
            end
            
            if os.get() == "windows" then
                links { libName.."d", libsToLinkGL, libsToLinkDebug, "BulletDynamics_Debug", "BulletCollision_Debug", "LinearMath_Debug", "freetype265d", "pthreadVC2" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            if os.get() == "macosx" then
                links { libName.."d", libsToLinkGL, "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            flags { "Symbols" }

        configuration "Release"

            debugdir ("bin")

            targetdir ("bin")

            targetname (demoName)

            if os.get() == "linux" then
                links { libName, libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype", "pthread", "z" }
                linkoptions { "-L../libs -L../bin -L/usr/local/lib -Wl,-rpath,../libs" }
            end

            if os.get() == "windows" then
                links { libName, libsToLinkGL, libsToLink, "BulletDynamics", "BulletCollision", "LinearMath", "freetype265", "pthreadVC2" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
            end

            if os.get() == "macosx" then
                links { libName, libsToLinkGL, "Cocoa.framework", "Carbon.framework", "freetype.framework", libsToLink, "BulletDynamics.framework", "BulletCollision.framework", "LinearMath.framework" }
                libdirs { rootdir.."/libs", rootdir.."/bin" }
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
    BuildDemo("examples/DepthOfField", "DepthOfField");
    BuildDemo("examples/SSAOExample", "SSAOExample");
    BuildDemo("examples/DeferredRendering", "DeferredRendering");
    BuildDemo("examples/LOD_example", "LOD_example");
    BuildDemo("examples/Decals", "Decals");
    BuildDemo("examples/IslandDemo", "IslandDemo");
    BuildDemo("examples/ParallaxMapping", "ParallaxMapping");
    
    -- Instancing Demo
    if _OPTIONS["GLES2"] or _OPTIONS["GLES3"] then
        BuildDemo("examples/ParticlesExample", "ParticlesExample");
    end

    --BuildDemo("examples/MotionBlur", "MotionBlur");
    if _OPTIONS["lua"] then
        BuildDemo("examples/LuaScripting", "LuaScripting");
    end

    -- ImGui Example only works with SFML for now
    if framework ~= "_SDL" and framework ~= "_SDL2" then
        BuildDemo("examples/RacingGame", "RacingGame");
        BuildDemo("examples/ImGuiExample", "ImGuiExample");
    end

end
