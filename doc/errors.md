# Compile errors, warnings and runtime exceptions

The language explicitly assigns every failure to exactly one of three tiers.
The tiers are disjoint: a failure mode has exactly one home.

## The rule

**A compile error is a construct that cannot be compiled into bytecode.**

If there is no rule to emit bytecode for a construct, compilation fails.
If a construct can be compiled, it compiles - misbehaviour is reported at
runtime as an exception, and at compile time at most as a warning.

Consequences:

- The same construct may be an error in one context and valid in another,
  because compilability differs. Example: `a = 1` with `a` undeclared in
  static context has no assignment target (no emission rule -> error),
  while in a member context it installs an attribute (has a rule -> compiles).
- Compilation success never depends on optimization level. The number and
  coverage of warnings may depend on it (deeper constant propagation finds
  more), but whether a program compiles may not.

## Tier 1: compile errors (structural)

Exhaustive classes:

- Syntax violations.
- Name/scope structure: undeclared names where no valid target exists,
  redeclaration in the same scope, scope-rule violations (e.g. exception
  declarations outside program scope).
- Invalid type expressions: unknown class, non-constant type, invalid cast
  target.
- Signature structure: lambda argument/return-type form, argument count
  mismatches.

## Tier 2: runtime exceptions (behavioral)

Every registered exception (see the exception registry) is a runtime event
and only a runtime event, including but not limited to:

- Arithmetic: DivideByZero, IntegerOverflow, ChangeOfMagnitude,
  ChangeOfPrecision, NonFinite (per computation mode).
- Type mismatches: InvalidType.
- Conversion failures: InvalidConversion.
- Contract violations: NotWritable (final/const reassignment), 
  NullDereference, OutOfBounds, InvalidProperty, InvalidValue.

Any of these may also be reported at compile time as an *exception warning*
when the compiler can prove the code throws - but the program still compiles
and throws at runtime (and the exception may be caught).

## Tier 3: warnings (best-effort)

Warnings never affect compilation success. Two classes:

- **Exception warnings**: the code provably throws a runtime exception
  (constant `1 / 0`, constant overflow, provably-invalid type operation,
  final reassignment visible at compile time).
- **Soft warnings**: legal but suspicious code (`a = a + 0`, a loop step
  modifying a variable not used in the condition, self-assignment, unused
  variables, unreachable code after return).

Warning coverage is explicitly unspecified and may grow with optimization
level; `AZO_TERM_FLAG_THROW_WARNED` deduplicates exception warnings across
optimizer iterations.

For strict workflows, a compiler flag (planned):
`--exception-warnings-are-errors`.
