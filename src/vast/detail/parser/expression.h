#ifndef VAST_DETAIL_PARSER_EXPRESSION_H
#define VAST_DETAIL_PARSER_EXPRESSION_H

#include "vast/detail/ast/query.h"
#include "vast/detail/parser/boost.h"
#include "vast/detail/parser/error_handler.h"
#include "vast/detail/parser/skipper.h"
#include "vast/detail/parser/value.h"

namespace vast {
namespace detail {
namespace parser {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator>
struct value_expression
  : qi::grammar<Iterator, ast::query::value_expr(), skipper<Iterator>>
{
    value_expression(error_handler<Iterator>& on_error);

    qi::rule<Iterator, ast::query::value_expr(), skipper<Iterator>>
        expr;

    qi::rule<Iterator, ast::query::expr_operand(), skipper<Iterator>>
        unary, primary;

    qi::rule<Iterator, std::string(), skipper<Iterator>>
        identifier;

    qi::symbols<char, arithmetic_operator>
        unary_op, binary_op;

    value<Iterator> val;
};

} // namespace ast
} // namespace detail
} // namespace vast

#endif
