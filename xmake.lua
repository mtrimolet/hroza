add_rules("mode.debug", "mode.release")

set_languages("c++latest")
-- set_warnings("allextra", "error")
set_symbols("hidden")
add_frameworks(is_plat("macosx") and { "Foundation" } or {})

add_repositories("tapzcrew-repo https://github.com/tapzcrew/xmake-repo main")

-- will become stormkit deps when their module is porperly re-exported
add_requires("glm", "frozen")

-- stormkit deps, remove when handled by xmake
add_requires("unordered_dense", "magic_enum", "tl_function_ref", "cpptrace")

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

add_defines("PUGIXML_USE_STD_MODULE")
add_requires(
    "pugixml",
    "ncurses"
)

add_requireconfs(
    "glm",
    "pugixml",
    "ncurses",
    "cpptrace",
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

        -- will become stormkit deps when their module is porperly re-exported
        "glm", "frozen",

        -- stormkit deps, remove when handled by xmake
        "unordered_dense", "magic_enum", "tl_function_ref", "cpptrace",

        "pugixml",
        "ncurses"
    )

    add_files("lib/**.mpp")
    add_files("src/**.mpp")
    add_files("src/*.cpp")
    set_rundir("$(projectdir)")
