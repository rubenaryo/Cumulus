APP_NAME = "Cumulus"

workspace (APP_NAME)
    architecture "x64"
    startproject (APP_NAME)

    configurations
    {
        "Debug",
        "Release"
    }
    
    flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}x64"

project (APP_NAME)
    location (APP_NAME)
    kind "WindowedApp"
    language "C++"

    targetdir ("_bin/" .. outputdir .. "/%{prj.name}")
    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }
	
    dependson { "Shaders" }

    includedirs
    {
		"external/**/include/",
        "%{prj.name}/src",
        "external/imgui/",
        "external/imgui/backends/"
    }

    libdirs
    {
        "external/assimp/",
		"external/directxtex/%{cfg.buildcfg}/"
    }

    links
    {
        "external/assimp/assimp",
        "external/directxtex/%{cfg.buildcfg}/DirectXTex",
        "dxgi",
        "d3d12"
    }

    prebuildcommands
    {
        ("{MKDIR} %{!wks.location}/_bin/" .. outputdir .. "/" .. APP_NAME)
    }
    postbuildcommands
    {
        ("{COPYFILE} %{!cfg.buildtarget.abspath} %{!wks.location}_bin/".. outputdir .. "/" .. APP_NAME .. "/%{prj.name}.dll"),
        ("{COPYFILE} %{!wks.location}/external/assimp/Assimp64.dll %{!wks.location}_bin/".. outputdir .. "/" .. APP_NAME .. "/Assimp64.dll")
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "latest"

        defines
        {
            "MN_PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "MN_DEBUG"
        symbols "On"
        staticruntime "Off"

    filter "configurations:Release"
        defines "MN_RELEASE"
        optimize "On"
        staticruntime "Off"

project "Shaders"
    location "Assets/Shaders"
    kind "ConsoleApp"
    shadermodel "6.5"

    shaderobjectfileoutput ("%{!wks.location}/_bin/Shaders/%%(Filename).cso")
    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{!wks.location}/Assets/Shaders/**.hlsl",
        "%{!wks.location}/Assets/Shaders/**.hlsli"
    }

    filter { "files:**.ps.hlsl" }
        shadertype "Pixel"

    filter { "files:**.vs.hlsl" }
        shadertype "Vertex"
		
	filter { "files:**.cs.hlsl" }
        shadertype "Compute"

project "ImGui"
    location "external/imgui"
    kind "StaticLib"

    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    includedirs
    {
        "external/imgui/",
        "external/imgui/backends/"
    }

    links
    {
        "dxgi",
        "d3d12"
    }

    files
    {
        "%{!wks.location}/external/imgui/*.h",
        "%{!wks.location}/external/imgui/*.cpp",
        "%{!wks.location}/external/imgui/backends/imgui_impl_dx12.h",
        "%{!wks.location}/external/imgui/backends/imgui_impl_dx12.cpp",
        "%{!wks.location}/external/imgui/backends/imgui_impl_win32.h",
        "%{!wks.location}/external/imgui/backends/imgui_impl_win32.cpp",
        "%{!wks.location}/external/imgui/misc/cpp/imgui_stdlib.*",
        "%{!wks.location}/external/imgui/misc/debuggers/cpp/imgui_stdlib.*",
        
    }