# GitHub Copilot Instructions

## Code Guidelines
- Add tests for new features/bug fixes and run all tests before committing
- Use English for commit messages and PR descriptions
- Use `./scripts/update_version.sh <version>` to update version files
- Remove `dev.cmake` before standard builds/tests; use it only for custom CMake options
- Run `./scripts/format-all.sh` before committing

## Communication Guidelines
- Respond in the user's language, except for `docs/`, which must be English
- Keep the same language throughout the conversation

## File Management
- Store temporary files, development documents, and drafts in `dev-docs/`
- Treat `dev-docs/` as disposable temporary storage
- Markdown files in `docs/` must be English and reviewed before publishing

## Tool Constraints
- **`gh` CLI:** Always prepend `$env:GH_PAGER=""` in PowerShell (for example, `$env:GH_PAGER=""; gh pr list`) or `GH_PAGER=` in bash/zsh; never omit it, and never modify global git or gh config.
- **Build/Test Work:** Use `.vscode/tasks.json` only as the reference for command sequence, `cwd`, config toggles, and prerequisites. Run the underlying commands directly in the terminal or with the appropriate dedicated tool; do **not** invoke VS Code tasks unless the user explicitly asks for a named task.


## Skills

- **Submit PR:** Follow `.github/skills/pr-submit/SKILL.md` to create or update pull requests
- **Review PR:** Follow `.github/skills/pr-review/SKILL.md` to review pull requests