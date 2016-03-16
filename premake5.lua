solution "gm_collider_cl"
	language     "C++"
	architecture "x32"
	location     "project"
	targetdir    "bin"

	flags "StaticRuntime"

	configurations { "Release" }

	configuration "Release"
		flags    "symbols"
		optimize "On"

	project "gm_collider_cl"

		kind "ConsoleApp"
		files { "*.hpp", "*cpp" }
		includedirs ( os.getenv("AMDAPPSDKROOT").."/include" )
		libdirs ( os.getenv("AMDAPPSDKROOT").."/lib/x86" )
		links { "OpenCL", "pthread" }
		if (os.get() == "linux") then
			buildoptions "-std=c++11 -Wl,-rpath=."
		end
		
