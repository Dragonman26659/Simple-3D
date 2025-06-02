-- premake5.lua
workspace "Simple 3D"
    configurations { "Debug", "Release"}

    newoption {
        trigger = "use-sdl",
        description = "Use SDL instead of GLFW",
        default = "off"
    }

    newoption {
        trigger = "use-imgui",
        description = "Enable ImGui support",
        default = "off"
    }

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
        "vendor/Vulkan/Include",
        "vendor/glm",
        "vendor/ImGui"
    }



    libdirs { 
        "vendor/SDL/bin", 
        "vendor/GLFW/lib-vc2022",
        "vendor/Vulkan/Lib"
    }

    links {
        "glfw3",
        "SDL2",
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
        architecture "x64"

    -- Add feature-specific defines
    filter { "options:use-sdl" }
        defines { "SDL_WINDOW" }

    filter { "options:use-imgui" }
        defines { "USEIMGUI" }

project "GLFW-Example"
    location '%{prj.name}'
    kind "ConsoleApp"
    language "C++"


    targetdir ("bin/%{prj.name}/%{cfg.buildcfg}")
    objdir ("bin-int/%{prj.name}/%{cfg.buildcfg}")


    files {
        "%{prj.name}/**.c",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp"
    }

    files {
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_demo.cpp"
    }

    includedirs {
        "Simple-3D/include",
        "vendor/GLFW/include",
        "vendor/Vulkan/Include",
        "vendor/TinyObjLoader/Include",
        "vendor/glm",
        "vendor/ImGui"
    }


    libdirs { 
        "vendor/GLFW/lib-vc2022" ,
        "bin/Simple-3D/%{cfg.buildcfg}"
    }

    links {
        "glfw3",
        "Simple-3D"
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
        architecture "x64"


        -- Add feature-specific defines
    filter { "options:use-sdl" }
        defines { "SDL_WINDOW" }

    filter { "options:use-imgui" }
        defines { "USEIMGUI" }

project "SDL-Example"
    location '%{prj.name}'
    kind "ConsoleApp"
    language "C++"
    
    
    targetdir ("bin/%{prj.name}/%{cfg.buildcfg}")
    objdir ("bin-int/%{prj.name}/%{cfg.buildcfg}")
    
    
    files {
        "%{prj.name}/**.c",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp"
    }

    files {
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_demo.cpp"
    }

    includedirs {
        "Simple-3D/include",
        "vendor/SDL/include",
        "vendor/Vulkan/Include",
        "vendor/TinyObjLoader/Include",
        "vendor/glm",
        "vendor/ImGui"
    }

    libdirs { 
        "vendor/SDL/bin",
        "bin/Simple-3D/%{cfg.buildcfg}"
    }

    links {
        "SDL2",
        "Simple-3D"
    }

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        symbols "On"

    -- Add feature-specific defines
    filter { "options:use-sdl" }
        defines { "SDL_WINDOW" }

    filter { "options:use-imgui" }
        defines { "USEIMGUI" }

    filter "system:windows"
        cppdialect "C++17"
        systemversion "latest"
        architecture "x64"