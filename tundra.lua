Build {
	Env = {
		CPPPATH = {
			"./include",
			"../basis/include"
		},
	},
	Units = function()
		StaticLibrary { 
			Name = "taco",
			Sources = {
				"src/scheduler.cpp",
				"src/windows/fiber_impl.cpp",
				"src/linux/fiber_impl.cpp",
				"src/mutex.cpp",
				"src/condition.cpp",
				"src/shared_mutex.cpp",
				"src/event.cpp"
			}
		}
		Program {
			Name = "scheduler",
			Depends = {
				"taco"
			},
			Sources = {
				"tests/scheduler.cpp"
			}
		}
		Program {
			Name = "future",
			Depends = {
				"taco"
			},
			Sources = {
				"tests/future.cpp"
			}
		}
		Program {
			Name = "generator",
			Depends = {
				"taco"
			},
			Sources = {
				"tests/generator.cpp"
			}
		}
		Default "taco"
	end,
	Configs = {
		{
			Name = "win64-msvc",
			DefaultOnHost = "windows",
			Tools = { { "msvc", TargetArch = "x64" } },
			Env = {
				CPPDEFS = { "BASIS_PLATFORM_WINDOWS" },
				CXXOPTS = { 
					"/W4", 
					"/EHsc",
					{ "/MTd", Config = "*-*-debug" },
					"/FS"
				},
				GENERATE_PDB = "1"
			}
		},
		{
			Name = "win64-msvc_analysis",
			SupportedHosts = { "windows" },
			Tools = { { "msvc", TargetArch = "x64" } },
			Env = {
				CPPDEFS = { "BASIS_PLATFORM_WINDOWS" },
				CXXOPTS = { 
					"/W4", 
					"/EHsc",
					{ "/MTd", Config = "*-*-debug" },
					"/FS",
					"/analyze",
				}
			}
		},
		{
			Name = "linux-gcc",
			DefaultOnHost =  "linux" ,
			Tools = { { "gcc", TargetArch = "x64" } },
			Env = {
				CPPDEFS = { "BASIS_PLATFORM_LINUX" },
				CXXOPTS = { 
					"-std=c++11", 
					{ "-g", Config = "*-*-debug"  },
				},
				PROGOPTS = { "-pthread" }
			},
			ReplaceEnv = {
				LD = { "$(CXX)" ; Config = { "*-gcc-*" } },
			},
		},
	},

	Variants = { "debug", "release" },
}