#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// GCC complains about a dangling pointer at lexy/action/validate.hpp:188:26,
// but there's nothing that can be done about that, so the error has to be suppressed
// Clang doesn't have -Wdangling-pointer flag, so it needs to be excluded
#if defined(__GNUC__) && !defined (__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif

#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>

// Turn off suppression for -Wdangling-pointer
#if defined(__GNUC__) && !defined (__clang__)
#  pragma GCC diagnostic pop
#endif

#include "fmt/core.h"

namespace ast {
constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
template <typename T> using ptr = std::shared_ptr<T>;
using expr_ptr = ptr<struct expression>;
using statement_ptr = ptr<struct statement>;
using statement_vector = std::vector<statement_ptr>;

struct value {
  virtual ~value() noexcept = default;
};

struct f64 : value {
  explicit f64(double value) : value(value) {};
  double value;
};

struct function : value {
  std::vector<ptr<struct variable>> args;
  statement_vector body;

  function() = default;
  explicit function(std::vector<ptr<struct variable>> args, statement_vector body)
      : args{std::move(args)}, body{std::move(body)} {}
};

struct env {
  std::unordered_map<std::string, ptr<value>> vars;
  env* parent = nullptr;

  static auto from_parent(env& parent) -> env {
    env child{};
    child.parent = &parent;
    return child;
  }

  template <typename T> auto get(std::string const &identifier) -> ptr<T> {
    if (vars.contains(identifier)) {
      return std::dynamic_pointer_cast<T>(vars.at(identifier));
    }

    // If identifier exists, just somewhere down the parent tree
    if (has(identifier)) {
      return parent->get<T>(identifier);
    }

    return nullptr;
  }

  auto has(std::string const &identifier) -> bool {
    return directly_has(identifier) or (parent and parent->has(identifier));
  }

  auto directly_has(std::string const &identifier) -> bool {
    return vars.contains(identifier);
  }
};

struct expression {
  virtual ~expression() = default;
  virtual auto eval(env &) -> double = 0;
};

struct statement {
  virtual ~statement() = default;
  virtual auto exec(env &) -> void = 0;
};

void run_statements(env &env, statement_vector &statements);

struct literal : expression {
  double value = 0;
  explicit literal(double val) : value{val} {}
  auto eval(env &) -> double override { return value; }
};

struct variable : expression {
  std::string identifier;
  explicit variable(std::string name) : identifier{std::move(name)} {}
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
      : lhs{std::move(lhs)}, rhs{std::move(rhs)} {}

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
      : lhs{std::move(lhs)}, rhs{std::move(rhs)}, op{op} {}

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
  explicit prefix(op_t op, expr_ptr val) : val{std::move(val)}, op{op} {}
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
  explicit postfix(expr_ptr callee, op_t) : callee{std::move(callee)} {}
  explicit postfix(expr_ptr callee, op_t, lexy::nullopt) : callee{std::move(callee)} {}
  explicit postfix(expr_ptr callee, op_t, std::vector<expr_ptr> args)
      : callee{std::move(callee)}, args{std::move(args)} {}
  auto eval(env &env) -> double override {
    auto var_ptr = std::dynamic_pointer_cast<variable>(callee);
    if (not var_ptr) {
      fmt::println("Call failed, lhs is not an identifier");
      return nan;
    }

    if (var_ptr->identifier == "report") {
      auto evaluated_args = args
        | std::views::transform([&](auto&& arg) { return arg->eval(env); })
        | std::ranges::to<std::vector>();
      fmt::print("Report:");
      for (auto &&arg : evaluated_args)
        fmt::print(" {}", arg);
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

    ast::env func_env = ast::env::from_parent(env);
    auto set_var = [&](ptr<variable> &var, double arg) {
      return var->set(func_env, arg);
    };

    for (auto &&[var, value] : zip(arg_vars, args | transform(eval_arg))) {
      set_var(var, value);
    }

    try {
      run_statements(func_env, func->body);
      return nan;
    } catch (double return_value) {
      return return_value;
    }
  }
};

struct func_decl : statement {
  std::string identifier;
  function body;
  explicit func_decl(std::string identifier, function body)
      : identifier{std::move(identifier)}, body{std::move(body)} {}

  auto exec(env &env) -> void override {
    if (env.get<f64>(identifier)) {
      fmt::println("'{}' is a variable", identifier);
      return;
    };

    env.vars.insert_or_assign(identifier, std::make_shared<function>(body));
  }
};

struct return_statement : statement {
  expr_ptr expr;
  explicit return_statement(expr_ptr expr) : expr{std::move(expr)} {}
  auto exec(env &env) -> void override {
    throw expr->eval(env);
  }
};

struct expression_statement : statement {
  expr_ptr expr;
  explicit expression_statement(expr_ptr expr) : expr{std::move(expr)} {}
  auto exec(env &env) -> void override {
    expr->eval(env);
  }
};

struct empty_statement : statement {
  explicit empty_statement() = default;
  auto exec(env &env) -> void override { std::ignore = env; }
};

struct if_statement : statement {
  expr_ptr condition;
  statement_vector if_branch;
  statement_vector else_branch;

