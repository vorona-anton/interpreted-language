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

        if package:has_tool("cxx", "cl") then
            -- Disable warning C4702: unreachable code on msvc compiler
            --[[Reason:
            Compiler displays that warning for each operation within an lexy::dsl::expression_production,
            because of include/lexy/dsl/expression.hpp(524) (last line of lexyd::_expr::_parse) being unreachable
            These warnings can cause trouble if user enables warnings-as-errors becausethe warnings would come
            from within the package that they don't have control over.
            ]]
            package:add("cxxflags", "/wd4702")
        end
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