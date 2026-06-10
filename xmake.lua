add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})
add_rules("mode.debug", "mode.releasedbg", "mode.release")
set_policy("run.autobuild", true)
set_policy("build.warning", true)
-- If compiling with MSVC, add flag that specifies that __cplusplus macro to use actual cpp standard
add_cxxflags("cl::/Zc:__cplusplus")
-- xmake-repo doesn't have the latest version of lexy, so there repos folder with redefinition of the lexy package that has newer version
add_repositories("my-repo repos", {rootdir = os.scriptdir()})

set_languages("c++23")

local app_reqs = { "fmt", "lexy 2025.05.0" }
local app_pkgs = table.imap(app_reqs, function(i, v) return v:split(' ')[1] end) -- Remove version from each req

add_requireconfs("*", { debug = is_mode("debug") })
add_requireconfs("lexy", { configs = { unicode = true } })
add_requires(app_reqs)

target("app")
    set_kind("binary")
    add_files("src/main.cpp")

    set_warnings("allextra", "pedantic", "error")

    add_packages(app_pkgs)

    if is_plat("windows") then
        add_defines("PLAT_WINDOWS")
    elseif is_plat("linux") then 
        add_defines("PLAT_LINUX")
    elseif is_plat("macosx") then 
        add_defines("PLAT_MAC")
    end
    