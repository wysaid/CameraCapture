# Standalone Skills

This directory contains standalone skill folders that can be reused as general agent skills.

Each standalone skill should follow these rules:

- it lives in its own folder under `skills/`
- the folder has a top-level `SKILL.md`
- supporting files are text-based
- frontmatter stays concise and machine-parseable
- publishing or packaging commands should target the skill folder itself, not the repository root

For this repository, the standalone skill is:

- [ccap](./ccap)