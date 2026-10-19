# Issue tracker: Local Markdown

Issues and specs for this repo live as markdown files in `.scratch/`.

> **Planned migration**: the repo is in its early stage (design docs first). A private GitHub remote is expected later — re-run the `/setup-matt-pocock-skills` skill after the remote exists to switch this tracker to GitHub Issues (`gh` CLI).

## Conventions

- One feature per directory: `.scratch/<feature-slug>/`
- The spec is `.scratch/<feature-slug>/spec.md`
- Implementation issues are one file per ticket at `.scratch/<feature-slug>/issues/<NN>-<slug>.md`, numbered from `01` — never a single combined tickets file
- Comments and conversation history append to the bottom of the file under a `## Comments` heading

## Where other early-stage artifacts live

- Design docs: repo root, following `设计文档模版.md`
- Architecture diagrams: `compress-system.drawio` (draw.io)
- Course notes: `meeting/`

## When a skill says "publish to the issue tracker"

Create a new file under `.scratch/<feature-slug>/` (creating the directory if needed).

## When a skill says "fetch the relevant ticket"

Read the file at the referenced path. The user will normally pass the path or the issue number directly.
