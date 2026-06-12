# Output Format

Every sub-agent must return its findings as a JSON object with exactly this schema.
Return only the JSON object. Do not include any explanatory text before or after it.

Field descriptions:
- `title`: short summary of the issue
- `filepath`: path from repo root (e.g. `firmware/esp32/main/led_driver.c`)
- `line`: line number as an integer; use `0` when the finding applies to the file as a whole and no single line is the locus of the issue
- `issue`: what is wrong and why it matters
- `suggestion`: concrete fix or alternative

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
  "should": [
    {
      "title": "Example should-fix finding",
      "filepath": "firmware/esp32/main/settings.c",
      "line": 17,
      "issue": "Description of the problem.",
      "suggestion": "Suggested improvement."
    }
  ],
  "nit": [
    {
      "title": "Example nit finding",
      "filepath": "firmware/esp32/main/mode_manager.c",
      "line": 0,
      "issue": "Minor issue applying to the file as a whole.",
      "suggestion": "Minor improvement suggestion."
    }
  ]
}
```

If a severity level has no findings, return an empty array for that key.

Example with no findings at any level:
```json
{
  "must": [],
  "should": [],
  "nit": []
}
```
