# Rules for arithmetic operations

Supported operators: `+ - * / % ++ -- += -= *= /= %= << >> &= |= ^= <<= >>= & | ^ ~ && || !`

Operator precedence follows C/C++ rules.

## Maskable exceptions

Four exceptions are individually controllable (maskable) and may also be
enabled/disabled in the execution environment:

- `CHANGE_OF_PRECISION` — a value can be represented only approximately: the
  format restriction means the exact value may differ from the original
  (rounding). **Masked (suppressed) by default.**
- `CHANGE_OF_MAGNITUDE` — the magnitude of a value cannot be represented by the
  target type, so it would be clamped to the largest/smallest representable
  value. **Thrown by default.**
- `NON_FINITE` — a calculation with finite operands produces a non-finite
  result (infinity or NaN). **Masked (suppressed) by default**; IEEE 754 rules
  apply and the value propagates. Thrown only when a finite calculation first
  produces a non-finite value; propagation of an existing infinity/NaN is
  silent.
- `INTEGER_OVERFLOW` — an integer operation produces a result that does not fit
  its (promoted) type. **Thrown by default.**

The general principle: **rounding and non-finite follow IEEE 754 and are
allowed by default; integer overflow and clamping are language-level concepts
(there is no IEEE behavior for narrowing integers) and throw by default.**

## Computation modes

Three computation modes override the default overflow/clamping/non-finite
behavior for a region or a single expression. Each may be used as a statement
block `{ ... }` or as an expression `(expr)`:

- `finite` — throws `NON_FINITE` on the first non-finite result (a finite
  computation producing infinity/NaN).
- `saturate` — clamps integer overflow and narrowing to the nearest
  representable value (saturation).
- `wrap` — wraps integer overflow and truncates narrowing (two's complement /
  modular arithmetic).

```
v  = finite (normalize(u));            // throws if normalize produces inf/NaN
p  = saturate (pixel + brightness);    // clamps instead of overflowing
h  = wrap (a * 31 + b);                // wraps on overflow

finite {                               // statement form
    v = normalize(u);                  // throws if u is a zero vector
    m = inverse(mat);                  // throws if mat is singular
}
```

A mode saves the current mode, applies to the enclosed code — including called
interpreted code — and restores on exit, even on an exception.

The modes affect the exceptions as follows:

| mode        | `INTEGER_OVERFLOW` | `CHANGE_OF_MAGNITUDE` (narrowing) | `NON_FINITE` | `CHANGE_OF_PRECISION` (rounding) |
|-------------|--------------------|-----------------------------------|--------------|----------------------------------|
| (default)   | throw              | throw                             | allow (IEEE) | allow                            |
| `saturate`  | clamp              | clamp                             | allow (IEEE) | allow                            |
| `wrap`      | wrap               | wrap (truncate)                   | allow (IEEE) | allow                            |
| `finite`    | throw              | throw                             | **throw**    | allow                            |

`finite` does not change integer overflow/clamping; `saturate` and `wrap` do
not change rounding or non-finite.

**NOTE:** `saturate` and `wrap` are integer modes — floating-point overflow is
always IEEE (`inf`/NaN), never clamped or wrapped, so `saturate((float) 1e100)`
still yields `inf`. `wrap` on signed types is two's complement:
`wrap((int8) 130) == -126`.

## Cast qualifiers

A single cast qualifier remains:

- `exact` — additionally throws `CHANGE_OF_PRECISION` on rounding. Clamping and
  overflow still follow the computation mode.

`(int8 exact) b` throws if `b` needs rounding or does not fit `int8`. Combined
with a mode, e.g. `wrap((int8 exact) b)` throws on rounding but wraps on
clamping.

## Binary + - * / %

Floating-point arithmetic follows IEEE 754 rules.

Floating-point to integer conversions round towards zero.

Integer division rounds towards zero.

Integer modulo is defined by the identity:

```
(a / b) * b + a % b == a
```

so the remainder takes the sign of the dividend (as in C).

**NOTE:** division truncates toward zero and `%` has the sign of the dividend,
matching C/Java. Python-style floored division/remainder would need a distinct
operator.

### Argument promotion rules

The common type of a binary arithmetic expression is chosen by the first
matching rule (operands are converted to it, and the operation is evaluated in
that type):

1. complex double — if either operand is complex double, or one is complex
   float and the other is double or a 64-bit integer (int64/uint64).
