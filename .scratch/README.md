# `.scratch/` — local issue tracker

Local-markdown issue tracker for this repo. Full rules live in `docs/agents/issue-tracker.md`.

## Naming (English only)

Every path under `.scratch/` is **lowercase ASCII kebab-case**. Chinese or any other
non-ASCII name is never used here — even when the issue content itself is written in
Chinese, the directory and file names stay English.

- Feature dir — `.scratch/<feature-slug>/` → e.g. `week1-research-and-design`
- Spec — `.scratch/<feature-slug>/spec.md`
- Issue — `.scratch/<feature-slug>/issues/<NN>-<slug>.md` → e.g. `issues/01-skill-research-doc.md`

Ticket numbering starts at `01`; never a single combined tickets file.

## Layout

```
.scratch/
└── <feature-slug>/
    ├── spec.md              # what this feature/effort is and its acceptance
    └── issues/
        ├── 01-<slug>.md     # one file per ticket
        └── 02-<slug>.md
```

## Ticket file shape

- Title as `#` heading
- A `Status:` line near the top (the tracker is local, so status is recorded in-file)
- Body: what to do and why
- `## Comments` at the bottom for appended conversation history

## Status

No feature directory exists yet. The Week-1 breakdown proposed earlier is **not
approved for drafting** — the project background and the tech-selection tradeoffs
(pros/cons, why BRPC and not ByteRPC, etc.) get aligned in conversation first. The
first feature dir is created only when that has happened and the user says go.
