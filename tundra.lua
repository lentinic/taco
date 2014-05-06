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
				CPPDEFS = { "WIN32" },
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
				CPPDEFS = { "WIN32" },
				CXXOPTS = { 
					"/W4", 
					"/EHsc",
					{ "/MTd", Config = "*-*-debug" },
					"/FS",
					"/analyze",
				}
			}
		},
	},

	Variants = { "debug", "release" },
}