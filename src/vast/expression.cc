#include "vast/expression.h"

#include <boost/variant/apply_visitor.hpp>
#include "vast/logger.h"
#include "vast/optional.h"
#include "vast/regex.h"
#include "vast/serialization.h"
#include "vast/detail/ast/query.h"
#include "vast/detail/parser/error_handler.h"
#include "vast/detail/parser/skipper.h"
#include "vast/detail/parser/query.h"
#include "vast/util/convert.h"
#include "vast/util/make_unique.h"

namespace vast {
namespace expr {

bool operator==(node const& x, node const& y)
{
  return x.equals(y);
}

bool operator<(node const& x, node const& y)
{
  return x.is_less_than(y);
}

constant::constant(value v)
  : val(std::move(v))
{
}

constant* constant::clone() const
{
  return new constant{*this};
}

bool constant::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;
  return val == static_cast<constant const&>(other).val;
}

bool constant::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();
  return val < static_cast<constant const&>(other).val;
}

void constant::serialize(serializer& sink) const
{
  sink << val;
}

void constant::deserialize(deserializer& source)
{
  source >> val;
}

bool extractor::equals(node const& other) const
{
  return (typeid(*this) == typeid(other));
}

bool extractor::is_less_than(node const& other) const
{
  return typeid(*this).hash_code() < typeid(other).hash_code();
}


timestamp_extractor* timestamp_extractor::clone() const
{
  return new timestamp_extractor{*this};
}

void timestamp_extractor::serialize(serializer&) const
{
}

void timestamp_extractor::deserialize(deserializer&)
{
}


name_extractor* name_extractor::clone() const
{
  return new name_extractor{*this};
}

void name_extractor::serialize(serializer&) const
{
}

void name_extractor::deserialize(deserializer&)
{
}


id_extractor* id_extractor::clone() const
{
  return new id_extractor{*this};
}

void id_extractor::serialize(serializer&) const
{
}

void id_extractor::deserialize(deserializer&)
{
}


offset_extractor::offset_extractor(string event, offset off)
  : event{std::move(event)},
    off{std::move(off)}
{
}

offset_extractor* offset_extractor::clone() const
{
  return new offset_extractor{*this};
}

void offset_extractor::serialize(serializer& sink) const
{
  sink << event << off;
}

void offset_extractor::deserialize(deserializer& source)
{
  source >> event >> off;
}

bool offset_extractor::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;

  auto& that = static_cast<offset_extractor const&>(other);
  return event == that.event && off == that.off;
}

bool offset_extractor::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();

  auto& that = static_cast<offset_extractor const&>(other);
  return std::tie(event, off) < std::tie(that.event, that.off);
}


type_extractor::type_extractor(value_type t)
  : type{t}
{
}

type_extractor* type_extractor::clone() const
{
  return new type_extractor{*this};
}

bool type_extractor::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;
  return type == static_cast<type_extractor const&>(other).type;
}

bool type_extractor::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();
  return type < static_cast<type_extractor const&>(other).type;
}

void type_extractor::serialize(serializer& sink) const
{
  sink << type;
}

void type_extractor::deserialize(deserializer& source)
{
  source >> type;
}


n_ary_operator::n_ary_operator(n_ary_operator const& other)
{
  for (auto& o : other.operands)
    operands.emplace_back(o->clone());
}

bool n_ary_operator::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;
  auto& that = static_cast<n_ary_operator const&>(other);
  if (operands.size() != that.operands.size())
    return false;
  for (size_t i = 0; i < operands.size(); ++i)
    if (*operands[i] != *that.operands[i])
      return false;
  return true;
}

bool n_ary_operator::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();
  auto& that = static_cast<n_ary_operator const&>(other);
  return std::lexicographical_compare(
      operands.begin(), operands.end(),
      that.operands.begin(), that.operands.end(),
      [](std::unique_ptr<node> const& x, std::unique_ptr<node> const& y)
      {
        return *x < *y;
      });
}

void n_ary_operator::serialize(serializer& sink) const
{
  sink << operands;
}

