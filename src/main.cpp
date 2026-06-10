#include <vector>

#include "lexy/callback/container.hpp"
#include "lexy/dsl/ascii.hpp"
#include "lexy/dsl/eof.hpp"
#include "lexy/dsl/separator.hpp"
#include "lexy/dsl/terminator.hpp"
#include "lexy/grammar.hpp"
#include <fmt/base.h>

#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/argv_input.hpp>
#include <lexy_ext/report_error.hpp>

namespace
{
struct Color
{
    std::uint8_t r, g, b;
};

namespace grammar {
namespace dsl = lexy::dsl;
struct channel
{
    static constexpr auto rule = dsl::integer<std::uint8_t>(dsl::n_digits<2, dsl::hex>);
    static constexpr auto value = lexy::forward<std::uint8_t>;
};

struct color : lexy::token_production
{
    static constexpr auto rule = dsl::hash_sign + dsl::times<3>(dsl::p<channel>);
    static constexpr auto value = lexy::construct<Color>;
};

struct production {
    static constexpr auto whitespace = dsl::ascii::space;
    static constexpr auto rule = dsl::terminator(dsl::eof).opt_list(dsl::p<color>, dsl::sep(dsl::argv_separator));
    static constexpr auto value = lexy::as_list<std::vector<Color>>;
};
}
}

auto main(int argc, char** argv) -> int {
    lexy::argv_input input(argc, argv);
    auto result = lexy::parse<grammar::production>(input, lexy_ext::report_error);
    if (result.has_value())
    {
        for (auto const& color : result.value()) {
            fmt::println("#{0:02x}{1:02x}{2:02x} - rgb({0}, {1}, {2})", color.r, color.g, color.b);
        }
    }

    return result ? 0 : 1;
}