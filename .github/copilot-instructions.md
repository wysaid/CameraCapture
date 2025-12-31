# GitHub Copilot Instructions

## Code Guidelines
- Add tests for bug fixes or new features
- Run all tests before committing
- Use English for commit messages and PR descriptions
- Run `./scripts/update_version.sh <new_version>` to update version files
- `dev.cmake` (git-ignored) overrides default CMake options. Remove it for standard builds/tests; keep for custom settings
- Run `scripts/format-all.sh` to format source files before committing

## Communication Guidelines
- Respond in the user's language to facilitate cross-regional team collaboration (except for core documentation in `docs/`)
- Maintain language consistency within conversations

## Documentation Guidelines
- `.md` files in `docs/` must be in English
- Commit new `.md` files only after review
- Store development-related documents that don't need to be committed in the `dev-docs/` directory