void n_ary_operator::deserialize(deserializer& source)
{
  source >> operands;
}

void n_ary_operator::add(std::unique_ptr<node> n)
{
  operands.push_back(std::move(n));
}

namespace {

struct match_visitor
{
  using result_type = bool;

  result_type operator()(string const& lhs, regex const& rhs) const
  {
    return rhs.match(lhs);
  }

  template <typename T, typename U>
  result_type operator()(T const&, U const&) const
  {
    return false;
  }
};

struct in_visitor
{
  using result_type = bool;

  result_type operator()(string const& lhs, string const& rhs) const
  {
    return rhs.find(lhs);
  }

  result_type operator()(string const& lhs, regex const& rhs) const
  {
    return rhs.search(lhs);
  }

  result_type operator()(address const& lhs, prefix const& rhs) const
  {
    return rhs.contains(lhs);
  }

  template <typename T, typename U>
  result_type operator()(T const&, U const&) const
  {
    return false;
  }
};

struct ni_visitor
{
  using result_type = bool;

  result_type operator()(string const& lhs, string const& rhs) const
  {
    return lhs.find(rhs);
  }

  result_type operator()(regex const& lhs, string const& rhs) const
  {
    return lhs.search(rhs);
  }

  result_type operator()(prefix const& lhs, address const& rhs) const
  {
    return lhs.contains(rhs);
  }

  template <typename T, typename U>
  result_type operator()(T const&, U const&) const
  {
    return false;
  }
};

} // namespace <anonymous>

predicate::binary_predicate predicate::make_predicate(relational_operator op)
{
  switch (op)
  {
    default:
      assert(! "invalid operator");
      return {};
    case match:
      return [](value const& lhs, value const& rhs) -> bool
      {
        return value::visit(lhs, rhs, match_visitor{});
      };
    case not_match:
      return [](value const& lhs, value const& rhs) -> bool
      {
        return ! value::visit(lhs, rhs, match_visitor{});
      };
    case in:
      return [](value const& lhs, value const& rhs) -> bool
      {
        return value::visit(lhs, rhs, in_visitor{});
      };
    case not_in:
      return [](value const& lhs, value const& rhs) -> bool
      {
        return ! value::visit(lhs, rhs, in_visitor{});
      };
    case ni:
      return [](value const& lhs, value const& rhs) -> bool
      {
        return value::visit(lhs, rhs, ni_visitor{});
      };
    case not_ni:
      return [](value const& lhs, value const& rhs) -> bool
      {
        return ! value::visit(lhs, rhs, ni_visitor{});
      };
    case equal:
      return [](value const& lhs, value const& rhs)
      {
        return lhs == rhs;
      };
    case not_equal:
      return [](value const& lhs, value const& rhs)
      {
        return lhs != rhs;
      };
    case less:
      return [](value const& lhs, value const& rhs)
      {
        return lhs < rhs;
      };
    case less_equal:
      return [](value const& lhs, value const& rhs)
      {
        return lhs <= rhs;
      };
    case greater:
      return [](value const& lhs, value const& rhs)
      {
        return lhs > rhs;
      };
    case greater_equal:
      return [](value const& lhs, value const& rhs)
      {
        return lhs >= rhs;
      };
  }
}

predicate::predicate(relational_operator op)
  : op{op}
{
  pred = make_predicate(op);
}

node const& predicate::lhs() const
{
  assert(operands.size() == 2);
  assert(operands[0]);
  return *operands[0];
}

node const& predicate::rhs() const
{
  assert(operands.size() == 2);
  assert(operands[1]);
  return *operands[1];
}

predicate* predicate::clone() const
{
  return new predicate{*this};
}

bool predicate::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;
  return op == static_cast<predicate const&>(other).op
      && n_ary_operator::equals(other);
}

bool predicate::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();
  auto& that = static_cast<predicate const&>(other);
  return op != that.op
    ? op < that.op
    : std::lexicographical_compare(
          operands.begin(), operands.end(),
          that.operands.begin(), that.operands.end(),
          [](std::unique_ptr<node> const& x, std::unique_ptr<node> const& y)
          {
            return *x < *y;
          });
}

