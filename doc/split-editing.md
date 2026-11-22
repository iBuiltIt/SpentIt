# Engine split-editing contract

`SplitEditor` wraps a validated `Transaction` and designates one allocation by
its current index as the remainder. It works on validated values in memory.
See [ledger split editing](ledger-editing.md#split-amount-editing) for the
production editor and persistence workflow.

## Operations and invariants

- Construction and `designate_remainder(index)` preserve every allocation; neither recalculates nor discards an imported mixed-sign structure. The index must exist.
- `edit(index, allocation)`, `append(allocation)`, and `remove(index)` modify the explicitly selected allocation and recalculate only the designated remainder amount. Its category and memo remain intact; all other allocations retain their values and order. Removal before the remainder adjusts its index.
- The remainder cannot be directly edited or removed. Designate another allocation first. A single-allocation transaction therefore always retains its one remainder. Adding a split starts by appending a manual allocation. Metadata-only editing of the remainder is not yet provided.
- The remainder equals the effective total minus all explicitly entered amounts, exactly. Cancellation occurs before the final signed-64-bit range check, so neither a large subtotal nor negating the signed minimum creates a false overflow. A genuinely unrepresentable remainder is rejected.
- Ordinary mode permits zero allocations/remainders and rejects a result containing both positive and negative allocations with `mixed_sign_choice_required`. This includes entering a negative deduction on a positive transaction, crossing through zero into the opposite sign, and creating nonzero offsetting allocations for a zero total. The caller can present the choice and retry on `allow_mixed_signs()`.
- Existing mixed-sign transactions initialize with mixed signs enabled. Explicit enabling persists for that editor value and its descendants, including if the values later become same-sign. Opening a new editor derives the mode from its current allocations. No mixed-sign preference is persisted.
- `correct_total(total)` preserves entered allocations, adjusts only the remainder, and retains the original imported amount through repeated corrections. The same sign-choice and range rules apply. Manual transactions still have no imported amount. Correcting a total does not create history or perform any storage operation.
- Every operation returns a new validated value. On errors, the input editor/transaction is unchanged. No invalid intermediate transaction is exposed for saving. Currency differences, bad indices, protected remainder edits, and overflow produce explicit errors.

`Transaction` accepts valid mixed-sign allocations directly, including imports.
The explicit sign choice belongs to the editing workflow.

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

See [account money](accounts.md) and [currency snapshot provenance](../third_party/iso4217/README.md).
