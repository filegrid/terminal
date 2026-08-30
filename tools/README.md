# Tool layout

- `cmake/`: CMake helper scripts used during configuration and version generation.
- `portable/`: portable package generation tool, packaging CMake helpers, and identity-package scripts.
- `workspace-icons/`: scripts that import, generate, compose, and validate workspace icon assets.
- `diagnostics/`: locally generated crash/debug helper executables; these are not product source or packaging input.

Product runtime code must not be added under `tools/`.