void predicate::serialize(serializer& sink) const
{
  n_ary_operator::serialize(sink);
  sink << op;
}

void predicate::deserialize(deserializer& source)
{
  n_ary_operator::deserialize(source);
  source >> op;
  pred = make_predicate(op);
}

conjunction* conjunction::clone() const
{
  return new conjunction{*this};
}

bool conjunction::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;
  return n_ary_operator::equals(other);
}

bool conjunction::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();
  auto& that = static_cast<n_ary_operator const&>(other);
  return std::lexicographical_compare(
      operands.begin(), operands.end(),
      that.operands.begin(), that.operands.end(),
      [](std::unique_ptr<node> const& x, std::unique_ptr<node> const& y)
      {
        return *x < *y;
      });
}

void conjunction::serialize(serializer& sink) const
{
  n_ary_operator::serialize(sink);
}

void conjunction::deserialize(deserializer& source)
{
  n_ary_operator::deserialize(source);
}


disjunction* disjunction::clone() const
{
  return new disjunction{*this};
}

bool disjunction::equals(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return false;
  return n_ary_operator::equals(other);
}

bool disjunction::is_less_than(node const& other) const
{
  if (typeid(*this) != typeid(other))
    return typeid(*this).hash_code() < typeid(other).hash_code();
  auto& that = static_cast<n_ary_operator const&>(other);
  return std::lexicographical_compare(
      operands.begin(), operands.end(),
      that.operands.begin(), that.operands.end(),
      [](std::unique_ptr<node> const& x, std::unique_ptr<node> const& y)
      {
        return *x < *y;
      });
}

void disjunction::serialize(serializer& sink) const
{
  n_ary_operator::serialize(sink);
}

void disjunction::deserialize(deserializer& source)
{
  n_ary_operator::deserialize(source);
}

namespace {

/// Takes a query AST and generates a polymorphic query expression tree.
class expressionizer
{
public:
  using result_type = void;

  static trial<std::unique_ptr<node>>
  apply(detail::ast::query::query const& q, schema const& sch)
  {
    std::unique_ptr<n_ary_operator> root;

    if (q.rest.empty())
    {
      // WLOG, we can always add a conjunction as parent if we just have a
      // single predicate.
      root = make_unique<conjunction>();

      expressionizer visitor{root.get(), sch};
      boost::apply_visitor(std::ref(visitor), q.first);
      if (visitor.error_)
        return std::move(*visitor.error_);

      return {std::move(root)}; // FIXME: add to parent.
    }

    // First, split the query expression at each OR node.
    std::vector<detail::ast::query::query> ors;
    ors.push_back({q.first, {}});
    for (auto& pred : q.rest)
      if (pred.op == logical_or)
        ors.push_back({pred.operand, {}});
      else
        ors.back().rest.push_back(pred);

    // Our AST root will be a disjunction iff we have at least two terms.
    if (ors.size() >= 2)
      root = make_unique<disjunction>();

    // Then create a conjunction for each set of subsequent AND nodes between
    // two OR nodes.
    std::unique_ptr<conjunction> conj;
    for (auto& ands : ors)
    {
      n_ary_operator* local_root;
      if (! root)
      {
        root = make_unique<conjunction>();
        local_root = root.get();
      }
      else if (! ands.rest.empty())
      {
        auto conj = make_unique<conjunction>();
        local_root = conj.get();
        root->add(std::move(conj));
      }
      else
      {
        local_root = root.get();
      }

      expressionizer visitor{local_root, sch};
      boost::apply_visitor(std::ref(visitor), ands.first);
      if (visitor.error_)
        return std::move(*visitor.error_);
      for (auto pred : ands.rest)
      {
        boost::apply_visitor(std::ref(visitor), pred.operand);
        if (visitor.error_)
          return std::move(*visitor.error_);
      }
    }

    return {std::move(root)};
  }

  expressionizer(n_ary_operator* parent, schema const& sch)
    : parent_(parent),
      schema_(sch)
  {
  }