2. complex float — if either operand is complex float.
3. double — if either operand is double, or one is float and the other is a
   64-bit integer.
4. float — if either operand is float.
5. int64 — if either operand is int64, or one operand is signed and the other
   is uint32 or uint64 (the wider signed type can represent all uint32 values;
   a signed/uint64 mix goes to int64 because there is no wider signed type).
6. uint64 — if either operand is uint64 (both are unsigned at this point).
7. int32 — if either operand is signed (any signed type, or a
   signed/small-unsigned mix).
8. uint32 — otherwise (both operands are unsigned and at most 32 bits).

Notes / rationale:

- float combined with a 64-bit integer promotes to double (float cannot
  represent all int64/uint64 values exactly); the same rule lifts complex float
  to complex double.
- rounding can still happen on (u)int32 to float or (u)int64 to double
  conversion.
- Small integers (int8..int16, uint8..uint16) promote to int32/uint32 —
  sub-32-bit arithmetic is always done at machine width, matching C integer
  promotion.
- Mixed signed/unsigned where the unsigned type is narrower than the signed
  result is still exact (uint32 always fits in int64); uint64 mixed with signed
  also goes to int64 (a large uint64 value overflows int64 — see computation
  modes).

**NOTE:** mixing signed and unsigned promotes to a *signed* type (int64),
unlike C, which promotes to unsigned. `int32 + uint32` is `int64` here but
`uint32` in C. This is deliberate (it avoids the C trap where a negative signed
value silently becomes a huge unsigned value), but it means `int64 + uint64`
overflows on large uint64 values.

### Promotion errors

If, during argument promotion, a value cannot be exactly represented by the
target type:

- value rounding (e.g. `1.5f` rounds to `1U`)
  - constant folding: a warning and the rounded value.
  - runtime: `CHANGE_OF_PRECISION` is thrown under `exact`, masked otherwise;
    the rounded value is used if ignored.
- value clamping (e.g. `-10` becomes `0U`)
  - constant folding: an error by default; a warning and the clamped value
    under `saturate`; the truncated value under `wrap`.
  - runtime: `CHANGE_OF_MAGNITUDE` is thrown by default; the clamped value is
    used under `saturate`; the truncated value under `wrap`.
- invalid arguments (non-numeric argument)
  - constant folding: an error.
  - runtime: `INVALID_TYPE` is thrown.

### Calculation errors

- integer overflow
  - constant folding: an error by default; a warning and the clamped value
    under `saturate`; the wrapped value under `wrap`.
  - runtime: `INTEGER_OVERFLOW` is thrown by default; the clamped value is used
    under `saturate`; the wrapped value under `wrap`.

