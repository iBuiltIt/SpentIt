# 015 — File exchange and backup entry point

Status: **active**  
Last updated: 2026-11-09

The owner accepted and authorized four File menu commands: QIF Export, SpentIt
Export, SpentIt Import and Backup. The [agreed design and versioned file format](../doc/file-exchange.md)
are authoritative; [status](../doc/status.md) records implementation and actual checks.

- QIF exports selected accounts' current ledger transactions within individual
  inclusive ranges. Include categories is optional; transfer references remain.
  Include opening balance defaults on and exports the immediately-before-period
  balance, or the stored opening for the full range.
- SpentIt Export independently selects Rules, Reports and Categories. SpentIt
  Import shows only nonempty sections, stacked vertically, with individual choices
  and All on/off per section. Categories merge by full name with explicit Income
  conflict choices. Rules and reports append as new definitions, including namesakes.
- Referenced categories/accounts must match or be explicitly mapped. Missing
  categories can be created only when selected from the imported category section.
  Selected definitions save atomically; importing rules never applies transactions.
- Export accounts and all three definition groups default on. Progress and errors
  appear inside the exchange dialogs. QIF output normalizes text controls to spaces
  without changing imported/stored text; see the contract for exact characters and
  account-name collision protection.
- Backup is the existing Settings backup operation, exposed from File.

Plaintext exports are an intentional reconstruction path. They do not preserve all
originals, recognition, history or document settings and do not replace encrypted
backup. This scope does not authorize database migrations, changing rule matching
semantics, report-results export, or the deferred manual publication pass (MAN-018).

Acceptance includes exact QIF amounts/splits/openings and current-row scope;
round-trip definitions and dependency mapping; atomic failure/retry/crash recovery;
malformed/untrusted-file rejection; protected export publication; menu/dialog
selection and unsaved-work guards. Platform and native-picker limits must remain
explicit in the verification record.