  void operator()(detail::ast::query::query const& q)
  {
    auto n = apply(q, schema_);
    if (n)
      parent_->add(std::move(*n));
    else
      error_ = n.failure();
  }

  void operator()(detail::ast::query::predicate const& operand)
  {
    boost::apply_visitor(*this, operand);
  }

  void operator()(detail::ast::query::tag_predicate const& pred)
  {
    auto op = pred.op;
    if (invert_)
    {
      op = negate(op);
      invert_ = false;
    }

    std::unique_ptr<extractor> lhs;
    if (pred.lhs == "name")
      lhs = make_unique<name_extractor>();
    else if (pred.lhs == "time")
      lhs = make_unique<timestamp_extractor>();
    else if (pred.lhs == "id")
      lhs = make_unique<id_extractor>();

    auto rhs = make_unique<constant>(detail::ast::query::fold(pred.rhs));
    auto p = make_unique<predicate>(op);
    p->add(std::move(lhs));
    p->add(std::move(rhs));

    parent_->add(std::move(p));
  }

  void operator()(detail::ast::query::type_predicate const& pred)
  {
    auto op = pred.op;
    if (invert_)
    {
      op = negate(op);
      invert_ = false;
    }

    auto lhs = make_unique<type_extractor>(pred.lhs);
    auto rhs = make_unique<constant>(detail::ast::query::fold(pred.rhs));
    auto p = make_unique<predicate>(op);
    p->add(std::move(lhs));
    p->add(std::move(rhs));

    parent_->add(std::move(p));
  }

  void operator()(detail::ast::query::offset_predicate const& pred)
  {
    auto op = pred.op;
    if (invert_)
    {
      op = negate(op);
      invert_ = false;
    }

    auto lhs = make_offset_extractor(pred.event, pred.off);
    auto rhs = make_unique<constant>(detail::ast::query::fold(pred.rhs));
    auto p = make_unique<predicate>(op);
    p->add(std::move(lhs));
    p->add(std::move(rhs));

    parent_->add(std::move(p));
  }

  void operator()(detail::ast::query::schema_predicate const& pred)
  {
    auto op = pred.op;
    if (invert_)
    {
      op = negate(op);
      invert_ = false;
    }

    std::vector<string> ids;
    for (auto& str : pred.lhs)
      ids.emplace_back(str);

    auto offs = schema_.find_offsets(ids);
    if (offs.empty())
    {
      error_ = error{"invalid argument name sequence"};
      return;
    }

    // Ensure that all types are compatible to each other.
    std::vector<type_ptr> types;
    for (auto& p : offs)
      types.push_back(schema_.find_type(p.first, p.second));

    assert(! types.empty());
    for (auto& t : types)
      if (! t->represents(types[0]))
      {
        error_ =
          error{"type clash: " + to_string(*t) + " <> " + to_string(*types[0])};
        return;
      }

    auto disj = make_unique<disjunction>();
    for (auto& p : offs)
    {
      auto pr = make_unique<predicate>(op);
      auto lhs = make_offset_extractor(p.first, p.second);
      auto rhs = make_constant(pred.rhs);
      pr->add(std::move(lhs));
      pr->add(std::move(rhs));
      disj->add(std::move(pr));
    }

    parent_->add(std::move(disj));
  }

  void operator()(detail::ast::query::negated_predicate const& pred)
  {
    // Since all operators have a complement, we can push down the negation to
    // the operator-level (as opposed to leaving it at the predicate level).
    invert_ = true;
    boost::apply_visitor(*this, pred.operand);
  }

private:
  std::unique_ptr<offset_extractor> make_offset_extractor(string e, offset o)
  {
    return make_unique<offset_extractor>(std::move(e), std::move(o));
  }

  std::unique_ptr<constant>
  make_constant(detail::ast::query::value_expr const& expr)
  {
    return make_unique<constant>(detail::ast::query::fold(expr));
  }

