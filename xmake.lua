add_rules("mode.debug", "mode.release")

set_languages("c++latest")

add_repositories("tapzcrew-repo https://github.com/tapzcrew/xmake-repo main")

add_requires("stormkit develop", {
    configs = {
        image = false,
        wsi = false,
        log = false,
        entities = false,
        gpu = false,
        components = { "core", "main" },
    },
})
-- stormkit deps, remove when handled by xmake
add_requires("glm", "frozen", "unordered_dense", "magic_enum", "tl_function_ref", "cpptrace")
add_frameworks(is_plat("macosx") and { "Foundation" } or {})

add_defines("PUGIXML_USE_STD_MODULE")
add_requires(
    "pugixml",
    {system=false}
)


add_requireconfs("**", {configs = {modules = true}})
add_cxxflags("-fexperimental-library")

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

target("hroza")
    set_kind("binary")

    add_packages(
        "stormkit",

        -- stormkit deps, remove when handled by xmake
        "glm", "frozen", "unordered_dense", "magic_enum", "tl_function_ref", "cpptrace",

        "pugixml"
    )

    add_files("src/*.cpp")
    add_files("src/**.mpp")
    set_rundir("$(projectdir)")