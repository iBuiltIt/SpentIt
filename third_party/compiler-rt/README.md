# Optional libFuzzer runtime

The QIF fuzz harness can use the compiler's bundled libFuzzer. The tested AppleClang installation does not include that runtime. An optional local build uses LLVM compiler-rt **17.0.6**:

- Source: [official LLVM release archive](https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/compiler-rt-17.0.6.src.tar.xz).
- SHA-256: `11b8d09dcf92a0f91c5c82defb5ad9ff4acf5cf073a80c317204baa922d136b4`.
- License: Apache-2.0 with LLVM exception; the downloaded archive retains its upstream `LICENSE.TXT`.
- No source changes. Source/object/archive output stays under ignored `build/` directories. This developer-only runtime is not linked into the application or shipped as part of it.
- [Build helper](../../tools/build-libfuzzer.py) verifies the archive, uses C++17 and two compiler jobs, and refuses a failed compilation. It instruments the runtime with ASan to match libc++ container annotations. The runtime is not coverage/UBSan-instrumented; the engine and harness are instrumented with coverage, ASan and UBSan.

See the [hardening guide](../../doc/hardening.md) for the complete reproduction commands and campaign limits.
