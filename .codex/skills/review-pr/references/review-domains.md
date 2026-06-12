# Review Domains

## 1. Typo & Grammar

Focus only on spelling and grammar in:

- identifiers
- comments
- log messages
- error messages
- documentation and tooling text

Do not report design, performance, or style issues in this pass.

## 2. Code Style

Focus only on readability and consistency:

- consistency with nearby project patterns
- file placement
- meaningful naming
- redundant or misleading comments
- avoidable structural confusion

Do not report design, performance, or spelling issues in this pass.

## 3. Design & Logic

This is the highest-priority domain. Focus on:

- correctness against intended behavior
- missing branches or edge cases
- error handling and propagation
- boundary values and invalid inputs
- SRP, KISS, DRY, and YAGNI issues that affect maintainability

### ESP32 / FreeRTOS Checks

Apply these checks only when changed files include C/C++ source or headers.

#### ISR safety

- No heap allocation from ISR context.
- No blocking calls from ISR context.
- No normal logging from ISR context.
- FreeRTOS calls from ISR must use `FromISR` variants.
- `portYIELD_FROM_ISR` must be used when a higher-priority task is woken.
- ISR functions and directly/indirectly called functions that must run during flash-disabled periods need `IRAM_ATTR`.
- ISRs should defer non-trivial work to tasks.

#### Critical sections

- Every enter has a matching exit on every path.
- ISR context uses ISR critical-section variants.
- Critical sections contain no blocking calls, logging, heap allocation, or long work.

#### Shared state

- ISR/task shared state is protected and uses `volatile` where appropriate.
- Composite state reads are protected from partial updates.
- Multi-byte or struct state shared across contexts is protected or transferred through a queue/ringbuffer.

#### FreeRTOS task safety

- Task notification actions match the data-loss semantics needed.
- Tasks do not busy-wait.
- Stack sizes are plausible for new or deeper call chains.
- Semaphore/mutex use cannot deadlock or recurse incorrectly.

#### C memory and buffers

- No unbounded `strcpy`, `strcat`, `sprintf`, or `gets`.
- Fixed buffers remain null-terminated.
- Array indices are bounded.
- `sizeof` is not accidentally applied to a pointer when buffer size is required.
- Large buffers are not placed on small task stacks without justification.

#### Watchdog and peripherals

- Long loops yield or reset watchdog as appropriate.
- `esp_timer` callbacks are short and non-blocking.
- GPIO ISR service initialization order is correct.
- RMT / WS2812B timing changes match protocol requirements.
- NVS handles are closed on all paths.
- NVS writes are not performed at excessive frequency.

## 4. Performance & Resource Usage

Focus only on performance and resource usage:

- redundant computation
- avoidable division/modulo/floating-point in hot paths
- unnecessary memory copies
- large structs passed by value
- loop-invariant work inside loops
- O(n) calls such as `strlen` repeated in loops
- IRAM placement for truly latency-sensitive hot paths
- stack allocation of large buffers
- resource release on all paths

Do not report design, style, or spelling issues in this pass.
