add_rules("mode.debug", "mode.release", "mode.releasedbg")

set_languages("c++latest")
set_warnings("allextra", "error")

set_symbols("hidden")
if is_mode("release") then
    set_optimize("fastest")
elseif is_mode("debug") then
    set_symbols("debug", "hidden")
    add_cxflags("-ggdb3", { tools = { "clang", "gcc" } })
    add_mxflags("-ggdb3", { tools = { "clang", "gcc" } })
elseif is_mode("releasedbg") then
    set_optimize("fast")
    set_symbols("debug", "hidden")
    add_cxflags("-fno-omit-frame-pointer", { tools = { "clang", "gcc" } })
    add_mxflags("-ggdb3", { tools = { "clang", "gcc" } })
end

add_frameworks(is_plat("macosx") and { "Foundation" } or {})

add_repositories("tapzcrew-repo https://github.com/tapzcrew/xmake-repo main")

-- will become stormkit deps when their module is porperly re-exported
add_requires("glm", "frozen")

-- stormkit deps, remove when handled by xmake
add_defines("MAGIC_ENUM_USE_STD_MODULE")
add_defines("MAGIC_ENUM_DEFAULT_ENABLE_ENUM_FORMAT=0")
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
    "ncurses",
    "ftxui"
)

add_requireconfs(
    "glm",
    "pugixml",
    -- "ncurses",
    "cpptrace",
    {system=false}
)

if is_mode("debug") then
    add_defines("_LIBCPP_DEBUG")
end

add_requireconfs("**", { configs = { modules = true, std_import = true, cpp = "latest" }})
add_cxxflags("-fexperimental-library")

option("compile_commands", { default = true, category = "root menu/support" })

if get_config("compile_commands") then
    add_rules("plugin.compile_commands.autoupdate", { outputdir = ".vscode", lsp = "clangd" })
end

target("hroza")
    set_kind("binary")

    add_packages(
        "stormkit",

        -- will become stormkit deps when their module is porperly re-exported
        "glm", "frozen",

        -- stormkit deps, remove when handled by xmake
        "unordered_dense", "magic_enum", "tl_function_ref", "cpptrace",

        "pugixml",
        "ncurses",
        "ftxui"
    )

    add_files("lib/**.mpp")
    add_files("src/**.mpp")
    add_files("src/*.cpp")
    set_rundir("$(projectdir)")
