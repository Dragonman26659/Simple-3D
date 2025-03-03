-- premake5.lua
workspace "Simple 3D"
    configurations { "Debug", "Release"}

project "Simple-3D"
    location '%{prj.name}'
    kind "StaticLib"
    language "C++"


    targetdir ("bin/%{prj.name}/%{cfg.buildcfg}")
    objdir ("bin-int/%{prj.name}/%{cfg.buildcfg}")


    files {
        "%{prj.name}/**.c",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp"
    }

    includedirs {
        "Simple-3D/include",
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

project "GLFW-Example"
    location '%{prj.name}'
    kind "WindowedApp"
    language "C++"


    targetdir ("bin/%{prj.name}/%{cfg.buildcfg}")
    objdir ("bin-int/%{prj.name}/%{cfg.buildcfg}")


    files {
        "%{prj.name}/**.c",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp"
    }

    includedirs {
        "Simple-3D/include",
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

project "SDL-Example"
    location '%{prj.name}'
    kind "WindowedApp"
    language "C++"
    
    
    targetdir ("bin/%{prj.name}/%{cfg.buildcfg}")
    objdir ("bin-int/%{prj.name}/%{cfg.buildcfg}")
    
    
    files {
        "%{prj.name}/**.c",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp"
    }

    includedirs {
        "Simple-3D/include",
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