  std::unique_ptr<node> make_glob_node(std::string const& expr)
  {
    // Determine whether we need a regular expression node or whether basic
    // equality comparison suffices. This check is relatively crude at the
    // moment: we just look whether the expression contains * or ?.
    auto glob = regex("\\*|\\?").search(expr);
    auto p = make_unique<predicate>(glob ? match : equal);
    auto lhs = make_unique<name_extractor>();
    p->add(std::move(lhs));
    if (glob)
      p->add(make_unique<constant>(regex::glob(expr)));
    else
      p->add(make_unique<constant>(expr));
    return std::move(p);
  }

  n_ary_operator* parent_;
  schema const& schema_;
  bool invert_ = false;
  optional<error> error_;
};

} // namespace <anonymous>

trial<ast> ast::parse(std::string const& str, schema const& sch)
{
  if (str.empty())
    return error{"cannot create AST from empty string"};

  auto i = str.begin();
  auto end = str.end();
  using iterator = std::string::const_iterator;
  std::string err;
  detail::parser::error_handler<iterator> on_error{i, end, err};
  detail::parser::query<iterator> grammar{on_error};
  detail::parser::skipper<iterator> skipper;
  detail::ast::query::query q;
  bool success = phrase_parse(i, end, grammar, skipper, q);
  if (! success || i != end)
    return error{std::move(err)};

  if (! detail::ast::query::validate(q))
    return error{"failed validation"};

  auto n = expressionizer::apply(q, sch);
  if (n)
    return {std::move(*n)};
  else
    return n.failure();
}

ast::ast(std::string const& str, schema const& sch)
{
  if (auto a = parse(str, sch))
    *this = std::move(*a);
}

ast::ast(node const& n)
  : ast{std::unique_ptr<node>{n.clone()}}
{
}

ast::ast(std::unique_ptr<node> n)
  : node_{std::move(n)}
{
}

ast::ast(ast const& other)
  : node_{other.node_ ? other.node_->clone() : nullptr}
{
}

ast& ast::operator=(ast const& other)
{
  node_.reset(other.node_ ? other.node_->clone() : nullptr);
  return *this;
}

ast& ast::operator=(ast&& other)
{
  node_ = std::move(other.node_);
  return *this;
}

ast::operator bool() const
{
  return node_ != nullptr;
}

void ast::accept(const_visitor& v)
{
  if (node_)
    node_->accept(v);
}

void ast::accept(const_visitor& v) const
{
  if (node_)
    node_->accept(v);
}

node const* ast::root() const
{
  return node_ ? node_.get() : nullptr;
}

namespace {

struct conjunction_tester : public expr::default_const_visitor
{
  virtual void visit(expr::conjunction const&)
  {
    flag_ = true;
  }

  bool flag_ = false;
};

struct disjunction_tester : public expr::default_const_visitor
{
  virtual void visit(expr::disjunction const&)
  {
    flag_ = true;
  }

  bool flag_ = false;
};

struct predicate_tester : public expr::default_const_visitor
{
  virtual void visit(expr::predicate const&)
  {
    flag_ = true;
  }

  bool flag_ = false;
};

class meta_predicate_tester : public expr::default_const_visitor
{
public:
  virtual void visit(expr::predicate const& pred)
  {
    pred.lhs().accept(*this);
  }

  virtual void visit(expr::name_extractor const&)
  {
    flag_ = true;
  }

  virtual void visit(expr::timestamp_extractor const&)
  {
    flag_ = true;
  }

  virtual void visit(expr::id_extractor const&)
  {
    flag_ = true;
  }

  bool flag_ = false;
};

class time_predicate_tester : public expr::default_const_visitor
{
public:
  virtual void visit(expr::predicate const& pred)
  {
    pred.lhs().accept(*this);
  }

  virtual void visit(expr::timestamp_extractor const&)
  {
    flag_ = true;
  }

  bool flag_ = false;
};

class name_predicate_tester : public expr::default_const_visitor
{
public:
  virtual void visit(expr::predicate const& pred)
  {
    pred.lhs().accept(*this);
  }

  virtual void visit(expr::name_extractor const&)
  {
    flag_ = true;
  }

