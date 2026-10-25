# Engine split-editing contract

Last updated: 2026-10-25

The second engine milestone adds `SplitEditor`, an in-memory value that wraps a validated `Transaction` and designates one allocation by its current index as the remainder. It is independent of UI and storage. The UI will need a separate temporary text-input state; this API does not accept invalid monetary text or persist keystrokes.

## Operations and invariants

- Construction and `designate_remainder(index)` preserve every allocation; neither recalculates nor discards an imported mixed-sign structure. The index must exist.
- `edit(index, allocation)`, `append(allocation)`, and `remove(index)` modify the explicitly selected allocation and recalculate only the designated remainder amount. Its category and memo remain intact; all other allocations retain their values and order. Removal before the remainder adjusts its index.
- The remainder cannot be directly edited or removed. Designate another allocation first. A single-allocation transaction therefore always retains its one remainder. Adding a split starts by appending a manual allocation. Metadata-only editing of the remainder is not yet provided.
- The remainder equals the effective total minus all explicitly entered amounts, exactly. Cancellation occurs before the final signed-64-bit range check, so neither a large subtotal nor negating the signed minimum creates a false overflow. A genuinely unrepresentable remainder is rejected.
- Ordinary mode permits zero allocations/remainders and rejects a result containing both positive and negative allocations with `mixed_sign_choice_required`. This includes entering a negative deduction on a positive transaction, crossing through zero into the opposite sign, and creating nonzero offsetting allocations for a zero total. The caller can present the choice and retry on `allow_mixed_signs()`.
- Existing mixed-sign transactions initialize with mixed signs enabled. Explicit enabling persists for that editor value and its descendants, including if the values later become same-sign. Opening a new editor derives the mode from its current allocations. No mixed-sign preference is persisted.
- `correct_total(total)` preserves entered allocations, adjusts only the remainder, and retains the original imported amount through repeated corrections. The same sign-choice and range rules apply. Manual transactions still have no imported amount. Correcting a total does not create history or perform any storage operation.
- Every operation returns a new validated value. On errors, the input editor/transaction is unchanged. No invalid intermediate transaction is exposed for saving. Currency differences, bad indices, protected remainder edits, and overflow produce explicit errors.

The mixed-sign choice is a workflow guard, not a ban in the transaction model: `Transaction` still accepts valid mixed-sign allocations directly, including imports. The synthetic sheet experiment now offers an explicit mixed-sign checkbox; production UI/persistence remains unimplemented.

## Example

```cpp
#include <spentit/split_editor.hpp>
using namespace spentit;

const Currency cad("CAD"); // scale 2 from the pinned ISO catalog
const auto original = Transaction::imported(
    Money(10000, cad), {{"Uncategorized", "", Money(10000, cad)}});
const SplitEditor editor(original, 0);
const auto split = editor.append({"Transport", "Ticket", Money(2500, cad)});
// Remainder is 7500; original imported total is still 10000.
const auto corrected = split.correct_total(Money(11000, cad));
// Transport stays 2500, remainder becomes 8500, original remains 10000.
```

See [feature 002](../features/002-accounts-money-splits.md), [currency snapshot provenance](../third_party/iso4217/README.md), and [build/test results](building.md). Accounts, history, storage, UI confirmation, historical-currency policy, and conversion remain deferred.
