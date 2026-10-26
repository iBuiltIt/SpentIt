# PCRE2 rules dependency

The optional `SPENTIT_RULES` target uses PCRE2 **10.49**, checked through pkg-config at configuration and through the runtime version/Unicode capability at pattern compilation. Engine-only builds do not require it. The current development build uses the separately installed library; this is not yet a reproducible packaged dependency closure.

- Upstream release: [PCRE2 10.49](https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.49).
- Public interfaces: [API and resource limits](https://www.pcre.org/current/doc/html/pcre2api.html), [pattern semantics](https://www.pcre.org/current/doc/html/pcre2pattern.html).
- The upstream license is retained unchanged in [LICENSE.md](LICENSE.md), copied from the installed 10.49 distribution. SHA-256: `197d8a73ffee0d6b09adba2f9c677b5f5aede24edf89258a68e48248d010d811`.
- No upstream patches. No JIT usage. The adapter uses UTF-8, Unicode properties, default case-insensitive search, capture bounds, compilation/match memory budgets, match/depth limits and cancellation callouts. Invalid input and exhausted resources are errors rather than non-matches.
- Templates use literal text, `$$` and numeric `${n}` capture references. Unknown references fail; nonparticipating optional captures expand to empty text. Backslash-C is disabled so captures cannot split UTF-8 code units.

These third-party terms do not select SpentIt's license. Packaging and Windows/Linux validation remain future work.