  bool flag_ = false;
};

class constant_finder : public expr::default_const_visitor
{
public:
  virtual void visit(expr::predicate const& pred)
  {
    pred.rhs().accept(*this);
  }

  virtual void visit(expr::constant const& c)
  {
    val_ = &c.val;
  }

  value const* val_ = nullptr;
};

class offset_finder : public expr::default_const_visitor
{
public:
  virtual void visit(expr::predicate const& pred)
  {
    pred.lhs().accept(*this);
  }

  virtual void visit(expr::offset_extractor const& oe)
  {
    off_ = &oe.off;
  }

  offset const* off_ = nullptr;
};

class operator_finder : public expr::default_const_visitor
{
public:
  virtual void visit(expr::predicate const& pred)
  {
    op_ = &pred.op;
  }

  relational_operator const* op_ = nullptr;
};

} // namespace <anonymous>

bool ast::is_conjunction() const
{
  conjunction_tester visitor;
  accept(visitor);
  return visitor.flag_;
}

bool ast::is_disjunction() const
{
  disjunction_tester visitor;
  accept(visitor);
  return visitor.flag_;
}

bool ast::is_predicate() const
{
  predicate_tester visitor;
  accept(visitor);
  return visitor.flag_;
}

bool ast::is_meta_predicate() const
{
  meta_predicate_tester visitor;
  accept(visitor);
  return visitor.flag_;
}

bool ast::is_time_predicate() const
{
  time_predicate_tester visitor;
  accept(visitor);
  return visitor.flag_;
}

bool ast::is_name_predicate() const
{
  name_predicate_tester visitor;
  accept(visitor);
  return visitor.flag_;
}

value const* ast::find_constant() const
{
  constant_finder visitor;
  accept(visitor);
  return visitor.val_;
}

offset const* ast::find_offset() const
{
  offset_finder visitor;
  accept(visitor);
  return visitor.off_;
}

relational_operator const* ast::find_operator() const
{
  operator_finder visitor;
  accept(visitor);
  return visitor.op_;
}

void ast::serialize(serializer& sink) const
{
  if (node_)
  {
    sink << true;
    sink << node_;
  }
  else
  {
    sink << false;
  }
}

void ast::deserialize(deserializer& source)
{
  bool valid;
  source >> valid;
  if (valid)
    source >> node_;
}

bool ast::convert(std::string& str, bool tree) const
{
  if (node_)
    return expr::convert(*node_, str, tree);
  str = "";
  return true;
}

bool operator==(ast const& x, ast const& y)
{
  return x.node_ && y.node_ ? *x.node_ == *y.node_ : false;
}

bool operator<(ast const& x, ast const& y)
{
  return x.node_ && y.node_ ? *x.node_ < *y.node_ : false;
}


namespace {

class evaluator : public const_visitor
{
public:
  evaluator(event const& e)
    : event_{e}
  {
  }

  value const& result() const
  {
    return result_;
  }

  virtual void visit(constant const& c)
  {
    result_ = c.val;
  }

  virtual void visit(timestamp_extractor const&)
  {
    result_ = event_.timestamp();
  }

  virtual void visit(name_extractor const&)
  {
    result_ = event_.name();
  }

  virtual void visit(id_extractor const&)
  {
    result_ = event_.id();
  }

  virtual void visit(offset_extractor const& o)
  {
    auto v = event_.at(o.off);
    result_ = v ? *v : invalid;
  }

  virtual void visit(type_extractor const& t)
  {
    if (! extractor_state_)
    {
      extractor_state_ = extractor_state{};
      extractor_state_->pos.emplace_back(&event_, 0);
    }
    result_ = invalid;
    while (! extractor_state_->pos.empty())
    {
      auto& rec = *extractor_state_->pos.back().first;
      auto& idx = extractor_state_->pos.back().second;
      if (idx == rec.size())
      {
        // Out of bounds.
        extractor_state_->pos.pop_back();
        continue;
      }
      auto& arg = rec[idx++];
      if (extractor_state_->pos.size() == 1 && idx == rec.size())
        extractor_state_->complete = true; // Finished with top-most record.
      if (! arg)
        continue;
      if (arg.which() == record_value)
      {
        extractor_state_->pos.emplace_back(&arg.get<record>(), 0);
        continue;
      }
      if (arg.which() == t.type)
      {
        result_ = arg;
        break;
      }
    }
  }

