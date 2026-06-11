
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#define LEXY_HAS_UNICODE_DATABASE 1
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>

#include "fmt/core.h"

namespace ast {
constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
template <typename T> using ptr = std::shared_ptr<T>;
using expr_ptr = ptr<struct expression>;

struct value {
  virtual ~value() noexcept = default;
};

struct f64 : value {
  explicit f64(double value) : value(value) {};
  double value;
};

struct function : value {
  std::vector<ptr<struct variable>> args;
  expr_ptr result;

  function() = default;
  explicit function(std::vector<ptr<struct variable>> args, expr_ptr result)
      : args{LEXY_MOV(args)}, result{LEXY_MOV(result)} {}
};

struct env {
  std::unordered_map<std::string, ptr<value>> vars;

  template <typename T> auto get(std::string const &identifier) -> ptr<T> {
    ptr<value> name = has(identifier) ? vars.at(identifier) : nullptr;

    return std::dynamic_pointer_cast<T>(name);
  }

  auto has(std::string const &identifier) -> bool {
    return vars.contains(identifier);
  }
};

struct expression {
  virtual ~expression() = default;
  virtual auto eval(env &) -> double = 0;
};

struct literal : expression {
  double value = 0;
  explicit literal(double val) : value{val} {}
  auto eval(env &) -> double override { return value; }
};

struct variable : expression {
  std::string identifier;
  explicit variable(std::string name) : identifier{name} {}
  auto eval(env &env) -> double override {
    if (not env.has(identifier)) {
      fmt::println("Variable '{}' doesn't exist", identifier);
      return nan;
    }

    auto var = env.get<f64>(identifier);

    if (not var) {
      fmt::println("'{}' isn't a variable", identifier);
      return nan;
    }

    return var->value;
  }

  auto set(env &env, double value) -> double {
    if (not env.has(identifier)) {
      env.vars[identifier] = std::make_shared<f64>(value);
      return value;
    }

    auto var = env.get<f64>(identifier);

    if (not var) {
      fmt::println("'{}' isn't a variable", identifier);
      return nan;
    }

    return var->value = value;
  }
};

struct assignment : expression {
  enum struct op_t { value };

  expr_ptr lhs;
  expr_ptr rhs;
  explicit assignment(expr_ptr lhs, op_t, expr_ptr rhs)
      : lhs{LEXY_MOV(lhs)}, rhs{LEXY_MOV(rhs)} {}

  auto eval(env &env) -> double override {
    auto var = std::dynamic_pointer_cast<variable>(lhs);
    if (not var) {
      fmt::println("Assignment failed, lhs is not a variable name");
      return nan;
    }

    auto value = rhs->eval(env);
    var->set(env, value);
    return value;
  }
};

struct binop : expression {
  enum struct op_t { add, sub, mul, div };

  expr_ptr lhs, rhs;
  op_t op;

  explicit binop(expr_ptr lhs, op_t op, expr_ptr rhs)
      : lhs{lhs}, rhs{rhs}, op{op} {}

  auto eval(env &env) -> double override {
    switch (op) {
      using enum op_t;
    case add:
      return lhs->eval(env) + rhs->eval(env);
    case sub:
      return lhs->eval(env) - rhs->eval(env);
    case mul:
      return lhs->eval(env) * rhs->eval(env);
    case div:
      return lhs->eval(env) / rhs->eval(env);
    default:
      std::unreachable();
    }
  }
};

struct prefix : expression {
  enum struct op_t { invert, plus };

  expr_ptr val;
  op_t op;
  explicit prefix(op_t op, expr_ptr val) : val{val}, op{op} {}
  auto eval(env &env) -> double override {
    switch (op) {
      using enum op_t;
    case invert:
      return -val->eval(env);
    case plus:
      return val->eval(env);
    default:
      std::unreachable();
    }
  }
};

struct postfix : expression {
  enum struct op_t { value };

