# 009 — Durable import

Status: **active**
Last updated: 2026-11-09

The durable import workflow is the application, not a separate experiment.
Behavior is in [feature 003](003-qif-import-review.md), the
[QIF profile](009-qif-profile.md), the [statement profile](003-ofx-profile.md),
and [doc/qif-account-imports.md](../doc/qif-account-imports.md).

The owner has private QIF samples and matching QFX, ASO, QBO, and OFX exports
of one real-world dataset. Ask for access when a structural question needs
them. Do not record their paths, filenames, or contents, do not move the
originals, and do not commit them. Prefer synthetic fixtures. Private samples
are not routine test inputs.
