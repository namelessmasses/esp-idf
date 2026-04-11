# AI Agent Instructions

## Responses

- Keep responses concise for an expert audience: avoid unnecessary
  explanation and focus on actionable guidance.
- Research the codebase and relevant external best practices before
  proposing changes.
- Prefer general solutions over one-off fixes.

## Code Formatting & Style

- Use cuddle braces with a blank line after opening braces and before
  closing braces.
- Use 4 spaces for indentation; never use tabs.
- Use snake_case for variables and functions, PascalCase for classes
  and structs, and ALL_CAPS for macros and constants.
- Prefer descriptive names, align declarations and assignments when it
  improves readability, replace magic numbers with named constants, and
  keep lines under 80 characters.
- Use a Hungarian-style notation 
  - _member_ `m*`
  - _global_ `g*`
  - _static_ `s*`
  - _temporary_ `tmp*`
  - _size_ `sz*`
  - _count_ `n*`
  - _index_ `idx*`
  - _pointer_ `p*`
  - _input_ `in*` and _output_ `out*` function formal arguments
  - _boolean_ `b*` 
  - _function_ `fn*`
  - _compile-time constant_ `k*` values
  - _const_ `c*`
  - _volatile_ `v*`
  - _const-volatile_ `cv*` 

### Comments

- `//` for single-line
- `/** */` for multi-line comments
  - Internal lines of multi-line comments should start with `*` and be aligned with the first `*` after the
    opening `/**`.
- Do not use end-of-line comments
  
### Documentation

- Use Doxygen-compatible Javadoc comments: `/** */`, relevant `@` tags,
  and `@see` references when useful.

## Code Edits

- For non-trivial edits, include a concise summary of why the solution is
  optimal.
- Keep edits aligned with the formatting and documentation rules while
  improving readability, maintainability, and performance.
- Do not use band-aid solutions or local rule bypasses as a substitute for a
  general fix. Use them only when a root-cause solution is not feasible and
  the user explicitly approves the exception.
- Prefer simplification and refactoring to reduce duplication, increase
  modularity, preserve high cohesion and low coupling, and use
  composition over inheritance where appropriate.
- Do not reintroduce recently deleted code unless needed to restore
  functionality or produce a better solution.

