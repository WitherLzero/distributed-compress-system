# AGENTS.md

## Agent skills

### Issue tracker

Issues live as local markdown files under `.scratch/<feature>/` — the repo has no remote yet; a private GitHub remote is planned, re-run `/setup-matt-pocock-skills` after it exists to switch to GitHub Issues. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: the five canonical roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), written as a `Status:` line in each local issue file. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` + `docs/adr/` at the repo root (created lazily by `/domain-modeling`). See `docs/agents/domain.md`.

### Dev workflow

Windows + WSL2 双副本协作约定（git 拓扑、两侧分工、git 纪律、已知坑）：`docs/agents/dev-workflow.md`。环境搭建实操另见 `docs/research/wsl-build-environment.md`。

### Collaboration protocol

人机协作节奏（实现票 → 检查点 → 四拍循环、学习三层细度）：`docs/agents/collaboration-protocol.md`。
