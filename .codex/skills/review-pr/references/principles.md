# Review Principles

Apply all three principles before making any finding.

## 1. Fact-based review only

- No speculative comments. Every finding must be grounded in code or text you actually read.
- Verify that a file or directory exists before reporting a path issue.
- Read the actual code; do not infer behavior from names alone.
- Do not report concerns that cannot be confirmed from the available context.

## 2. Full context before commenting

- Never review changed lines in isolation when behavior depends on surrounding code.
- Track callers and callees of modified functions when a finding depends on behavior outside the changed hunk.
- Verify downstream effects on changed interfaces or type definitions.
- Read the PR description and existing comments; do not duplicate findings already acknowledged there.

## 3. Actionable suggestions

- Always provide a concrete fix, not just a complaint.
- Include a file path and line number, using line `0` only for file-level issues.
- Explain why the proposed change improves correctness, maintainability, or clarity.
