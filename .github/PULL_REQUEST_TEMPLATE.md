# Pull Request

<!--
  Thanks for contributing! A good PR description helps reviewers a lot.
  Please tick every box in the checklist at the bottom; the CI will not
  run unless your branch is up to date with the base branch.
-->

## What does this PR do?

One-paragraph summary of the change.

> Example: "Add BLE Apple Continuity spam module (F-B03) and wire it into
> the module registry, OLED menu and web UI. Includes 6 unit tests for
> the Apple payload builder."

## Related issue

Link the issue this PR fixes, implements or relates to. Use
`Fixes #123`, `Closes #456` or `Refs #789` so GitHub auto-links.

- Fixes #
- Closes #
- Refs #

## Type of change

Tick exactly one; add sub-bullets if needed.

- [ ] 🐛 **Bug fix** — non-breaking change that fixes an issue
- [ ] ✨ **New feature** — non-breaking change that adds functionality
- [ ] 💥 **Breaking change** — fix or feature that would cause existing
      functionality to change
- [ ] 📚 **Documentation** — docs only (no code change)
- [ ] 🧹 **Chore** — build, CI, tooling, refactor with no behaviour change
- [ ] ⏪ **Revert** — reverts a previous commit

## Affected modules

- [ ] `src/core/`
- [ ] `src/hal/`
- [ ] `src/radio/`
- [ ] `src/modules/Nrf24Jammer/`
- [ ] `src/modules/Cc1101Jammer/`
- [ ] `src/modules/WifiAttack/`
- [ ] `src/modules/BleAttack/`
- [ ] `src/modules/Spectrum/`
- [ ] `src/ui/web/`
- [ ] `src/ui/oled/`
- [ ] `src/ui/cli/`
- [ ] `src/utils/`
- [ ] `tests/`
- [ ] `docs/`
- [ ] `tools/` / `scripts/`
- [ ] N/A

## How was this tested?

Describe the test setup and what you ran. Be specific — "it works" is
not a test report.

- [ ] `pio test -e native` — host-side unit tests
- [ ] `pio run -e esp32s3-n16r8` — primary env builds cleanly
- [ ] `pio run -e esp32-classic` — classic ESP32 builds
- [ ] `pio run -e esp32c3` — ESP32-C3 builds
- [ ] Manual on real hardware — describe which board, which scenario
- [ ] `pio check` (clang-tidy) — no new findings
- [ ] `./tools/lint/run-clang-format.sh` — code is formatted
- [ ] No new warnings under `-Wall -Wextra -Werror`

## Screenshots / recordings

If your PR changes the web UI, OLED layout or any visual output, attach
before/after screenshots or short `.gif`/`.mp4` recordings.

## Checklist

Author:

- [ ] My code follows the project's [CONTRIBUTING.md](../../blob/main/CONTRIBUTING.md) style
- [ ] I have run `clang-format` and `clang-tidy` on the changed files
- [ ] I have added unit tests for new functionality (Unity, in `tests/`)
- [ ] I have updated relevant docs in `docs/`
- [ ] I have updated the [CHANGELOG.md](../../blob/main/CHANGELOG.md) under "Unreleased"
- [ ] My changes do not introduce new compiler warnings
- [ ] I have rebased on the latest `main` / `develop`
- [ ] Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/)

Reviewer:

- [ ] At least one maintainer approval
- [ ] CI is green (build / test / lint workflows)
- [ ] Breaking changes are documented in the PR description
- [ ] No secrets, keys or credentials are committed
