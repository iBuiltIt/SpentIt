#pragma once
#include <spentit/import_store.hpp>
#include <optional>

namespace test_session {
// Explicit successful close/reopen for fixtures testing cross-session removal.
// Moving the connection into an optional releases its lock before reopening.
inline void close_reopen(spentit::storage::ImportStore& store,
                         const std::filesystem::path& path,std::string_view password) {
    const auto account=store.account().identity;
    std::optional<spentit::storage::ImportStore> connection{std::move(store)};
    connection->certify({});
    connection.reset();
    store=spentit::storage::ImportStore::open(path,password);
    store.select_account(account);
}
}
