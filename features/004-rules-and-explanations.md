# 004 — Rules and explanations

Status: **active**  
Last updated: 2026-10-25  
Implementation: none; requirements and proposed design only.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). These are future requirements, not implemented category behavior.

- Regex matching is a core requirement.
- Rules are displayed and edited in a sheet-style interface.
- Rules can normalize displayed payees, assign categories, and populate memos from source data.
- Rules address transaction and split fields, including split memos. Split classification can use the parent payee's classification.
- Show rule collisions/overlaps and their consequences; ordering may resolve intentional overlaps.
- A transaction with a matching rule offers rule editing; an unmatched transaction offers rule creation. Rule editing has explicit commit/discard controls.
- Preserve original import fields and explain the rules responsible for derived values.

## Proposed design

Use PCRE2 directly where needed to control execution limits. A rule includes identity, enabled state, priority, scope, input field, regex/options, actions, and continuation behavior. Record the applied rule version and field contributions so explanations remain meaningful after later rule edits.

Evaluate matches against defined source fields by default. Make parent-category inheritance explicit; do not replace source split memos or recursively rerun arbitrary rules. Specify phases/dependencies before implementation. Capture replacement, case sensitivity, Unicode semantics, and empty-field behavior require explicit contracts.

Preview changes before applying edited rules. Do not silently recategorize committed history. When several rules contributed, show each rule and its changes rather than pretending one rule explains the entire row. Preserve manual overrides unless the user explicitly chooses to replace them.

Classify feedback separately: multiple rules matching can be compatible; competing assignments to the same field can be a conflict; later matches can be shadowed by an earlier stopping rule. Evaluate concrete overlaps on existing transactions and test cases. Do not claim exhaustive overlap detection for arbitrary PCRE2 expressions or unseen inputs.

Example: a payee matching `Tfl Travel.*` yields Transport; a split memo matching `Wise Charges for:.*` yields Service Fees; a split matching `CARD-.*` explicitly inherits Transport from its parent's classification.

Bound pattern/input size and matching resources, provide cancellation, and keep evaluation off the UI thread. A compilation error or resource-limit failure is a visible error, not a non-match or permission to commit partially classified results silently.

## Acceptance criteria

- Transaction-field and split-field rules produce deterministic, explainable results on shared fixtures.
- The synthetic Wise-shaped import allocates its fee and transport amount correctly without source mutation.
- Invalid patterns and resource exhaustion are surfaced without hanging the UI.
- Reordering rules previews affected transactions and competing field assignments.
- Compatible overlaps, conflicting writes, and shadowed rules have distinct feedback.
- Rule edit/discard leaves existing rules/results unchanged when cancelled; reapplication is explicit and reviewable.
- Historical explanations remain valid after rules change or are deleted.

## Open questions

- First-match, last-match, or per-field precedence; explicit stop/continue defaults.
- Parent classification phase and dependency-cycle rejection.
- Regex flags, replacement syntax, rule versioning, manual-override semantics, and transaction/split selection in the editor.
- Scope of rule commit versus draft commit: rule changes may be document-global and must not be ambiguously coupled to discarding an import.

## Reference

- [PCRE2 API and resource controls](https://www.pcre.org/current/doc/html/pcre2api.html)
