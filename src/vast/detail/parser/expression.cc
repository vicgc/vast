#include "vast/detail/parser/expression_definition.h"

typedef std::string::const_iterator iterator_type;
template struct vast::detail::parser::value_expression<iterator_type>;
