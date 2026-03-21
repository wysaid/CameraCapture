# Publishing ccap To ClawHub

This file is for maintainers of the `ccap` skill bundle.

## Prerequisites

- `clawhub` CLI installed
- authenticated with `clawhub login`
- ready to publish the folder `skills/ccap`

Install the CLI if needed:

```bash
npm i -g clawhub
```

Authenticate:

```bash
clawhub login
clawhub whoami
```

## First Publish Example

```bash
clawhub publish ./skills/ccap \
  --slug ccap \
  --name "ccap" \
  --version 0.1.0 \
  --tags latest \
  --changelog "Initial ClawHub release"
```

## Update Example

```bash
clawhub publish ./skills/ccap \
  --slug ccap \
  --name "ccap" \
  --version 0.1.1 \
  --tags latest \
  --changelog "Refine installation guidance and command examples"
```

## Notes

- Publish the skill folder, not the repository root.
- Keep `SKILL.md` as the main public entrypoint.
- Keep frontmatter stable and concise; ClawHub parses it during publish.
- If release-binary guidance changes, verify asset naming before updating the skill text.
- If platform support changes, update both `SKILL.md` and the referenced notes.