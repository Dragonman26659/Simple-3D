-- premake5.lua
workspace "Simple 3D"
    configurations { "Debug", "Release"}

project "Simple 3D"
    location "/src/Simple-3D"
    kind "StaticLib"
    language "C++"


    targetdir ("bin/Simple-3D/%{cfg.buildcfg}")
    objdir ("bin-int/Simple-3D/%{cfg.buildcfg}")

    files {
        "src/%{prj.name}/**.h",
        "src/%{prj.name}/**.hpp", 
        "src/%{prj.name}/**.c", 
        "src/%{prj.name}/**.cpp",
        "include/**.h",
        "nclude/**.hpp"
    }

    includedirs {
        "vendor/GLFW/include",
        "vendor/SDL/include",
        "vendor/Vulkan/Include"
    }



    libdirs { 
        "vendor/SDL/bin", 
        "vendor/GLFW/lib-vc2022",
        "vendor/Vulkan/Lib",
    }

    links {
        "glfw3",
        "SDL2-staticd",
        "vulkan-1.lib"
    }





    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        symbols "On"
    
    filter "system:windows"
        cppdialect "C++17"
        systemversion "latest"

project "GLFW Example"
    location "/src/GLFW-Example"
    kind "WindowedApp"
    language "C++"
    targetdir "bin/Examples/GLFW/%{cfg.buildcfg}"
    objdir ("bin-int/Examples/GLFW/%{cfg.buildcfg}")

    files {
        "src/%{prj.name}/**.h",
        "src/%{prj.name}/**.hpp", 
        "src/%{prj.name}/**.c", 
        "src/%{prj.name}/**.cpp"
    }

    includedirs {
        "include",
        "vendor/GLFW/include"
    }


    libdirs { 
        "vendor/GLFW/lib-vc2022" 
    }

    links {
        "glfw3"
    }

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        symbols "On"

    filter "system:windows"
        cppdialect "C++17"
        systemversion "latest"

project "SDL Example"
    location "/src/SDL-Example"
    kind "WindowedApp"
    language "C++"

    targetdir "bin/Examples/SDL2/%{cfg.buildcfg}"
    objdir ("bin-int/Examples/SDL2/%{cfg.buildcfg}")

    files {
        "src/%{prj.name}/**.h",
        "src/%{prj.name}/**.hpp", 
        "src/%{prj.name}/**.c", 
        "src/%{prj.name}/**.cpp"
    }

    includedirs {
        "include",
        "vendor/SDL/include"
    }

    libdirs { 
        "vendor/SDL/bin"
    }

    links {
        "SDL2-staticd"
    }

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        symbols "On"

    filter "system:windows"
        cppdialect "C++17"
        systemversion "latest"