  expr_ptr callee;
  std::vector<expr_ptr> args;
  explicit postfix(expr_ptr callee, op_t) : callee{callee} {}
  explicit postfix(expr_ptr callee, op_t, lexy::nullopt) : callee{callee} {}
  explicit postfix(expr_ptr callee, op_t, std::vector<expr_ptr> args)
      : callee{LEXY_MOV(callee)}, args{LEXY_MOV(args)} {}
  auto eval(env &env) -> double override {
    auto var_ptr = std::dynamic_pointer_cast<variable>(callee);
    if (not var_ptr) {
      fmt::println("Call failed, lhs is not an identifier");
      return nan;
    }

    if (var_ptr->identifier == "report") {
      fmt::print("Report:");
      for (auto &&arg : args)
        fmt::print(" {}", arg->eval(env));
      fmt::print("\n");
      return 0;
    }

    if (not env.has(var_ptr->identifier)) {
      fmt::println("Call failed, function doesn't exist");
      return nan;
    }

    auto func = env.get<function>(var_ptr->identifier);
    auto &arg_vars = func->args;
    if (arg_vars.size() != args.size()) {
      fmt::println("Call failed, expected {} args, got {}", arg_vars.size(),
                   args.size());
      return nan;
    }

    using namespace std::views;
    auto eval_arg = [&](expr_ptr &arg) { return arg->eval(env); };

    ast::env func_env{};
    auto set_var = [&](ptr<variable> &var, double arg) {
      return var->set(func_env, arg);
    };

    for (auto &&[var, value] : zip(arg_vars, args | transform(eval_arg))) {
      set_var(var, value);
    }

    return func->result->eval(func_env);
  }
};

struct func_decl {
  std::string identifier;
  function body;
  explicit func_decl(std::string identifier, function body)
      : identifier{LEXY_MOV(identifier)}, body{LEXY_MOV(body)} {}

  auto set(env &env) -> void {
    if (env.get<f64>(identifier)) {
      fmt::println("'{}' is a variable", identifier);
      return;
    };

    env.vars.insert_or_assign(identifier, std::make_shared<function>(body));
  }
};

using statement = std::variant<ast::func_decl, ast::expr_ptr>;
} // namespace ast

namespace {
namespace detail {
namespace dsl = lexy::dsl;

template <std::floating_point T = double, typename Base = dsl::decimal>
struct real_scanner : lexy::scan_production<T>, lexy::token_production {
  template <typename Context, typename Reader>
  static constexpr auto scan(lexy::rule_scanner<Context, Reader> &scanner)
      -> lexy::scan_result<T> {
    lexy::scan_result<int> int_part;
    scanner.parse(int_part,
                  dsl::integer<int>(dsl::digits<Base>.no_leading_zero()));

    T result = int_part.value();

    // If we encounter a dot, this means we have a fraction we need to parse
    // Since lexy has no built in way for that
    if (scanner.branch(dsl::lit_c<'.'>)) {
      auto frac_begin = scanner.position();
      scanner.parse(dsl::digits<Base>);

      T fraction = 0;
      T denominator = Base::digit_radix;
      while (frac_begin != scanner.position()) {
        fraction += Base::digit_value(*frac_begin++) / denominator;
        denominator *= Base::digit_radix;
      }

      result += fraction;
    }

    return result;
  }
};
} // namespace detail

namespace extra {
namespace dsl = lexy::dsl;

template <std::floating_point T = double, typename Base = dsl::decimal>
static constexpr auto real = dsl::p<detail::real_scanner<T, Base>>;
} // namespace extra

namespace callbacks {
template <std::floating_point T>
static constexpr auto as_real = lexy::as_integer<T>;
}

namespace grammar {
namespace dsl = lexy::dsl;

struct single_comment {
  static constexpr auto rule = LEXY_LIT("//") >>
                               dsl::until(dsl::newline).or_eof();
};

struct block_comment {
  static constexpr auto rule = LEXY_LIT("/*") >> dsl::until(LEXY_LIT("*/"));
};

struct comment {
  static constexpr auto rule =
      dsl::inline_<single_comment> | dsl::inline_<block_comment>;
};

struct whitespace {
  static constexpr auto rule = dsl::ascii::space | dsl::inline_<comment>;
};

struct number {
  static constexpr auto rule = dsl::peek(dsl::digit<>) >> extra::real<double>;
  static constexpr auto value =
      lexy::new_<ast::literal, ast::ptr<ast::literal>>;
};

constexpr auto id_rule = dsl::identifier(dsl::unicode::xid_start_underscore,
                                         dsl::unicode::xid_continue);

constexpr auto kw_fn = LEXY_KEYWORD("fn", id_rule);

struct identifier {
  static constexpr auto rule = id_rule.reserve(kw_fn);
  static constexpr auto value = lexy::as_string<std::string>;
};

struct var {
  static constexpr auto rule = dsl::p<identifier>;
  static constexpr auto value =
      lexy::new_<ast::variable, ast::ptr<ast::variable>>;
};

struct expr : lexy::expression_production {
  static constexpr auto atom = dsl::p<number> | dsl::p<var>;

