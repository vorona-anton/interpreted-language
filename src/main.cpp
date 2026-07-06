#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
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
// constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
template <typename T> using ptr = std::shared_ptr<T>;
using expr_ptr = ptr<struct expression>;
using statement_ptr = ptr<struct statement>;
using statement_vector = std::vector<statement_ptr>;
using func_ptr = ptr<struct function>;

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };

using none = std::monostate;

template <class T>
constexpr std::string_view type_name_v = "unknown";  // comment: fallback for unhandled, no hard error

template <>
constexpr std::string_view type_name_v<none> = "none";

template <>
constexpr std::string_view type_name_v<double> = "double";

template <>
constexpr std::string_view type_name_v<bool> = "bool";

template <>
constexpr std::string_view type_name_v<func_ptr> = "function";

template <typename T>
constexpr auto type_name_of = type_name_v<std::decay_t<T>>;

struct value {
  std::variant<none, bool, double, func_ptr> data;

  template <typename T>
  value(T t) : data{t} {}

  operator bool() {
    auto* ptr = std::get_if<bool>(&data);
    if (not ptr) {
      fmt::println("Value isn't a bool, default to false");
    }
    return ptr ? *ptr : false;
  }

  auto operator+(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs + rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot add {} and {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator-(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs - rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot subtract {} and {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator*(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs * rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot multiply {} and {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator/(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs / rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot subtract {} and {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator<(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs < rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} < {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator<=(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs <= rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} <= {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator>(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs < rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} < {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator>=(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](double lhs, double rhs) { return lhs >= rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} >= {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator==(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      []<typename T>(T lhs, T rhs) { return lhs == rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} == {}", type_name_of<T>, type_name_of<U>);
        return none{};
      },
    }, lhs.data, rhs.data);
  }

  auto operator!=(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      []<typename T>(T lhs, T rhs) { return lhs != rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} != {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator&&(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](bool lhs, bool rhs) { return lhs && rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} && {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator||(this const value& lhs, const value& rhs) -> value {
    return std::visit<value>(overload{
      [](bool lhs, bool rhs) { return lhs || rhs; },
      []<typename T, typename U>(T&&, U&&) {
        fmt::println("Cannot compare if {} && {}", type_name_of<T>, type_name_of<U>);
        return none{};
      }
    }, lhs.data, rhs.data);
  }

  auto operator-() -> value {
    return std::visit<value>(overload{
      [](double v) { return -v; },
      []<typename T>(T&&) {
        fmt::println("Cannot negate {}", type_name_of<T>);
        return none{};
      }
    }, data);
  }

  auto operator+() -> value {
    return std::visit<value>(overload{
      [](double v) { return v; },
      []<typename T>(T&&) {
        fmt::println("Cannot complement {}", type_name_of<T>);
        return none{};
      }
    }, data);
  }

  auto operator!() -> value {
    return std::visit<value>(overload{
      [](bool v) { return !v; },
      []<typename T>(T&&) {
        fmt::println("Cannot invert {}", type_name_of<T>);
        return none{};
      }
    }, data);
  }
};

struct env {
  std::unordered_map<std::string, value> vars;
  env* parent = nullptr;

  static auto from_parent(env& parent) -> env {
    env child{};
    child.parent = &parent;
    return child;
  }

  auto get(std::string const &identifier) -> value* {
    if (vars.contains(identifier)) {
      return &vars.at(identifier);
    }

    // If identifier exists, just somewhere down the parent tree
    if (has(identifier)) {
      return parent->get(identifier);
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
  virtual auto eval(env &) -> value = 0;
};

struct statement {
  virtual ~statement() = default;
  virtual auto exec(env &) -> void = 0;
};

void run_statements(env &env, statement_vector &statements);

struct literal : expression {
  double value = 0;
  explicit literal(double val) : value{val} {}
  auto eval(env &) -> ast::value override { return value; }
};

struct variable : expression {
  std::string identifier;
  explicit variable(std::string name) : identifier{std::move(name)} {}
  auto eval(env &env) -> value override {
    if (not env.has(identifier)) {
      fmt::println("Variable '{}' doesn't exist", identifier);
      return none{};
    }

    auto var = env.get(identifier);

    if (not var) {
      fmt::println("'{}' isn't a variable", identifier);
      return none{};
    }

    return *var;
  }

  auto declare(env& env, const ast::value& value) -> ast::value {
    if (env.directly_has(identifier)) {
      fmt::println("Variable '{}' is already declared", identifier);
      return none{};
    };

    env.vars.insert_or_assign(identifier, value);
    return value;
  }

  auto assign(env &env, const value& value) -> ast::value {
    if (not env.has(identifier)) {
      fmt::println("Variable '{}' doesn't exist", identifier);
      return none{};
    }

    auto var = env.get(identifier);

    if (not var) {
      fmt::println("'{}' isn't a variable", identifier);
      return none{};
    }

    return *var = value;
  }
};

struct function {
  virtual ~function() = default;
  virtual auto eval(env &env, std::vector<value> arguments) -> value = 0;
};

struct user_function : function {
  std::vector<ptr<struct variable>> parameters;
  statement_vector body;

  user_function() = default;
  explicit user_function(std::vector<ptr<struct variable>> parameters, statement_vector body)
    : parameters{std::move(parameters)}, body{std::move(body)} {}
  
  auto eval(env &env, std::vector<value> arguments) -> value override {
    if (parameters.size() != parameters.size()) {
      fmt::println(
        "Call to function at {} failed, expected {} args, got {}",
        fmt::ptr(this),
        parameters.size(),
        arguments.size()
      );
      return none{};
    }

    ast::env func_env = ast::env::from_parent(env);

    for (auto &&[var, value] : std::views::zip(parameters, arguments)) {
      var->declare(func_env, value);
    }

    try {
      run_statements(func_env, body);
      return none{};
    } catch (value return_value) {
      return return_value;
    }
  }
};

struct builtin_function : function {
  struct {
    std::size_t min = 0;
    std::optional<std::size_t> max = 0; // Nullopt means that there's no limit
  } argument_count;
  using FunctionType = auto(*)(env &env, std::vector<value> arguments) -> value;
  FunctionType function_body;

  explicit builtin_function(decltype(argument_count) argument_count, FunctionType function_body)
    : argument_count{argument_count}, function_body{function_body} {}

  static auto make_shared(decltype(argument_count) argument_count, FunctionType function_body) -> func_ptr {
    return std::make_shared<builtin_function>(argument_count, function_body);
  }

  auto eval(env &env, std::vector<value> arguments) -> value override {
    return function_body(env, std::move(arguments));
  }
};

struct assignment : expression {
  enum struct op_t { value };

  expr_ptr lhs;
  expr_ptr rhs;
  explicit assignment(expr_ptr lhs, op_t, expr_ptr rhs)
      : lhs{std::move(lhs)}, rhs{std::move(rhs)} {}

  auto eval(env &env) -> value override {
    auto var = std::dynamic_pointer_cast<variable>(lhs);
    if (not var) {
      fmt::println("Assignment failed, lhs is not a variable name");
      return none{};
    }

    auto value = rhs->eval(env);
    var->assign(env, value);
    return value;
  }
};

struct binop : expression {
  enum struct op_t { add, sub, mul, div };

  expr_ptr lhs, rhs;
  op_t op;

  explicit binop(expr_ptr lhs, op_t op, expr_ptr rhs)
      : lhs{std::move(lhs)}, rhs{std::move(rhs)}, op{op} {}

  auto eval(env &env) -> value override {
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

struct chained_comparison : expression {
  enum class op_t : std::uint8_t {
    lt, lte, gt, gte, eq, neq, logical_and, logical_or
  };

  std::vector<op_t> ops;
  std::vector<expr_ptr> operands;

  explicit chained_comparison() = default;
  explicit chained_comparison(expr_ptr lhs, op_t op, expr_ptr rhs)
    : ops{op}, operands{std::move(lhs), std::move(rhs)} {}

  auto eval(env &env) -> value override {
    using namespace std::views;

    static constexpr auto equals_true = std::identity{};
    auto eval_expression = std::bind_back(&expression::eval, env);

    if (ops[0] == op_t::logical_and) {
      return std::ranges::all_of(operands, equals_true, eval_expression);
    } else if (ops[0] == op_t::logical_or) {
      return std::ranges::any_of(operands, equals_true, eval_expression);
    }

    value prev = std::invoke(eval_expression, operands[0]);

    auto compare = [&prev](op_t op, const value& curr) -> bool {
      value result = none{};
      switch (op) {
        case op_t::lt:  result = prev <  curr; break;
        case op_t::lte: result = prev <= curr; break;
        case op_t::gt:  result = prev >  curr; break;
        case op_t::gte: result = prev >= curr; break;
        case op_t::eq:  result = prev == curr; break;
        case op_t::neq: result = prev != curr; break;
        default:
          std::unreachable();
      }

      prev = curr;
      return result;
    };

    // This is written specifically such that every operand is evaluated only once and such that
    // in expressions like `a() == b() == c()`, c() doesn't get evaluated if a() doesn't equal b()
    return std::ranges::all_of(
      zip_transform(compare, ops, operands | drop(1) | transform(eval_expression)),
      std::identity{}
    );
  }
};

struct prefix : expression {
  enum struct op_t { negate, complement, invert };

  expr_ptr val;
  op_t op;
  explicit prefix(op_t op, expr_ptr val) : val{std::move(val)}, op{op} {}
  auto eval(env &env) -> value override {
    switch (op) {
      using enum op_t;
    case negate: return -val->eval(env);
    case complement: return +val->eval(env);
    case invert: return !val->eval(env);
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
  auto eval(env &env) -> value override {
    auto lhs_val = callee->eval(env);
    if (not std::holds_alternative<func_ptr>(lhs_val.data)) {
      fmt::println("Call failed, lhs is not a function");
      return none{};
    }

    using std::views::transform;

    auto func = std::get<func_ptr>(lhs_val.data);
    auto evaluated_args = args
      | transform(std::bind_back(&expression::eval, env))
      | std::ranges::to<std::vector>();
    return func->eval(env, std::move(evaluated_args));
  }
};

struct func_decl : statement {
  std::string identifier;
  func_ptr body;
  explicit func_decl(std::string identifier, user_function body)
      : identifier{std::move(identifier)}, body{std::make_unique<user_function>(std::move(body))} {}

  auto exec(env &env) -> void override {
    if (env.directly_has(identifier)) {
      fmt::println("'{}' is already declared", identifier);
      return;
    };

    env.vars.insert_or_assign(identifier, body);
  }
};

struct variable_decl : statement {
  ptr<ast::variable> variable;
  expr_ptr initializer;
  explicit variable_decl(ptr<ast::variable> variable, expr_ptr initializer)
    : variable{std::move(variable)}, initializer{std::move(initializer)} {}

  auto exec(env &env) -> void override {
    variable->declare(env, initializer->eval(env));
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
    run_statements(new_env, condition->eval(env) ? if_branch : else_branch);
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

constexpr auto kw_let = LEXY_KEYWORD("let", id_rule);
constexpr auto kw_fn = LEXY_KEYWORD("fn", id_rule);
constexpr auto kw_return = LEXY_KEYWORD("return", id_rule);
constexpr auto kw_if = LEXY_KEYWORD("if", id_rule);
constexpr auto kw_else = LEXY_KEYWORD("else", id_rule);
constexpr auto kw_true = LEXY_KEYWORD("true", id_rule);
constexpr auto kw_false = LEXY_KEYWORD("false", id_rule);

struct identifier {
  static constexpr auto rule =
      id_rule.reserve(kw_let, kw_fn, kw_return, kw_if, kw_else, kw_true, kw_false);
  static constexpr auto value = lexy::as_string<std::string>;
};

struct var {
  static constexpr auto rule = dsl::p<identifier>;
  static constexpr auto value =
      lexy::new_<ast::variable, ast::ptr<ast::variable>>;
};

// Expr is separated into relational, equality and expr expression_productions
// Because lexy emits operator_group_error even when parent and child groups are on different precedence level
// So `a < b == c < d` which should get parsed as `(a < b) == (c < d)` emits and error message
// Which is not desired, so to avoid this, there should only be 1 group per expression production
struct relational : lexy::expression_production {
  static constexpr auto atom = dsl::parenthesized(dsl::recurse<struct expr>) | dsl::p<number> | dsl::p<var>;

  struct call : dsl::postfix_op {
    static constexpr auto op = dsl::op<ast::postfix::op_t::value>(
        dsl::parenthesized.opt(dsl::recurse<struct expr_list>));
    using operand = dsl::atom;
  };

  struct prefix : dsl::prefix_op {
    static constexpr auto op =
      dsl::op<ast::prefix::op_t::complement>(dsl::lit_c<'+'>)
      / dsl::op<ast::prefix::op_t::negate>(dsl::lit_c<'-'>)
      / dsl::op<ast::prefix::op_t::invert>(dsl::lit_c<'!'>);
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

  struct less_than : dsl::infix_op_list {
    static constexpr auto op =
      dsl::op<ast::chained_comparison::op_t::lt>(dsl::not_followed_by(LEXY_LIT("<"), dsl::lit_c<'='>))
      / dsl::op<ast::chained_comparison::op_t::lte>(LEXY_LIT("<="));
    using operand = addition;
  };

  struct greater_than : dsl::infix_op_list {
    static constexpr auto op =
      dsl::op<ast::chained_comparison::op_t::gt>(dsl::not_followed_by(LEXY_LIT(">"), dsl::lit_c<'='>))
      / dsl::op<ast::chained_comparison::op_t::gte>(LEXY_LIT(">="));
    using operand = addition;
  };

  using operation = dsl::groups<less_than, greater_than>;  
  
  static constexpr auto value = lexy::fold_inplace<std::unique_ptr<ast::chained_comparison>>(
    [] { return std::make_unique<ast::chained_comparison>(); },
    [](auto& node, ast::expr_ptr operand) { node->operands.emplace_back(LEXY_MOV(operand)); },
    [](auto& node, ast::chained_comparison::op_t op) { node->ops.emplace_back(op); }
  ) >> lexy::callback<ast::expr_ptr>(
    lexy::forward<ast::expr_ptr>,
    lexy::new_<ast::chained_comparison, ast::expr_ptr>,
    lexy::new_<ast::binop, ast::expr_ptr>,
    lexy::new_<ast::prefix, ast::expr_ptr>,
    lexy::new_<ast::postfix, ast::expr_ptr>
  );
};

struct equality : lexy::expression_production {
  static constexpr auto atom = dsl::p<relational>;

  struct equal : dsl::infix_op_list {
    static constexpr auto op = dsl::op<ast::chained_comparison::op_t::eq>(LEXY_LIT("=="));
    using operand = dsl::atom;
  };

  // Not chainable by design
  struct not_equal : dsl::infix_op_single {
    static constexpr auto op = dsl::op<ast::chained_comparison::op_t::neq>(LEXY_LIT("!="));
    using operand = dsl::atom;
  };

  using operation = dsl::groups<equal, not_equal>;

  static constexpr auto value = lexy::fold_inplace<std::unique_ptr<ast::chained_comparison>>(
    [] { return std::make_unique<ast::chained_comparison>(); },
    [](auto& node, ast::expr_ptr operand) { node->operands.emplace_back(LEXY_MOV(operand)); },
    [](auto& node, ast::chained_comparison::op_t op) { node->ops.emplace_back(op); }
  ) >> lexy::callback<ast::expr_ptr>(
    lexy::forward<ast::expr_ptr>,
    lexy::new_<ast::chained_comparison, ast::expr_ptr>
  );
};

struct expr : lexy::expression_production {
  static constexpr auto atom = dsl::p<equality>;

  struct log_or : dsl::infix_op_list {
    static constexpr auto op = dsl::op<ast::chained_comparison::op_t::logical_or>(LEXY_LIT("||"));
    using operand = dsl::atom;
  };

  struct log_and : dsl::infix_op_list {
    static constexpr auto op = dsl::op<ast::chained_comparison::op_t::logical_and>(LEXY_LIT("&&"));
    using operand = dsl::atom;
  };

  using logical = dsl::groups<log_or, log_and>;

  struct assignment : dsl::infix_op_single {
    static constexpr auto op =
        dsl::op<ast::assignment::op_t::value>(dsl::not_followed_by(dsl::equal_sign, dsl::equal_sign));
    using operand = logical;
  };

  using operation = assignment;

  static constexpr auto value = lexy::fold_inplace<std::unique_ptr<ast::chained_comparison>>(
    [] { return std::make_unique<ast::chained_comparison>(); },
    [](auto& node, ast::expr_ptr operand) { node->operands.emplace_back(LEXY_MOV(operand)); },
    [](auto& node, ast::chained_comparison::op_t op) { node->ops.emplace_back(op); }
  ) >> lexy::callback<ast::expr_ptr>(
    lexy::forward<ast::expr_ptr>,
    lexy::new_<ast::assignment, ast::expr_ptr>
  );
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
  static constexpr auto value = lexy::construct<ast::user_function>;
};

struct function_declaration {
  static constexpr auto rule =
      kw_fn >> dsl::p<identifier> + dsl::p<function_body>;
  static constexpr auto value = lexy::new_<ast::func_decl, ast::statement_ptr>;
};

struct variable_declaration {
  static constexpr auto rule = kw_let >> dsl::p<var> + dsl::equal_sign + dsl::p<expr>;
  static constexpr auto value = lexy::new_<ast::variable_decl, ast::statement_ptr>;
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
  static constexpr auto rule = dsl::opt(kw_else >> (dsl::p<scope_declaration> | dsl::else_ >> dsl::recurse<struct if_statement>));
  static constexpr auto value = lexy::callback<ast::statement_vector>(
    [](lexy::nullopt) -> ast::statement_vector { return {}; },
    lexy::forward<ast::statement_vector>,
    [](ast::statement_ptr if_stmt) -> ast::statement_vector { return {if_stmt}; }
  );
};

struct if_statement {
  static constexpr auto
      rule = kw_if >> dsl::p<expr>
        + dsl::p<scope_declaration> + dsl::p<else_clause>;

  static constexpr auto value = lexy::new_<ast::if_statement, ast::statement_ptr>;
};

struct statement {
  static constexpr auto rule = dsl::p<variable_declaration> | dsl::p<function_declaration> | dsl::p<return_statement>
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