---
name: review-pr
description: Use when reviewing AuroraBlink GitHub pull requests with Codex; gathers PR context safely, applies typo, style, design/logic, and performance review passes, deduplicates must/should/nit findings, and can post an English review comment when explicitly requested.
---

# AuroraBlink PR Review

Use this skill when the user asks Codex to review an AuroraBlink pull request, especially requests like "review PR #55" or "check this pull request".

## References

Load these reference files as needed:

- `references/principles.md` - review principles that every pass must follow
- `references/review-domains.md` - typo, style, design/logic, and performance review scopes
- `references/output-format.md` - structured finding schema and final report format

## Workflow

### 1. Gather PR Context

Prefer GitHub connector tools when available. Use `gh` only as a fallback.

Gather:

- PR title, body, base branch, head branch, head SHA, and mergeability
- Changed file list
- Unified diff or patch
- Existing PR comments and reviews, so findings already acknowledged are not repeated

If the PR number is missing, ask for it. If the PR cannot be found, stop and report that the number or repository context is wrong.

### 2. Choose Context Strategy

Default to **diff + on-demand file reads**. This is the normal Codex mode.

Use these rules:

- **Diff only**: acceptable for small documentation/config changes or when the user asks for a lightweight review.
- **Diff + on-demand**: default; read related files only when a finding needs caller/callee, type, or surrounding context.
- **Full file**: use only when the PR is small enough that complete changed files are useful and cheap.

When reading local files for full-file or on-demand context, verify that the local checkout matches the PR head first:

```bash
git branch --show-current
```

Compare the current branch with the PR head branch. If they do not match, either:

- read file contents from GitHub by the PR head ref, or
- switch to diff-only review, or
- ask the user before changing the local checkout.

Do not base findings on local files from the wrong branch.

### 3. Run Review Passes

Apply `references/principles.md` before producing any finding.

Run the four logical review domains from `references/review-domains.md`:

1. Typo & Grammar
2. Code Style
3. Design & Logic
4. Performance & Resource Usage

If Codex multi-agent tools are available, run the domains as independent sub-agent passes and merge the results. If not, run the four passes sequentially in separate sections of your own reasoning. Either way, keep the domains separate until aggregation.

Design & Logic and Performance are always relevant for firmware/source changes. Typo & Grammar and Code Style are especially relevant for new docs, tooling, comments, logs, identifiers, new files, or new patterns. If a domain is not useful for the PR, include its final section as `Skipped by reviewer: <reason>` rather than silently omitting it.

### 4. Finding Rules

Only report issues that are grounded in code or text you actually read.

Every finding must include:

- severity: `must`, `should`, or `nit`
- title
- file path from repo root
- line number, or `0` for file-level findings
- issue
- suggestion

Severity:

- `must`: correctness, data loss, security, build breakage, hardware/firmware behavior risk
- `should`: maintainability, robustness, unclear design, likely future bug
- `nit`: minor wording, naming, formatting, or low-risk cleanup

Do not duplicate issues already acknowledged in the PR body or earlier comments unless the acknowledgement is incomplete or the implementation still has the problem.

### 5. Deduplicate

Merge overlapping findings before writing the final report.

Treat findings as duplicates when they refer to the same file, their line numbers are within 5 lines, and their issue descriptions overlap substantially. Keep the most specific finding. For line `0` findings, deduplicate by file path and semantic overlap.

### 6. Output and Comment

Write the report in English using `references/output-format.md`.

Lead with findings. Keep the summary short and secondary.

If the user asked you to comment on the PR, post the final report as a PR comment. Prefer GitHub connector tools for comments; use `gh pr comment` only as fallback.

Before posting, verify that the comment describes the actual reviewed head SHA or state.

## Final Checks

Before finishing, confirm:

- PR context was fetched from the intended PR
- local file reads, if any, used the PR head or GitHub head ref
- all four review domains are represented in the final report
- must/should/nit counts are correct after deduplication
- no generated local artifacts were included in the review outcome