  explicit if_statement(expr_ptr condition, statement_vector if_branch, statement_vector else_branch)
      : condition{std::move(condition)}, if_branch{std::move(if_branch)}, else_branch{std::move(else_branch)} {}

  auto exec(env &env) -> void override {
    ast::env new_env = ast::env::from_parent(env);
    run_statements(new_env, condition->eval(env) == 1 ? if_branch : else_branch);
  }
};

void run_statements(env &env, statement_vector &statements) {
  for (auto &&statement : statements) {
    statement->exec(env);
  }
}
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
constexpr auto kw_return = LEXY_KEYWORD("return", id_rule);
constexpr auto kw_if = LEXY_KEYWORD("if", id_rule);
constexpr auto kw_else = LEXY_KEYWORD("else", id_rule);

struct identifier {
  static constexpr auto rule =
      id_rule.reserve(kw_fn, kw_return, kw_if, kw_else);
  static constexpr auto value = lexy::as_string<std::string>;
};

struct var {
  static constexpr auto rule = dsl::p<identifier>;
  static constexpr auto value =
      lexy::new_<ast::variable, ast::ptr<ast::variable>>;
};

struct expr : lexy::expression_production {
  static constexpr auto atom = dsl::parenthesized(dsl::recurse<expr>) | dsl::p<number> | dsl::p<var>;

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

struct scope_declaration {
  static constexpr auto rule  = dsl::curly_bracketed.opt_list(dsl::recurse<struct statement>);
  static constexpr auto value = lexy::as_list<ast::statement_vector>;
};

struct function_body {
  static constexpr auto rule = dsl::p<arg_list> + dsl::p<scope_declaration>;
  static constexpr auto value = lexy::construct<ast::function>;
};

struct function_declaration {
  static constexpr auto rule =
      kw_fn >> dsl::p<identifier> + dsl::p<function_body>;
  static constexpr auto value = lexy::new_<ast::func_decl, ast::statement_ptr>;
};

struct return_statement {
  static constexpr auto rule = kw_return >> dsl::p<expr> + dsl::semicolon;
  static constexpr auto value = lexy::new_<ast::return_statement, ast::statement_ptr>;
};

struct expression_statement {
  static constexpr auto rule = dsl::p<expr> + dsl::semicolon;
  static constexpr auto value = lexy::new_<ast::expression_statement, ast::statement_ptr>;
};

struct empty_statement {
  struct useless_semicolon {
    static constexpr auto name = "Unnessesary semicolon";
  };

  static constexpr auto rule = dsl::semicolon;
  static constexpr auto value = lexy::new_<ast::empty_statement, ast::statement_ptr>;
};

struct else_clause {
  static constexpr auto rule = dsl::opt(kw_else >> dsl::p<scope_declaration>);
  static constexpr auto value = lexy::callback<ast::statement_vector>(
    [](lexy::nullopt) -> ast::statement_vector { return {}; },
    lexy::forward<ast::statement_vector>
  );
};

struct if_statement {
  static constexpr auto
      rule = kw_if >> dsl::p<expr>
        + dsl::p<scope_declaration> + dsl::p<else_clause>;

  static constexpr auto value = lexy::new_<ast::if_statement, ast::statement_ptr>;
};

struct statement {
  static constexpr auto rule = dsl::p<function_declaration> | dsl::p<return_statement>
    | dsl::p<if_statement> | dsl::p<empty_statement>
    | dsl::else_ >> dsl::p<expression_statement>;
  static constexpr auto value = lexy::forward<ast::statement_ptr>;
};

struct program {
  static constexpr auto whitespace = dsl::inline_<grammar::whitespace>;
  static constexpr auto rule =
      dsl::terminator(dsl::eof).opt_list(dsl::p<statement>);
  static constexpr auto value = lexy::as_list<ast::statement_vector>;
};
} // namespace grammar

int parse_file(char const *path, ast::statement_vector &out_statements) {
  auto file = lexy::read_file<lexy::utf8_char_encoding>(path);
  if (not file) {
    fmt::println("Couldn't open the file! Error ID: {}", (int)file.error());
    return 1;
  }

  auto result =
      lexy::parse<grammar::program>(file.buffer(), lexy_ext::report_error);
  if (not result.has_value())
    return 1;

  out_statements = result.value();
  return 0;
}
} // namespace

auto main(int argc, char **argv) -> int try {
  std::string raw_input;
  ast::env env{};

  if (argc == 2) {
    ast::statement_vector statements;
    int failure = parse_file(argv[1], statements);
    if (failure) {
      return 1;
    }

    ast::run_statements(env, statements);
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
    auto parse_result = lexy::parse<grammar::program>(input, lexy_ext::report_error);
    if (not parse_result.has_value())
      return 1;

    auto statements = parse_result.value();

    run_statements(env, statements);
  }
} catch (double return_value) {
  return static_cast<int>(return_value);
}