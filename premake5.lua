APP_NAME = "Application"

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

    includedirs
    {
		"external/**/include/",
        "%{prj.name}/src"
    }

    libdirs
    {
        "external/assimp/",
        "external/dxtk12/%{cfg.buildcfg}/"
    }

    links
    {
        "external/assimp/assimp",
        "external/dxtk12/%{cfg.buildcfg}/DirectXTK12"
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
        shadermodel "5.0"

    filter "configurations:Release"
        defines "MN_RELEASE"
        optimize "On"
        staticruntime "Off"
        shadermodel "5.0"

project "Shaders"
    location "Assets/Shaders"
    kind "ConsoleApp"
    shadermodel "5.0"

    shaderobjectfileoutput ("%{!wks.location}/_bin/Shaders/%%(Filename).cso")
    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{!wks.location}/Assets/Shaders/**.hlsl",
        "%{!wks.location}/Assets/Shaders/**.hlsli"
    }

    filter { "files:**PS.hlsl" }
        shadertype "Pixel"

    filter { "files:**VS.hlsl" }
        shadertype "Vertex"
