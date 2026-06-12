# Review Principles

Apply all three principles before making any finding.

## 1. Fact-based review only
- No speculative comments. Every finding must be grounded in code you have actually read.
- Verify that a file or directory exists before reporting a path issue.
- Read the actual code; do not infer behavior from names alone.
- Do not report concerns that cannot be confirmed from the available context.

## 2. Full context before commenting
- Never review changed lines in isolation. Trace the full impact of each change.
- Track callers and callees of modified functions.
- Verify downstream effects on changed interfaces or type definitions.
- Read the PR description and author's code comments; do not duplicate findings already explained there.

## 3. Actionable suggestions
- "This name is bad" is not sufficient. Always provide a concrete fix.
- Provide specific remediation steps or alternatives, not just problem identification.
- Always include file path and line number (e.g. `firmware/esp32/main/led_backend.c:12`).
- Explain why the proposed change improves the code.
