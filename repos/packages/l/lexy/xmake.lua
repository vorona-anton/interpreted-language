package("lexy")
    set_homepage("https://lexy.foonathan.net/")
    set_description("C++ parsing DSL")
    set_license("BSL-1.0")

    add_urls("https://github.com/foonathan/lexy/releases/download/v$(version)/lexy-src.zip")
    add_versions("2025.05.0", "222f4fbc122bc2d8e237f97285ae06e52cfc9c83d7c423e08f604c6012f1250f")

    add_deps("cmake")

    add_configs("unicode", {
        description = "Enable Unicode database.", default = false, type = "boolean"
    })

    on_load(function (package)
        if package:config("unicode") then
            package:add("defines", "LEXY_HAS_UNICODE_DATABASE=1")
        end

        -- Disable warnings
        --[[Reason:
        Compiler may throw warnings from within the lexy headerfiles that the user has no control over
        ]]
        package:set("warnings", "none")
    end)

    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DLEXY_BUILD_EXAMPLES=OFF")
        table.insert(configs, "-DLEXY_BUILD_TESTS=OFF")
        table.insert(configs, "-DLEXY_ENABLE_INSTALL=ON")
        import("package.tools.cmake").install(package, configs)
    end)

    on_test(function (package)
        assert(package:has_cxxincludes("lexy/dsl.hpp", {configs = {languages = "c++17"}}))
    end)
package_end()