  struct call : dsl::postfix_op {
    static constexpr auto op = dsl::op<ast::postfix::op_t::value>(
        dsl::parenthesized.opt(dsl::recurse<struct expr_list>));
    using operand = dsl::atom;
  };

  struct prefix : dsl::prefix_op {
    static constexpr auto op =
        dsl::op<ast::prefix::op_t::plus>(dsl::lit_c<'+'>) /
        dsl::op<ast::prefix::op_t::invert>(dsl::lit_c<'-'>);
    using operand = call;
  };

  struct multiplication : dsl::infix_op_left {
    static constexpr auto op = dsl::op<ast::binop::op_t::mul>(dsl::lit_c<'*'>) /
                               dsl::op<ast::binop::op_t::div>(dsl::lit_c<'/'>);
    using operand = prefix;
  };

  struct addition : dsl::infix_op_left {
    static constexpr auto op = dsl::op<ast::binop::op_t::add>(dsl::lit_c<'+'>) /
                               dsl::op<ast::binop::op_t::sub>(dsl::lit_c<'-'>);
    using operand = multiplication;
  };

  struct assignment : dsl::infix_op_single {
    static constexpr auto op =
        dsl::op<ast::assignment::op_t::value>(dsl::equal_sign);
    using operand = addition;
  };

  using operation = assignment;
  static constexpr auto value = lexy::callback<ast::expr_ptr>(
      lexy::forward<ast::expr_ptr>, lexy::new_<ast::assignment, ast::expr_ptr>,
      lexy::new_<ast::binop, ast::expr_ptr>,
      lexy::new_<ast::prefix, ast::expr_ptr>,
      lexy::new_<ast::postfix, ast::expr_ptr>);
};

struct expr_list {
  static constexpr auto rule = dsl::list(dsl::p<expr>, dsl::sep(dsl::comma));
  static constexpr auto value = lexy::as_list<std::vector<ast::expr_ptr>>;
};

struct arg_list {
  static constexpr auto rule =
      dsl::parenthesized.opt_list(dsl::p<var>, dsl::sep(dsl::comma));
  static constexpr auto value =
      lexy::as_list<std::vector<ast::ptr<ast::variable>>>;
};

struct function_body {
  static constexpr auto rule = dsl::p<arg_list> + LEXY_LIT("->") + dsl::p<expr>;
  static constexpr auto value = lexy::construct<ast::function>;
};

struct function_declaration {
  static constexpr auto rule =
      kw_fn >> dsl::p<identifier> + dsl::p<function_body>;
  static constexpr auto value = lexy::construct<ast::func_decl>;
};

struct statement {
  static constexpr auto rule =
      dsl::p<function_declaration> | dsl::else_ >> dsl::p<expr>;
  static constexpr auto value = lexy::construct<ast::statement>;
};

struct program {
  static constexpr auto whitespace = dsl::inline_<grammar::whitespace>;
  static constexpr auto rule =
      dsl::terminator(dsl::eof).opt_list(dsl::p<statement> + dsl::semicolon);
  static constexpr auto value = lexy::as_list<std::vector<ast::statement>>;
};
} // namespace grammar
} // namespace

auto main(int argc, char **argv) -> int {
  std::string raw_input;
  ast::env env{};

  if (argc == 2) {
    auto file = lexy::read_file<lexy::utf8_char_encoding>(argv[1]);
    if (not file) {
      fmt::println("Couldn't open the file! Error ID: {}", (int)file.error());
      return 1;
    }

    auto result =
        lexy::parse<grammar::program>(file.buffer(), lexy_ext::report_error);
    if (not result.has_value())
      return 1;

    auto res = result.value();

    for (auto &&statement : res) {
      std::visit(
          [&]<typename T>(T &value) {
            if constexpr (std::same_as<T, ast::expr_ptr>) {
              value->eval(env);
            } else {
              value.set(env);
            }
          },
          statement);
    }

    return 0;
  }

  while (true) {
    raw_input.clear();
    raw_input.shrink_to_fit();
    fmt::print(">> ");
    std::getline(std::cin, raw_input);

    if (raw_input == "quit") {
      break;
    }

    auto input = lexy::string_input<lexy::utf8_char_encoding>(raw_input);
    auto result = lexy::parse<grammar::program>(input, lexy_ext::report_error);
    if (not result.has_value())
      return 1;

    auto res = result.value();

    for (auto &&statement : res) {
      std::visit(
          [&]<typename T>(T &value) {
            if constexpr (std::same_as<T, ast::expr_ptr>) {
              value->eval(env);
            } else {
              value.set(env);
            }
          },
          statement);
    }
  }
}