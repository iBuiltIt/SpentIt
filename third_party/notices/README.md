# Offline third-party notices

Recorded: 2026-11-08. These are notices, not a SpentIt license or a claim of
completed distribution compliance. The application embeds them for offline reading.

## Provenance

- Qt Base and Qt Declarative catalogs are assembled from the unmodified attribution
  JSON and referenced license files in the upstream v6.12.0 source archives.
  They retain a superset of runtime-source notices, including other-platform code;
  test/build-tool and Wayland protocol catalogs are excluded. Each entry carries its
  upstream source URL. LGPL v3 and GPL v3 are copied verbatim from Qt Base LICENSES.
- LGPL 2.1 is copied from Qt Base v6.12.0 LICENSES/LGPL-2.1-or-later.txt.
- SQLCipher and PCRE2 reuse the existing reviewed repository license files.
- SQLite text comes from SQLCipher v4.19.0 SQLITE_LICENSE.md.
- macOS transitive libraries were enumerated through otool -L recursively from
  the application and Cocoa platform plugin. Their installed package license files
  are copied verbatim, except GLib and libintl where the upstream header notice is
  retained alongside the complete LGPL 2.1 text. Manifest versions include package
  revisions when supplied. FreeType FTL is copied from the upstream VER-2-15-0
  docs/FTL.TXT; its installed license overview is also included.
- Source archives: https://github.com/qt/qtbase/archive/refs/tags/v6.12.0.tar.gz
  and https://github.com/qt/qtdeclarative/archive/refs/tags/v6.12.0.tar.gz.

## Updating and limits

`tools/prepare_about.py` embeds this manifest; it does not download at build/runtime.
The macOS extra-library profile checks installed Homebrew package versions relative
to the selected Qt installation. Mismatches are explicitly reported in the viewer
instead of labeling a new library with an old version. Other platforms need their
own transitive inventory. Qt catalogs identify their reviewed 6.12.0 version.

Before distribution, inspect the final deployed libraries/plugins and reference-data
terms, refresh matching upstream texts, and satisfy applicable library-source and
replacement/relinking requirements. Packaging may add components absent from the
development executable; About alone is not that audit. OS-provided libraries are
not copied into this notice set. No user file or financial data is consulted.

## Recorded file digests

| File | SHA-256 |
| --- | --- |
| GPL-3.0-only.txt | `8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903` |
| LGPL-2.1-or-later.txt | `1ccf09bf2f598308df4bed9cd8e9657dc5cd0973d2800318f2e241486e2edf3f` |
| LGPL-3.0-only.txt | `da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768` |
| brotli.txt | `3d180008e36922a4e8daec11c34c7af264fed5962d07924aea928c38e8663c94` |
| double-conversion.txt | `4af93c12062c58058378de2397dc1c92bbff9ddfb1d583a01c84127557ce97ca` |
| freetype-FTL.txt | `5a5ee54c5001bbad1cdc1a57cc3dd4c42199b2da09d39c7ee41fab002d02967f` |
| freetype.txt | `bd36c8b474855fa294c2ec5c184544478ef3720aad37d65a6296a4f264fd2d3b` |
| gettext.txt | `82aad9267e03a80e821e9d6e3268d0e97a531182f61bda7c4fd0d640b7ba66fd` |
| glib.txt | `c2be705bc6ff946dbd33ec19f8ae14a6ed0798765926b573b0aee7d780a46ff8` |
| graphite2.txt | `f01990a91e9fe909196f521211df1a1f78ce59066c2d095da7c299e9181abcc2` |
| harfbuzz.txt | `ba8f810f2455c2f08e2d56bb49b72f37fcf68f1f4fade38977cfd7372050ad64` |
| icu4c-79.txt | `e09827667e0bb3c808b2a463548ba814a5e6249b67353234d70c8dfc2bcc417d` |
| libb2.txt | `6a1ee543e5282cd9061881edf462e6fdab181f328da71fc2c9a6950a80e94d01` |
| libpng.txt | `bdb0a645ea18c60507d0368379b1ac5474b92255fcc2d115e07486a7672ba526` |
| md4c.txt | `e547c9a66c120b19f9838a1f7c2f51137f5c71ba27b70c3611b673f2cdb4e02e` |
| openssl-3.txt | `7d5450cb2d142651b8afa315b5f238efc805dad827d91ba367d8516bc9d49e7a` |
| openssl-4.txt | `7d5450cb2d142651b8afa315b5f238efc805dad827d91ba367d8516bc9d49e7a` |
| qtbase-notices.txt | `7256cd5a9fbcc75cbbcdfab1f1b68684c21133287bf4ed746a598e253049fea8` |
| qtdeclarative-notices.txt | `44ce3668e7caad3bf2d877c839da444f30d5f48f643b5551e92f7946c8219efd` |
| sqlite.txt | `d513445ea7b860648cc52b688f472272eb063fdd6d010eb4cf2a5829ffd5e0be` |
| zstd.txt | `7055266497633c9025b777c78eb7235af13922117480ed5c674677adc381c9d8` |
