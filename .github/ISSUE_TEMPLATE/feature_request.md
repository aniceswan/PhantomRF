---
name: ✨ Feature request
description: Suggest a new attack module, UI improvement, hardware support or anything else that would make PhantomRF more useful.
title: "[feature] "
labels: ["enhancement", "needs-triage"]
assignees: []
---

<!--
  Feature requests without context get deprioritised. Tell us **why**
  you need this and **how** you'd use it. The clearer the use case, the
  faster the design discussion.
-->

## Problem statement

What user pain does this feature solve? What can't you do today that you
want to be able to do?

> "As a **\[penetration tester / student / hardware hobbyist\]**, I want
> to **\[do X\]** so that I can **\[accomplish Y\]**."

## Proposed solution

Describe the feature in concrete terms. UI mockups, command-line examples
and protocol diagrams are all very welcome.

```
$ phantom-cli nrf24 sweep --from 0 --to 125 --method random --pa max
starting sweep on channels 0..125 ...
  [####################]  78%   channel 99 / 125   RSSI -54
  hit Ctrl+C to stop
```

## Alternatives considered

What other approaches did you think about? Why is the proposed solution
better? (Or: why might the alternatives be better?)

## Use cases

List the concrete scenarios where you would use this:

- **Use case 1**: ...
- **Use case 2**: ...
- **Use case 3**: ...

## Out-of-scope / non-goals

What should this feature **not** do? Keeping the scope tight avoids
scope creep.

## Affected modules

Which parts of the codebase would need to change?

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
- [ ] `hardware (schematic / PCB)`
- [ ] N/A — not sure

## Willingness to contribute

- [ ] I am willing to submit a PR for this feature
- [ ] I have already started a PR (link: ...)
- [ ] I would like to discuss the design first
- [ ] I am only requesting the feature, no PR planned

## Related

- Is this related to a tracked issue? (`#123`)
- Does it depend on another feature? (`#456`)
- Cross-references to PRs, discussions, or external specs.