  virtual void visit(predicate const& p)
  {
    bool result = false;
    do
    {
      p.lhs().accept(*this);
      auto lhs = result_;
      p.rhs().accept(*this);
      auto& rhs = result_;
      result = p.pred(lhs, rhs);
      if (result)
        break;
    }
    while (extractor_state_ && ! extractor_state_->complete);
    if (extractor_state_)
      extractor_state_ = {};
    result_ = result;
  }

  virtual void visit(conjunction const& c)
  {
    result_ = std::all_of(
        c.operands.begin(),
        c.operands.end(),
        [&](std::unique_ptr<node> const& operand) -> bool
        {
          operand->accept(*this);
          assert(result_ && result_.which() == bool_value);
          return result_.get<bool>();
        });
  }

  virtual void visit(disjunction const& d)
  {
    result_ = std::any_of(
        d.operands.begin(),
        d.operands.end(),
        [&](std::unique_ptr<node> const& operand) -> bool
        {
          operand->accept(*this);
          assert(result_ && result_.which() == bool_value);
          return result_.get<bool>();
        });
  }

private:
  struct extractor_state
  {
    std::vector<std::pair<record const*, size_t>> pos;
    bool complete = false; // Flag that indicates whether the type extractor
                           // has gone through all values with the given type.
  };

  event const& event_;
  value result_;
  optional<extractor_state> extractor_state_;
};

} // namespace <anonymous>

value evaluate(node const& n, event const& e)
{
  evaluator v{e};
  n.accept(v);
  return v.result();
}

value evaluate(ast const& a, event const& e)
{
  return a.root() ? evaluate(*a.root(), e) : invalid;
}

namespace {

class tree_printer : public const_visitor
{
public:
  tree_printer(std::string& str)
    : str_(str)
  {
  }

  virtual void visit(constant const& c)
  {
    indent();
    str_ += to<std::string>(c.val) + '\n';
  }

  virtual void visit(timestamp_extractor const&)
  {
    indent();
    str_ += "&time\n";
  }

  virtual void visit(name_extractor const&)
  {
    indent();
    str_ += "&name\n";
  }

  virtual void visit(id_extractor const&)
  {
    indent();
    str_ += "&id\n";
  }

  virtual void visit(offset_extractor const& o)
  {
    indent();
    str_ += to<std::string>(o.event);
    str_ += '@';
    auto first = o.off.begin();
    auto last = o.off.end();
    while (first != last)
    {
      str_ += to<std::string>(*first);
      if (++first != last)
        str_ += ",";
    }
    str_ += '\n';
  }

  virtual void visit(type_extractor const& t)
  {
    indent();
    str_ += "type(";
    str_ += to<std::string>(t.type);
    str_ += ")\n";
  }

  virtual void visit(predicate const& p)
  {
    indent();
    switch (p.op)
    {
      default:
        assert(! "invalid operator type");
        break;
      case match:
        str_ += "~";
        break;
      case not_match:
        str_ += "!~";
        break;
      case in:
        str_ += "in";
        break;
      case not_in:
        str_ += "!in";
        break;
      case ni:
        str_ += "ni";
        break;
      case not_ni:
        str_ += "!ni";
        break;
      case equal:
        str_ += "==";
        break;
      case not_equal:
        str_ += "!=";
        break;
      case less:
        str_ += "<";
        break;
      case less_equal:
        str_ += "<=";
        break;
      case greater:
        str_ += ">";
        break;
      case greater_equal:
        str_ += ">=";
        break;
    }
    str_ += '\n';

    ++depth_;
    p.lhs().accept(*this);
    p.rhs().accept(*this);
    --depth_;
  }

