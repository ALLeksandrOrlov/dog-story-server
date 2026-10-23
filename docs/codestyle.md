# C++ style guide

The project uses C++20 and the conventions below.
Formatting is checked with clang-format **18.1.8**:

```sh
python3 tools/check_style.py
python3 tools/check_style.py --fix
```

Only `.cpp` and `.hpp` files in `src/` are formatted. Tests are excluded.
The integrity of the fixed test suite is checked separately with
`python3 tools/verify_original_tests.py`.

## Formatting

- UTF-8, LF line endings, four spaces; continuation lines use eight spaces.
- Lines are limited to 120 characters. Opening braces go on the next line.
- `public`, `protected`, and `private` align with the class declaration; members use one indentation level.
- Namespace contents have no extra indentation.
- `const` precedes the type; `*` and `&` attach to the type: `const Map& map`.
- Conditions use `if (condition)`, loops use `for(...)` and `while(...)`,
  and selection uses `switch(...)`. Conditional bodies use braces.
- Constructor initializer lists start on the next line;
  the colon stays on the constructor declaration line.
- Headers go from general to local: system headers, external libraries, other
  project components, the same component, and the matching header for the `.cpp` file.
  The script determines the last two groups from file locations.

`.clang-format` controls indentation and layout. Use `tools/check_style.py`
for the full check, including header order.

## Names and interfaces

Files use `snake_case`; C++ headers have the `.hpp` extension and use
`#pragma once`. Namespaces group components by purpose, such as
`app`, `model`, `view`, and `http_handler`.
Functions internal to a translation unit live in an anonymous namespace.

Types, functions, and methods use `PascalCase`; method names describe actions.
Parameters, local variables, and public fields use `snake_case`.
Private and protected fields use the `m_` prefix. External contracts retain
required names, including `operator()`, `std::hash`, Boost serialization, and
standard library extension points. HTTP API names are independent of C++ style.

Objects and fields are initialized at declaration. Prefer `const`, RAII,
`enum class`, named constants, and explicit resource ownership. Input parameters
are const; mutable output parameters follow them. Parameters that transfer ownership
are accepted by value or non-const rvalue reference to preserve move semantics;
this is stated explicitly in the contract. Methods that do not change state are
marked `const`; overrides are marked `override`. Polymorphic interfaces must have
a virtual destructor. Use pImpl when it hides a significant external dependency.

## Documentation and readability

Classes, functions, and methods have Doxygen comments. Public method contracts
are documented in headers: purpose, input constraints, ownership, and significant
errors. Comments explain a reason or obligation rather than repeat the method name.
Internal helpers are documented next to their definitions.

```sh
doxygen Doxyfile
```

HTML documentation is written to `build/docs/html/index.html`; XML goes to
`build/docs/xml/`. Doxygen also checks private methods, static functions, and local
classes; missing documentation is treated as an error.

Readability relies on one responsibility per method, explicit invariants,
early exits on errors, shallow nesting, and meaningful names.
Pure computations are separated from HTTP, the database, and the file system.
An additional design pattern is introduced to address a specific responsibility boundary.

The formatter does not check architecture, ownership correctness, naming meaning,
the absence of magic values, or the validity of floating-point comparisons and
assignments in conditions. It also cannot guarantee the line length of indivisible
string literals and URLs. These require manual review and testing.
Changes to `const` and added braces must be checked by compiling the code.
