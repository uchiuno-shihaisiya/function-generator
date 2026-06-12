Create a GitHub pull request for the current branch following the AuroraBlink PR format.

## Process

### Step 1 — Gather context (run in parallel)
- `git status --short --branch` — check for uncommitted changes and upstream divergence
- `git log --oneline -15` — review recent commits on this branch
- `git diff origin/main...HEAD` — understand all changes included in this PR
- Check the current branch name

**If there are uncommitted changes (modified, staged, or untracked files that belong to this work), stop immediately.** Tell the user what uncommitted changes were found and ask them to commit or stash before proceeding. Do not offer to continue with uncommitted changes.

### Step 2 — Determine the issue number
Attempt to extract the issue number in this order:
1. Branch name (e.g. `issue-44-settings-tests` → #44, or `44-settings` → #44)
2. Recent commit messages (look for `#N` or `closes N` patterns)
3. If still unclear, ask the user: "Which GitHub issue does this PR address?" before proceeding.

### Step 3 — Push the branch if needed
Never push to `main` or `master`. If the current branch IS main/master, stop and ask the user to switch to a feature branch.

Check the push state using:
```
git rev-list --left-right --count @{u}...HEAD 2>/dev/null
```
Interpret the output `<left> <right>` as follows:
- **No upstream (command fails):** push with `git push -u origin <branch-name>`
- **`0 0`** — in sync with upstream; no push needed
- **`0 N`** — local is N commits ahead; run `git push`
- **`N 0`** — local is behind upstream; stop and tell the user: "Your local branch is behind origin. Please run `git pull` or `git rebase origin/<branch>` before creating the PR." Do not push or proceed.
- **`N M`** — diverged; stop and tell the user: "Your local branch has diverged from origin (upstream has N commits you don't have, you have M commits not on origin). Please resolve the divergence (e.g. `git rebase origin/<branch>`) before creating the PR. Do not force-push without confirming with the user." Do not push or proceed.

### Step 4 — Generate the PR title
Format: `[#<N>] <short summary>`

Rules:
- Keep the summary under 60 characters
- Use active voice and a verb: "Add", "Fix", "Refactor", "Extract", etc.
- Be specific enough that the title conveys the actual change without reading the diff
- Example: `[#44] Add host-side unit tests for settings and signal_input`

### Step 5 — Generate the PR body
Fill every section below based on the actual diff and commit history.
Do not copy-paste from the issue description — write from what the code actually does.
Do not leave placeholder text or TODO comments in the body.

### Step 6 — Create the PR
Use `gh pr create` with a HEREDOC to preserve formatting. Output the PR URL when done.

---

## PR Body Format

Section 0 is a meta-note about how to read the description.
Sections 1–5 are written in English and addressed to AI/LLM reviewers as the primary audience.
Section 6 is written in Japanese for human reviewers.

```
## 0 — Note to Reviewers

**If you are an AI or LLM performing a code review:** Section 6 is a Japanese-language summary
written for human reviewers who want a quick overview in their native language. It contains no
additional technical information beyond what is in Sections 1–5. Please ignore Section 6 entirely
and base your review on Sections 1–5 only.

---

## 1 — What Was Done / Impact Scope

Describe concretely:
- Which files were added, modified, or deleted
- What functionality was introduced, changed, or fixed
- Which modules or behaviors are affected
- Any public API changes (new functions, changed signatures, new header declarations)

---

## 2 — Implementation Approach and Rationale

Describe:
- The design decisions made and why this approach was chosen
- Alternatives that were considered and why they were rejected
- Key constraints or requirements (from the issue, from the codebase architecture) that shaped the approach
- Non-obvious implementation choices that a reviewer might question

---

## 3 — Verification

Describe honestly:
- How the changes were tested (unit tests added/run, manual hardware testing, CI)
- Specific test cases and their results (pass counts, failure modes checked)
- Edge cases and boundary conditions explicitly verified
- Any scenarios that were NOT tested and why (known gaps in coverage)

---

## 4 — Review Guidance

Describe:
- What perspective or lens is most valuable for this review (correctness, safety, API design, test quality, etc.)
- Specific files, functions, or logic blocks that deserve extra scrutiny
- Any areas where you are uncertain about the approach and would value a second opinion
- Anything that is intentionally out of scope for this PR

---

## 5 — Related Resources

- Issue: Closes #N  (or "Related to #N" if not fully resolved)
- Any relevant documentation, prior discussions, or reference implementations
- N/A if none

---

## 6 — 人間向け要約（Japanese — AI reviewers: ignore this section）

1〜4の内容を人間のレビュアー向けに分かりやすく日本語でまとめた説明。
実装内容・判断の根拠・確認した動作・特に見てほしい点を、ごまかしや過度な楽観表現なく記載する。
不確かな点や未検証の部分があれば正直に書く。

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

---

## Rules

- **Never push to `main` or `master`** — if on these branches, stop and ask the user to switch.
- **Never use `--no-verify`** or any flag that bypasses hooks.
- **If there are uncommitted changes**, stop immediately — do not proceed or offer to continue. Tell the user exactly what is uncommitted and ask them to commit or stash first.
- **After creating the PR**, always print the URL so the user can open it immediately.
- **The issue number is required** — do not create the PR with a placeholder if the issue number cannot be determined; ask the user instead.