  virtual void visit(conjunction const& conj)
  {
    indent();
    str_ += "&&\n";
    ++depth_;
    for (auto& op : conj.operands)
      op->accept(*this);
    --depth_;
  }

  virtual void visit(disjunction const& disj)
  {
    indent();
    str_ += "||\n";
    ++depth_;
    for (auto& op : disj.operands)
      op->accept(*this);
    --depth_;
  }

private:
  void indent()
  {
    str_ += std::string(depth_ * 2, indent_);
  }

  unsigned depth_ = 0;
  char indent_ = ' ';
  std::string& str_;
};

class expr_printer : public const_visitor
{
public:
  expr_printer(std::string& str)
    : str_(str)
  {
  }

  virtual void visit(constant const& c)
  {
    str_ += to<std::string>(c.val);
  }

  virtual void visit(timestamp_extractor const&)
  {
    str_ += "&time";
  }

  virtual void visit(name_extractor const&)
  {
    str_ += "&name";
  }

  virtual void visit(id_extractor const&)
  {
    str_ += "&id";
  }

  virtual void visit(offset_extractor const& o)
  {
    str_ += to<std::string>(o.event) + '@' + to<std::string>(o.off);
  }

  virtual void visit(type_extractor const& t)
  {
    str_ += ':' + to<std::string>(t.type);
  }

  virtual void visit(predicate const& p)
  {
    p.lhs().accept(*this);
    str_ += ' ';
    switch (p.op)
    {
      default:
        assert(! "invalid operator type");
        break;
      case match:
        str_ += "~";
        break;
      case not_match:
        str_ += "!~";
        break;
      case in:
        str_ += "in";
        break;
      case not_in:
        str_ += "!in";
        break;
      case ni:
        str_ += "ni";
        break;
      case not_ni:
        str_ += "!ni";
        break;
      case equal:
        str_ += "==";
        break;
      case not_equal:
        str_ += "!=";
        break;
      case less:
        str_ += "<";
        break;
      case less_equal:
        str_ += "<=";
        break;
      case greater:
        str_ += ">";
        break;
      case greater_equal:
        str_ += ">=";
        break;
    }
    str_ += ' ';
    p.rhs().accept(*this);
  }

  virtual void visit(conjunction const& conj)
  {
    auto singular = conj.operands.size() == 1;
    if (singular)
      str_ += '{';
    for (size_t i = 0; i < conj.operands.size(); ++i)
    {
      conj.operands[i]->accept(*this);
      if (i + 1 != conj.operands.size())
        str_ += " && ";
    }
    if (singular)
      str_ += '}';
  }

  virtual void visit(disjunction const& disj)
  {
    auto singular = disj.operands.size() == 1;
    if (singular)
      str_ += '[';
    for (size_t i = 0; i < disj.operands.size(); ++i)
    {
      disj.operands[i]->accept(*this);
      if (i + 1 != disj.operands.size())
        str_ += " || ";
    }
    if (singular)
      str_ += ']';
  }

private:
  std::string& str_;
};

class predicator : public expr::default_const_visitor
{
public:
  predicator(std::vector<expr::ast>& predicates)
    : predicates_{predicates}
  {
  }

  virtual void visit(expr::conjunction const& conj)
  {
    for (auto& op : conj.operands)
      op->accept(*this);
  }

  virtual void visit(expr::disjunction const& disj)
  {
    for (auto& op : disj.operands)
      op->accept(*this);
  }

  virtual void visit(expr::predicate const& pred)
  {
    predicates_.emplace_back(pred);
  }

private:
  std::vector<expr::ast>& predicates_;
};

} // namespace <anonymous>

std::vector<ast> predicatize(ast const& a)
{
  std::vector<ast> predicates;
  predicator p{predicates};
  a.accept(p);
  return predicates;
}


bool convert(node const& n, std::string& str, bool tree)
{
  str.clear();
  if (tree)
  {
    tree_printer v{str};
    n.accept(v);
  }
  else
  {
    expr_printer v{str};
    n.accept(v);
  }
  return true;
}

} // namespace expr
} // namespace vast
