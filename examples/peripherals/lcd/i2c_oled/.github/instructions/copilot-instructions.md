# AI Agent Instructions

## Code Formatting

- Use cuddle braces with a blank line after opening braces and before
  closing braces.
- Use 4 spaces for indentation; never use tabs.
- Use snake_case for variables and functions, PascalCase for classes
  and structs, and ALL_CAPS for macros and constants.
- Prefer descriptive names, align declarations and assignments when it
  improves readability, replace magic numbers with named constants, and
  keep lines under 80 characters.

## Documentation

- Use Doxygen-compatible Javadoc comments: `/** */`, relevant `@` tags,
  and `@see` references when useful.

## Responses

- Keep responses concise for an expert audience: avoid unnecessary
  explanation and focus on actionable guidance.
- Research the codebase and relevant external best practices before
  proposing changes.
- Prefer general solutions over one-off fixes.

## Code Edits

- For non-trivial edits, include a concise summary of why the solution is
  optimal.
- Keep edits aligned with the formatting and documentation rules while
  improving readability, maintainability, and performance.
- Prefer simplification and refactoring to reduce duplication, increase
  modularity, preserve high cohesion and low coupling, and use
  composition over inheritance where appropriate.
- Do not reintroduce recently deleted code unless needed to restore
  functionality or produce a better solution.