**NOTE:** integer overflow throws by default (it does not wrap, unlike C's
unsigned arithmetic or Java's `int`). Use `wrap { ... }` for modular
arithmetic.

- division by (integer) zero
  - constant folding: an error.
  - runtime: `DIVIDE_BY_ZERO` is thrown (unaffected by the computation mode).
- non-finite result of finite operands (e.g. `1e38 * 1e38`, `0.0 / 0.0`)
  - constant folding: a warning; the result (NaN/infinity) is still computed.
  - runtime: `NON_FINITE` is thrown under `finite`, masked otherwise; the result
    is computed if ignored.
- invalid arguments (`%` with a complex argument)
  - constant folding: an error.
  - runtime: `INVALID_TYPE` is thrown.

## Shifts (<< >>)

Only integral operands are allowed.

The left operand is promoted to `uint32` (if int8..uint32) or `uint64` (if
int64/uint64), keeping the bit pattern and left-padding with zeroes to the
promoted width.

The right operand is converted to `uint32`.

The result type is the promoted left operand type.

Both operations zero-fill the vacated positions. Shifting by the width of the
promoted left operand or more results in zero.

**AMBIGUOUS:** the original said "more than the width" results in zero, leaving
the exactly-equal-to-width case unspecified. Reworded to "the width or more";
adjust the boundary if that was intentional.

**NOTE:** `>>` is always a logical (zero-fill) shift, even for signed operands.
`int8(-8) >> 1` is `uint32(124)`, not `-4`. Java's `>>` is arithmetic; this is
not. There is currently no arithmetic right shift — see "Remarks".

### Promotion errors

- value clamping (e.g. `-10` becomes `0U`)
  - constant folding: an error.
  - runtime: `INVALID_VALUE` is thrown.
- invalid arguments (non-integral argument)
  - constant folding: an error.
  - runtime: `INVALID_TYPE` is thrown.

## Bitwise operations (& | ^)

Both operands are promoted to `uint32` or `uint64`, keeping the bit pattern and
left-padding with zeroes to the promoted width.

The result type is the promoted operand type.

### Promotion errors

- invalid arguments (non-integer argument)
  - constant folding: an error.
  - runtime: `INVALID_TYPE` is thrown.

## Unary operations (+ - ~)

`+` is a no-op — it preserves the type and value.

`-` is arithmetic negation — it preserves the type.

If the result cannot be represented in the operand type — negating a non-zero
unsigned value, or the most negative signed value (e.g. `-int8(-128)`) —
`INTEGER_OVERFLOW` follows the computation mode:

- constant folding: an error by default; a warning and the clamped value under
  `saturate`; the wrapped value under `wrap`.
- runtime: `INTEGER_OVERFLOW` is thrown by default; the clamped value is used
  under `saturate`; the wrapped value under `wrap`.

`~` on integers is bitwise NOT — it produces the unsigned type of the same
width.

`~` on complex is the complex conjugate — it preserves the type.

Rationale:

Unary operations do not have to manage mixed operand types; they are simple
transformations of a single value, so no promotion or cross-type conversion is
involved.

Negation is an arithmetic operation, so a result that does not fit the operand
type is `INTEGER_OVERFLOW` (not `CHANGE_OF_MAGNITUDE`, which is reserved for
conversion/narrowing). This matches binary subtraction: `0 - x` for an unsigned
`x` underflows as `INTEGER_OVERFLOW`, and so does `-x`.

`~` is overloaded for bitwise NOT and complex conjugate. These are two
completely different programming domains, so accidental misuse is very
unlikely. For the same reason `~` does not promote floats/doubles to complex —
real floating point and complex arithmetic are separate domains, so explicit
conversion is required.

Making bitwise NOT preserve the operand width (and yield an unsigned type)
resolves the classic confusion of whether to promote first and then flip, or
flip first and then promote.

```
uint8 b = ~a;     // never throws — ~a is always representable in uint8
uint8 b = a | b;  // never throws — a | b has only the low 8 bits set
```

**NOTE:** these examples use `uint8`, not `int8`. `~a` is `uint8`, and
assigning `uint8` to `int8` throws for values >= 128 (they do not fit the
signed range). The examples stay exact only because the target is unsigned.

**NOTE:** in C, `~` on an integer yields the promoted (signed) type; here
`~int8(x)` yields `uint8`. This is deliberate (flip only the operand's bits),
but it differs from C.

**NOTE:** unary `-` on unsigned overflows (the result is negative and does not
fit the unsigned type), so it follows the computation mode rather than wrapping
silently as in C. For the C two's-complement idiom (`x & -x`) use `wrap` or
`twosComplement` (see "Remarks").

## Increment and decrement (++ --)

`++x` and `x++` are `x = x + 1`; `--x` and `x--` are `x = x - 1`. They are
expressions, as in C: the prefix form yields the new value, the postfix form
yields the old value.

They follow the same rules as the equivalent operation and assignment:
rounding is allowed, and overflow/clamping follow the computation mode.

```
uint32 x = max;
x++;   // throws INTEGER_OVERFLOW (default), clamps (saturate), or wraps (wrap)
```

## Arithmetic assignment operations (+= -= *= /= %=)

`a += b` is the same as `a = (TYPE_OF(a)) (a + b)` — rounding is allowed, and
overflow/clamping follow the computation mode.

```
for (uint32 i = array.length - 1; i >= 0; i -= 1) ...
```

throws when `i` underflows past zero (unless wrapped or saturated by a mode).

## Bitwise assignment operations (&= |= ^=)

`a |= b` is the same as `a = (TYPE_OF(a)) (a | b)` — rounding allowed,
clamping/overflow follow the computation mode.

By default (clamping throws), the two error cases are caught:

- unintended bitwise operations on signed integers (the result would be
  negative, so it does not fit the signed target type and throws);
- using a wider type as the second operand of `|=` or `^=` so that the set bits
  cannot be represented.

## Shift assignment operations (<<= >>=)

`a >>= b` is the same as `a = (TYPE_OF(a)) (a >> b)` — rounding allowed,
clamping/overflow follow the computation mode.

**NOTE:** `a >>= 1` for a signed narrow `a` performs a logical shift, so
`int8(-8) >>= 1` yields `124`, not `-4` (see "Shifts").

## Assignment (=)

`a = b` behaves as `a = (TYPE_OF(a)) b` — rounding (and non-finite results) are
allowed, but a magnitude change (clamping) always throws, even under `wrap` or
`saturate`. The computation modes relax the implicit *computation* rules, not the
implicit *assignment* rules. To clamp or wrap on assignment, use an explicit
cast: `a = (TYPE_OF(a)) b`.

A compile-time warning is emitted unless the conversion is provably
magnitude-preserving.

**NOTE:** narrowing that does not fit throws. `uint8 x = 300` is an error, not
`255` (clamp) or `44` (C/Java truncate). For the bit-pattern result use
`uint8 x = (uint8) 300` (or `300 & 0xff`); for saturation use
`uint8 x = saturate((uint8) 300)`.

## Casting

Casting an arithmetic type to another result type follows the argument
promotion rules above, but with the target type fixed. If the cast requires
rounding or clamping:

- value rounding (e.g. `1.5f` rounds to `1U`)
  - constant folding: an error under `exact`, otherwise a warning and the
    rounded value.
  - runtime: `CHANGE_OF_PRECISION` is thrown under `exact`, masked otherwise;
    the rounded value is used if ignored.
- value clamping (e.g. `-10` becomes `0U`)
  - constant folding: an error by default; a warning and the clamped value
    under `saturate`; the truncated value under `wrap`.
  - runtime: `CHANGE_OF_MAGNITUDE` is thrown by default; the clamped value is
    used under `saturate`; the truncated value under `wrap`.
- non-finite result of finite operands (e.g. `1e100` converted to float)
  - constant folding: a warning and the result is infinity.
  - runtime: `NON_FINITE` is thrown under `finite`, masked otherwise; the result
    is infinity/NaN per IEEE 754.

**NOTE:** integer narrowing throws by default (`int8 a = 300`), but
floating-point overflow to infinity is IEEE-defined and silent by default
(`float f = 1e100` yields `inf`). Use `finite { ... }` to catch the latter.

### Floating-point to integer

Casting a float or double to an integer type rounds the fractional part towards
zero, then applies to the rounded value:

- NaN — never representable: thrown unconditionally (a hard error, regardless
  of mode; NaN has no clamp or wrap fallback).
- infinity, or a finite value outside the integer's range — treated as clamping
  (`CHANGE_OF_MAGNITUDE`): thrown by default, clamped to the target's MAX/MIN
  under `saturate` and `wrap`. `wrap` falls back to saturation here — a float
  value has no meaningful two's-complement truncation.

The fractional rounding itself is `CHANGE_OF_PRECISION` — thrown under `exact`,
silent otherwise.

**NOTE:** this is the one place a non-finite value is clamped rather than
propagated: `(int32) inf` yields `INT32_MAX` under `saturate`/`wrap`, and
throws by default. NaN is the exception — it always throws.

## Remarks

Wrapping and saturation are expressed with the `wrap` and `saturate` modes
rather than special operators. Bit reinterpretation between integer and
floating-point representations uses:

- `fromBits` — construct an integer, float or double from an unsigned integer.
- `toBits` — construct an unsigned integer from an integer, float or double.
- `twosComplement` — get the two's complement of an unsigned integer.

**AMBIGUOUS:** `twosComplement` is described as "get the two's complement of an
unsigned integer", which reads like negation (flip + 1). If it is meant to
*reinterpret* an unsigned bit pattern as a signed value, say so explicitly.

**NOTE:** there is no direct bit-preserving cast from unsigned to signed
integers. `wrap((int8) u)` truncates the bit pattern; use it (or `fromBits`)
explicitly rather than an implicit cast.

Rationale:

The standard arithmetic operators perform 'normal' calculations with 'normal'
integers and should behave in the least surprising way. Wrapping, saturation
and strict non-finiteness are specific, intentional modes, so they should be
spelled out explicitly (`wrap`, `saturate`, `finite`) rather than being the
default.

**TODO:** there probably should also be a signed shift operator for
multiply/divide by a power of two on both signed and unsigned types (the
current `>>` is logical only).
