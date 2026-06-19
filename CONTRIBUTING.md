# Contributing to PhantomRF

> Thank you for investing your time in PhantomRF. Whether you're fixing a
> typo, refactoring a function, or adding a brand-new attack module — every
> contribution is welcome and appreciated.

This guide covers everything you need to get your change merged:

1. [Code of conduct](#1-code-of-conduct)
2. [Filing issues](#2-filing-issues)
3. [Submitting pull requests](#3-submitting-pull-requests)
4. [Coding style](#4-coding-style)
5. [Commit messages](#5-commit-messages)
6. [Testing requirements](#6-testing-requirements)
7. [Documentation requirements](#7-documentation-requirements)
8. [Licensing](#8-licensing)
9. [Recognition](#9-recognition)

If anything below is unclear, [open an issue](https://github.com/anomalyco/PhantomRF/issues/new/choose) and we will improve this document.

---

## 1. Code of conduct

This project follows the [Contributor Covenant v2.1](./CODE_OF_CONDUCT.md).
By participating, you agree to uphold it. Be patient with newcomers,
generous with credit, and ruthless only with bugs and bad code — never
with people.

---

## 2. Filing issues

Before opening a new issue:

1. **Search the existing issues** to avoid duplicates.
2. **Try the latest `main`** — your bug may already be fixed.
3. **Read the [docs/](./docs)** — the answer might be there.

When you do open an issue, use the appropriate template:

- **🐛 [Bug report](./.github/ISSUE_TEMPLATE/bug_report.md)** — for
  incorrect behaviour, crashes, build failures, regressions.
- **✨ [Feature request](./.github/ISSUE_TEMPLATE/feature_request.md)** —
  for new attacks, UI improvements, hardware support.

For **security vulnerabilities**, do **not** open a public issue. Follow
[SECURITY.md](./SECURITY.md) and email `security@example.com` (or use the
GitHub Security tab).

---

## 3. Submitting pull requests

The full PR template is in
[.github/PULL_REQUEST_TEMPLATE.md](./.github/PULL_REQUEST_TEMPLATE.md).
The condensed version:

1. **Fork the repo** and create a topic branch:
   ```bash
   git checkout -b feature/my-cool-attack
   ```
2. **Make your change.** Commit early, commit often.
3. **Run all the checks locally** (see [§6](#6-testing-requirements)):
   ```bash
   pio run -e esp32s3-n16r8
   pio test -e native
   ./tools/lint/run-clang-format.sh
   ./tools/lint/run-clang-tidy.sh
   ```
4. **Push to your fork** and open a PR against `main` (or `develop` for
   work-in-progress). Fill in the template — every box.
5. **Wait for CI.** The build / test / lint workflows must be green.
6. **Address review feedback.** Force-pushes are fine while the PR is
   still in draft. Once review starts, prefer adding fix-up commits so
   reviewers can see the diff history.
7. **Squash-merge.** The maintainer will squash your commits on merge;
   your PR title becomes the commit message. Make it count (see
   [§5](#5-commit-messages)).

### Review SLA

We aim to give a first response within **7 days** of opening a PR. If
yours has been silent for longer than that, feel free to nudge us on
Discord (link in the README) or by `@`-mentioning a maintainer in the
PR thread.

---

## 4. Coding style

### Language

- **C++17**, no exceptions. We compile with `-std=c++17` everywhere.
- **No `.ino` files.** Everything is `.h` / `.cpp`.
- **No `new` / `delete` in application code** — use RAII, `std::unique_ptr`,
  or the module registry's ownership model.
- **No dynamic allocation in attack loops.** Jittering a heap allocation
  inside a 100 µs-tight channel hop will cost you a packet loss bug.

### Formatting

The repository pins clang-format-14; the project's style is in
[.clang-format](./.clang-format) (LLVM base, 4-space indent, 120-col
limit, `Attach` braces, sorted includes).

Run the formatter check before every commit:

```bash
# Verify (CI does the same)
./tools/lint/run-clang-format.sh

# Auto-fix in place
find src tests -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.c' \) \
    -print0 | xargs -0 clang-format-14 -i
```

### Static analysis

[.clang-tidy](./.clang-tidy) enables `bugprone-*`, `cert-*`,
`cppcoreguidelines-*`, `modernize-*`, `performance-*`, `portability-*`
and `readability-*` (with the noisy checks turned off individually —
see the config for the full list).

```bash
./tools/lint/run-clang-tidy.sh
# or, through PlatformIO:
pio check -e esp32s3-n16r8
```

### Naming

We follow the LLVM convention with project-specific tweaks:

| Element | Style | Example |
|---|---|---|
| Namespaces | `lower_case` | `phm::util` |
| Classes / structs / enums | `CamelCase` | `Nrf24Module` |
| Free functions | `camelBack` | `wifiChannelToNrf24Range` |
| Variables | `lower_case` | `current_channel` |
| Parameters | `lower_case` | `wifi_ch` |
| Constants / `constexpr` | `UPPER_CASE` | `NRF24_MAX_CHANNEL` |
| Class members | `lower_case_` (trailing underscore) | `rssi_` |
| Private members | `lower_case_` (trailing underscore) | `state_` |
| Macros | `UPPER_CASE` | `PHM_BOARD_ESP32S3` |
| Header guards | `#pragma once` | (no include guard macro) |

### Headers

- All public headers use `#pragma once`.
- Include order is dictated by clang-format: same-project first, then
  third-party libs, then `<...>` stdlib.
- Don't `#include` what you don't use. IWYU is enforced by
  `cppcoreguidelines-*` clang-tidy checks.

### Comments

- Doxygen comments on every public function:
  ```cpp
  /**
   * @brief One-line summary ending with a period.
   *
   * Longer description, if needed. Multi-paragraph is fine.
   *
   * @param x Description of the parameter.
   * @return Description of the return value.
   */
  ```
- No `//` line-end comments on the same line as code. Put the comment
  on the line above.
- TODO comments must be referenced by issue: `// TODO(#123): ...`.

### Error handling

- Use `phm::core::Result<T, E>` for fallible operations (defined in
  `src/core/Result.h`).
- Don't use exceptions. They are not enabled on the ESP32 Arduino core
  and the build will fail if you turn them on.
- Don't silently swallow errors. At minimum, log via `phm::Logger`.

### Concurrency

- Pin FreeRTOS tasks to a specific core (see DESIGN §3.3).
- Disable the core watchdogs in `setup()` if your task intentionally
  blocks for more than 5 s. Document why.
- Lock-free structures are preferred for cross-core queues; see
  `src/utils/RingBuffer.h`.

---

## 5. Commit messages

We follow **[Conventional Commits](https://www.conventionalcommits.org/)**.
The format is:

```
<type>(<scope>): <short summary>

<body>

<footer>
```

### Type

| Type | When to use |
|---|---|
| `feat` | New user-facing feature (e.g. a new attack module) |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `style` | Formatting, no logic change |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `perf` | Performance improvement |
| `test` | Adding or fixing tests |
| `build` | Build system, CI, scripts |
| `chore` | Maintenance (deps, .gitignore, etc.) |
| `revert` | Reverts a previous commit |

### Scope

A short, lower-case label naming the affected module:

- `core`, `hal`, `radio`, `nrf24`, `cc1101`, `wifi`, `ble`, `spectrum`,
  `settings`, `web`, `oled`, `cli`, `utils`, `tests`, `docs`, `ci`.

### Examples

```
feat(nrf24): add BLE data channel jam (F-N03)

Implements a sweep over channels 2..80 (even-only) using constant carrier.
Each channel is held for ~500 µs before hopping. Uses CONT_WAVE | PLL_LOCK
per DESIGN §10.1.

Refs #42
```

```
fix(settings): prevent NVS crash on empty SSID list

The previous code dereferenced a null pointer when the user cleared all
entries via the web UI. Now we early-return with a typed error.

Fixes #87
```

```
docs(readme): add link to web flasher
```

### Rules

- **Imperative mood** in the summary line ("add", not "added" or
  "adds").
- **No period** at the end of the summary.
- **72 chars** max for the summary line.
- **Reference issues** in the footer with `Fixes #`, `Closes #`,
  `Refs #`.

### Allowed (and encouraged) exceptions

- `chore:` commits during development that you intend to squash away
  on merge.
- `wip:` prefix on draft branches. PRs marked WIP are not expected
  to be merge-ready; they signal "I want feedback on direction".

---

## 6. Testing requirements

Every PR must include or update unit tests for any non-trivial change.
We use the [Unity](https://www.throwtheswitch.org/unity) test framework,
which PlatformIO ships with.

### Where tests live

```
tests/
├── test_channel_math.cpp       # src/utils/ChannelMath.*
├── test_frame_builder.cpp      # src/utils/FrameBuilder.*
├── test_settings.cpp           # src/modules/Settings/*
├── test_<your_module>.cpp      # your new test
├── native_mock.{h,cpp}         # Arduino / ESP32 mocks for host testing
└── unit/CMakeLists.txt         # for non-PlatformIO IDEs
```

### Running tests

```bash
# PlatformIO looks for tests in `test/` by default, but PhantomRF
# stores them in `tests/`. Create a one-line symlink so `pio test`
# finds them:
ln -s tests test

# Host-side (fast, no hardware)
pio test -e native

# Verbose host-side
pio test -e native -v

# Specific test
pio test -e native -f test_channel_math
```

If you'd rather not use a symlink, add `test_dir = tests` to the
`[platformio]` section of `platformio.ini`. The symlink approach is
what the CI workflow uses, so no extra configuration is needed there.

### Writing a test

Tests are organised as one **suite per source file**, with one
`setUp()` / `tearDown()` pair per suite. See the existing
[tests/test_channel_math.cpp](./tests/test_channel_math.cpp) for the
canonical example.

```cpp
#include <unity.h>
#include "utils/ChannelMath.h"

void setUp(void) {
    // per-test fixture
}

void tearDown(void) {
    // per-test cleanup
}

void test_nrf24_channel_zero_is_2400_mhz(void) {
    TEST_ASSERT_EQUAL_UINT16(2400u, phm::util::nrf24ChannelToFreq(0u));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_nrf24_channel_zero_is_2400_mhz);
    return UNITY_END();
}
```

### What to test

- All public functions in any header under `src/`.
- All branch points in a new module: success path, error path, edge
  cases.
- Anything that previously had a bug — add a regression test in
  the same commit that fixes the bug.

### Coverage targets

- New modules: **≥ 80 % line coverage** of the production code.
- Bug fixes: a regression test that fails without the fix.

We don't enforce coverage in CI yet (it's noisy on Arduino code), but
reviewers will check it manually for substantial PRs.

---

## 7. Documentation requirements

- **Public API changes** must update the Doxygen comments in the
  affected header(s).
- **User-facing changes** (new CLI command, new web-UI button, new
  OLED screen) must update:
  - the relevant file under [docs/](./docs), and
  - the [CHANGELOG.md](./CHANGELOG.md) under the `## [Unreleased]`
    heading.
- **Hardware changes** (new pin assignment, new board variant) must
  update [docs/hardware/](./docs/hardware) and the [docs/reference/pinout.md](./docs/reference/pinout.md).
- **Algorithm changes** (different sweep strategy, different
  register sequence) must update [docs/algorithms/](./docs/algorithms).

When in doubt, write more docs. The whole point of PhantomRF is
being understandable.

---

## 8. Licensing

PhantomRF is released under the **MIT License**. By submitting a pull
request, you agree to license your contribution under the same MIT
License that covers the project.

If you are contributing on behalf of an employer, please confirm in
the PR description that your employer has agreed to this dual-licensing
(employer retains copyright, contributor grants MIT license to the
project).

We do **not** require a CLA. The MIT License is permissive enough that
we don't need a separate paper trail.

### Third-party code

If your PR includes code copied from another project, the commit
message must say so and credit the original author and license. The
license must be compatible with MIT (MIT, BSD-2, BSD-3, Apache-2,
ISC, CC0, public domain). GPL, AGPL, LGPL, and SSPL are **not**
compatible — do not include code under those licenses.

---

## 9. Recognition

We maintain a [list of contributors](./README.md#contributors) in the
README, ordered by total contribution size. The list is regenerated
automatically on every release by a CI job that runs
[`git-shortlog`](https://git-scm.com/docs/git-shortlog) over the
release range.

If you'd rather not be listed, add the following to your
`.git/config` user section and we will skip you:

```ini
[user]
    name = Your Name
    email = you@example.com
    # PhantomRF: do-not-list
```

Or open an issue asking to be removed and we will do it.

---

## 10. Additional resources

- [DESIGN.md](./DESIGN.md) — the architecture document. **Read this
  first** if you're going to add a new module.
- [docs/algorithms/](./docs/algorithms) — algorithm-level deep dives
  (nRF24 constant carrier, CC1101 async-serial, 802.11 frame
  construction, etc.).
- [docs/reference/](./docs/reference) — the auto-generated command
  reference and the API reference.
- [SECURITY.md](./SECURITY.md) — how to report vulnerabilities.
- [LICENSE](./LICENSE) — the MIT license text.

Welcome aboard — and happy hacking! 🎉
