# Nested records with local scope.

event foo(r: record { a: addr, i: record { b: bool } })
event bar(r: record { s: record { t: record { x: int } } })
