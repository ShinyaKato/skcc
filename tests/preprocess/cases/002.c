#define object1 1 + 2
object1 // 1 + 2

#define object2 (object1) * 3
object2 // (1 + 2) * 3

#define object3 object2 + object1
object3 // (1 + 2) * 3 + 1 + 2

#define function1(a, b) a + b
function1(1, 2) // 1 + 2

#define function2(a, b) (object1) * (function1(a, b))
function2(3, 4) // (1 + 2) * (3 + 4)

#define X (Y) + x
#define Y (Z) + y
#define Z (X) + z
X // (((X) + z) + y) + x
Y // (((Y) + x) + z) + y
Z // (((Z) + y) + x) + z

#define REP(i, a, n) for(int i = a; i < n; i++)
REP(i, 0, N) REP(j, 0, N) // for(int i = 0; i < N; i++) for(int j = 0; j < N; j++)

#define no_args() replaced
no_args // no_args
no_args() // replaced

#define this that
#define stringify1(a) #a, a
stringify1(this is string) // "this is string" that is string

#define stringify2(a) # a
stringify2(this is string) // "this is string"
stringify2("this is string") // "\"this is string\""
stringify2("\\") // "\"\\\\\""
stringify2("\u1234") // "\"\\u1234\""
stringify2(\u1234) // "\u1234"

#define concat1 a ## b
concat1 // ab

#define concat2(a, b) a ## b
concat2(xxx, yyy) // xxxyyy
concat2(p + q, r + s) // p + qr + s
concat2(object1, object2) // object1object2
concat2(<, <=) // <<=
concat2(+, +) // ++
concat2(-, >) // ->

#define ellipsis1(a, ...) __VA_ARGS__
ellipsis1(a, b, c, d) // b, c, d

#define ellipsis2(...) __VA_ARGS__
ellipsis2(a, b, c, d) // a, b, c, d

#define hash_hash # ## #
#define mkstr(a) # a
#define in_between(a) mkstr(a)
#define join(c, d) in_between(c hash_hash d)
join(x, y) // "x ## y"
