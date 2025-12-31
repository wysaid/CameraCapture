# GitHub Copilot Instructions

## Code Guidelines
- Add tests for new features/bug fixes and run all tests before committing
- Use English for commit messages and PR descriptions
- Run `./scripts/update_version.sh <version>` to update version files
- Remove `dev.cmake` before standard builds/tests (use for custom CMake options)
- Run `scripts/format-all.sh` before committing

## Communication Guidelines
- Respond in user's language (except `docs/` which must be English)
- Maintain language consistency within conversations

## File Management
- Store temporary files, development documents, and drafts in `dev-docs/`
- `.md` files in `docs/` must be English and require review before publishing
- Treat `dev-docs/` as `/tmp`