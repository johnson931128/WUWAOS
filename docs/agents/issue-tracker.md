# Issue tracker: GitHub

Issues and workflow artifacts for this repo live as GitHub issues. Use the `gh` CLI for issue operations when the user explicitly asks to create, read, update, or close issues.

## Conventions

- Create issues with `gh issue create --title "..." --body "..."`.
- Read issues with `gh issue view <number> --comments`.
- List issues with `gh issue list --state open`.
- Comment with `gh issue comment <number> --body "..."`.
- Apply labels with `gh issue edit <number> --add-label "..."`.
- Close issues with `gh issue close <number> --comment "..."`.

Infer the repository from `git remote -v`; this clone uses GitHub.

## Pull requests as a triage surface

PRs as a request surface: no.

## WUWAOS constraint

Do not automatically create large batches of issues. For WUWAOS, prefer one small ticket or spec at a time unless the user confirms a broader planning session.

## When a skill says "publish to the issue tracker"

Create a GitHub issue only after the user confirms that publishing is desired.

## When a skill says "fetch the relevant ticket"

Run `gh issue view <number> --comments`.
