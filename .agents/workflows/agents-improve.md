---
description: "Extract reusable operational insights (gotchas, commands, pitfalls) to AGENTS.md or related SKILL file"
---

Analyze the session just completed strictly through the lens of reusable operational knowledge for future sessions.

### Core Intent:

Do NOT document changelogs, task summaries, or project specifications (general project architecture and specs belong in dedicated spec files). Your sole objective is capturing **durable institutional knowledge**: hard-won insights, environment quirks, non-obvious failure modes, and critical CLI commands.

### Inclusion Criteria (Add ONLY if true):

- **Tooling / Environment Quirks**: Toolchain flags, flashing/build quirks, environment variables, or dependency collisions that required non-obvious fixes.
- **Critical Pitfalls**: "Dead ends" or mistakes made during this session that a future agent might repeat if not explicitly warned.
- **Essential Operational Commands**: Non-standard or exact commands required for building, flashing, debugging, or validating this project that are not self-evident.

_If the task was routine implementation with zero reusable pitfalls or operational surprises, state that no updates are needed and do not modify any file._

### Routing (decide per insight, before writing):

1. **AGENTS.md** — only for cross-cutting knowledge that applies to most sessions in this repo (global build/test/flash commands, repo-wide environment quirks, pitfalls hit regardless of task). Max 1-2 lines per entry.
2. **Skill file** — for knowledge tied to a specific tool, device, workflow, or domain (e.g. one board, one toolchain, one deployment procedure). Add it to the existing `SKILL.md` that covers that topic.
   - List existing skills first (`.agents/skills/*/SKILL.md` or the repo's skills directory) and pick the closest match.
   - Create a new skill only if there is no fit AND the topic has several reusable entries; otherwise default to AGENTS.md.
   - If a skill is touched, make sure its `description` still says when to use it.
3. **Pointer** — if AGENTS.md lacks a reference to a skill that now holds relevant knowledge, add a one-line pointer (`See skill <name> for <topic>`), not the content itself.
4. When in doubt, prefer the skill over AGENTS.md. AGENTS.md is loaded in every session, so every line there has a cost.

### Rules for Updating (AGENTS.md and skills):

1. **Concise & Imperative**: Telegraphic bullets or mini-tables. No narrative prose, history lessons, or task recaps.
2. **Appropriate Section**: Integrate into existing sections (e.g. `## Operational Pitfalls & Gotchas`, `## Hardware / Flashing Quirks`, `## Verification Commands`); create AGENTS.md at the repo root only if missing.
3. **No Duplication**: Check AGENTS.md and the relevant skill first; refine existing points instead of adding redundant ones. Never store the same fact in both places.
4. **Keep it lean**: If AGENTS.md is growing past a few screens, move topic-specific blocks into a skill and leave a pointer.

User input/focus: $ARGUMENTS

Inspect AGENTS.md and the relevant skills, make only the strictly reusable additions, and summarize only the new rules added, stating for each whether it went to AGENTS.md or which skill file.
