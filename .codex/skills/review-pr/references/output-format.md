# Output Format

## Finding Schema

Represent intermediate findings with this schema:

```json
{
  "must": [
    {
      "title": "Example must-fix finding",
      "filepath": "firmware/esp32/main/led_driver.c",
      "line": 42,
      "issue": "Description of what is wrong and why it matters.",
      "suggestion": "Concrete fix or alternative."
    }
  ],
  "should": [],
  "nit": []
}
```

Field rules:

- `title`: short summary
- `filepath`: path from repository root
- `line`: integer line number; use `0` only for file-level findings
- `issue`: observable problem and impact
- `suggestion`: concrete remediation

## Final Report

Use this shape for the final review comment:

```markdown
## Review Result

### 1. Typo & Grammar Check
No issues found.

### 2. Code Style Check
#### Should Fix: Short summary
**File:** path/to/file:LINE
**Issue:** What is wrong and why it matters.
**Suggestion:** Concrete fix or alternative.

### 3. Design & Logic Check
No issues found.

### 4. Performance & Resource Usage Check
No issues found.

### Summary
- **Must Fix:** N items
- **Should Fix:** N items
- **Nit:** N items
```

If a domain is skipped, keep the section and write `Skipped by reviewer: <reason>`.
