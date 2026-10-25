# SQLCipher prototype dependency

Selected for the isolated prototype by the owner on 2026-10-25. This does not select the final application architecture or SpentIt license.

- Upstream: [SQLCipher Community Edition](https://github.com/sqlcipher/sqlcipher).
- Pin: release tag **v4.19.0**, SQLite **3.53.4**.
- [Source archive](https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz), retrieved 2026-10-25.
- Archive SHA-256: `7075f96cbabe45b4ecfc2e6b1745a625f856f695b0827a5506ce9ed85b906aa0`.
- Generated `sqlite3.c` SHA-256: `8640c653acadf665cce6331646f60b5b74a4690746f2c4a2d8f688a0570a0c0c`.
- Generated `sqlite3.h` SHA-256: `8a9d1bff44d75174ca6dea3ea9bac50a6104d86facb566647b8bb839375b7b3a`.
- Upstream code patches: none. Source and generated dependency files stay in ignored build directories. CMake checks the generated hashes before compiling, and the store checks the runtime SQLCipher version. Engine-only builds need none of this dependency.
- SQLCipher license: upstream BSD-style three-clause terms, copied unmodified in [LICENSE.md](LICENSE.md). SQLite's upstream public-domain material is included in the source archive; retain upstream notices in distribution review.
- Crypto provider used for verification: installed **OpenSSL 3.5.9**, Apache-2.0, through `OpenSSL::Crypto`. OpenSSL is a separately supplied system dependency, not downloaded or version-pinned by this prototype. CMake requires OpenSSL 3.x or newer compatible with that minimum; the exact tested version is recorded, not a reproducible full dependency closure. A shipping build must pin/package/review it separately and retain its notices.

No project license has been added. Third-party terms do not inherit the owner's intended SpentIt resale restriction. See [licensing requirements](../../features/007-licensing-and-distribution.md).

Build and test instructions: [storage prototype](../../doc/storage-prototype.md).
