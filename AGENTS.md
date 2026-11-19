# Azo

## Azo is a language implementation and interpreter built on top of az library

It has C/Java/JavaScript like syntax and strong runtime typing.
The az abstact fundamental type 'any' functions as the flexible base type.

The language specification is in doc/language.txt.

## Runtime model

The language is compiled into bytecode and executed in stack-based virtual machine.
All low-level features, such as memory management, come from the az library.

## Type system

The types form a single hierarchy with any at the base.
At moment new classes can only be created in az.

## Rules

doc/language.txt has to be kept up-to-date with code. It has to document all syntax with optional comment about semantics.

Non-trivial changes should be covered by tests.

Code comments should explain why something is done, not how.

Code should be readable - the ideal of 'self-commenting code' should be followed as much as possible.
