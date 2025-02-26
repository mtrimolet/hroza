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
    "ncurses",
    {system=false}
)

if is_mode("debug") then
    add_defines("_LIBCPP_DEBUG")
end

add_requireconfs("**", {configs = {modules = true}})
add_cxxflags("-fexperimental-library")

add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

target("hroza")
    set_kind("binary")

    add_packages(
        "stormkit",

        -- stormkit deps, remove when handled by xmake
        "glm", "frozen", "unordered_dense", "magic_enum", "tl_function_ref", "cpptrace",

        "pugixml",
        "ncurses"
    )

    add_files("lib/**.mpp")
    add_files("src/**.mpp")
    add_files("src/*.cpp")
    set_rundir("$(projectdir)")