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
				"src/task.cpp",
				"src/mutex.cpp",
				"src/condition.cpp"
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
	},

	Variants = { "debug", "release" },
}