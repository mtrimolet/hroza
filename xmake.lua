
add_rules("mode.debug", "mode.release")

set_languages("c++latest")

add_requires(
    "tinyxml2"
--     "nlohmann_json"
)

-- add_requireconfs("**", { configs = { cxxflags = table.join(cxxflags, "-Wno-unused-command-line-argument"), ldflags = ldflags } })
add_cxxflags("-fexperimental-library")

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

target("hello-world")
    set_kind("binary")

    add_packages(
        "tinyxml2"
    --     "nlohmann_json"
    )

    add_files("src/*.cpp")
    add_files("src/**.mpp")
    set_rundir(".")