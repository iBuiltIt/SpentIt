# Fuzzing and recovery verification

## Coverage-guided QIF fuzzing

The harness covers ordinary parsing, whole-document indexing and each admitted account's parsing with DMY/MDY and JPY/CAD/KWD scales. It checks retained byte ranges, diagnostic bounds, original/effective total agreement and exact allocation sums. Invalid indexes may retain a partially scanned section and are never passed to account parsing. The separate `amount_repair` diagnostic bit is not a guarantee of structural repairability; `repairable` remains the admission gate.

The corpus is synthetic. Small seeds cover ordinary/class/transfer records, missing money, repeated account sections, malformed account boundaries and extreme signed amounts. The generator adds field/split/account count boundaries, invalid UTF-8 and BOM/CRLF. Generated/mutated corpus and crash artifacts remain under ignored `build/`, never routine private samples.

AppleClang's bundled compiler lacks libFuzzer. The optional pinned [compiler-rt runtime](../third_party/compiler-rt/README.md) provides it without a system installation. Python 3.12 or later is required for safe archive extraction:

```sh
curl -L --fail https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/compiler-rt-17.0.6.src.tar.xz -o /tmp/compiler-rt-17.0.6.src.tar.xz
python3 tools/build-libfuzzer.py /tmp/compiler-rt-17.0.6.src.tar.xz
cmake -S . -B build/qif-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPENTIT_SANITIZERS=ON -DSPENTIT_QIF_FUZZER=ON -DSPENTIT_FUZZER_RUNTIME="$PWD/build/libfuzzer-runtime/libFuzzer.a"
cmake --build build/qif-fuzz --target spentit_qif_fuzzer -j 2
python3 tools/prepare-qif-fuzz-corpus.py build/qif-fuzz/boundary-corpus
mkdir -p build/qif-fuzz/artifacts
build/qif-fuzz/tests/engine/spentit_qif_fuzzer build/qif-fuzz/boundary-corpus -dict=tests/fixtures/qif/fuzz.dict -seed=20261028 -max_total_time=300 -max_len=1048576 -len_control=0 -timeout=10 -rss_limit_mb=2048 -artifact_prefix=build/qif-fuzz/artifacts/
```

A bundled compiler runtime can instead omit `SPENTIT_FUZZER_RUNTIME`. Engine and harness use coverage instrumentation and ASan/UBSan. The standalone runtime uses ASan (matching libc++ container annotations), without coverage or UBSan instrumentation. No sanitizer option suppresses checks in the parser. A time-bounded campaign is not proof of absence of defects; retain and minimize any genuine parser failure, add a deterministic regression, then rerun.

## Recovery and native verification

Use the `app-release` test labels and fault runner described in
[building](building.md). Combine capture, repairs, duplicate decisions,
publication, edits, Undo and recovery on synthetic files when working on the
whole workflow. Check reopened state on both sides of the durable boundary
before retrying. A combined boundary scenario complements the exhaustive
write/sync sweeps.

Use the [agent diagnostics guide](agent-diagnostics-guide.md) for performance
work. Run timings without concurrent builds or test runs. A native scrolling
comparison requires an exposed window and frame samples in every measured
pass. Qt accessibility-tree checks and offscreen keyboard tests are separate
from native interaction and spoken screen-reader verification.
