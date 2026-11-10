#pragma once

#include "spentit/action_history.hpp"
#include <string_view>

struct sqlite3;

namespace spentit::storage {

// Internal persistence primitive. The caller owns the SQLCipher transaction and
// must restore/validate the domain payload before calling reversed(). This class
// never executes a financial inverse or commits a transaction.
class ActionChain {
public:
    explicit ActionChain(sqlite3* db) : db_(db) {}
    // The document's existing session and mutation-generation tables are the
    // sole session/epoch authority and must already exist.
    void initialize();
    app::ActionToken token() const;
    std::optional<app::ActionSummary> head() const;
    std::vector<app::ActionSummary> summaries(std::int64_t before, std::size_t limit) const;
    std::optional<app::ActionSummary> preparing() const;
    void require_current(const app::ActionToken&) const;
    void require_idle() const;
    void require_empty() const;

    // Request digests include command kind, target, token and domain arguments,
    // canonicalized by the domain adapter. A retry
    // is resolved before token validation, even after Undo or session expiry.
    enum class Outcome { applied, undone, cancelled, no_change, expired, preparing };
    std::optional<Outcome> outcome(std::string_view id, std::string_view digest) const;
    std::optional<Outcome> durable_outcome(std::string_view id, std::string_view digest) const;
    void applied(const app::ActionToken&, const app::ActionSummary&, std::string_view digest);
    void no_change(const app::ActionToken&, std::string_view id, std::string_view digest);
    void compatible_change(const app::ActionToken&);
    void begin_import(const app::ActionToken&, const app::ActionSummary&, std::string_view digest);
    void import_stage(std::string_view id, app::ImportStage);
    app::ImportStage import_stage(std::string_view id) const;
    void finish_import(std::string_view id, bool cancelled);
    void reversed(const app::ActionToken&, std::string_view operation, std::string_view digest);
    // Remove one applied action from the chain after its domain restore. The
    // action need not be the head. The caller checked the session token once.
    void detached(std::string_view action, std::string_view operation, std::string_view digest);
    void expire(const app::ActionToken&);
    void verify() const;

private:
    sqlite3* db_;
    void transaction() const;
    void advance();
    bool retry(std::string_view id, std::string_view digest, int kind) const;
    void insert(const app::ActionSummary&, std::string_view digest, bool preparing);
};

} // namespace spentit::storage
