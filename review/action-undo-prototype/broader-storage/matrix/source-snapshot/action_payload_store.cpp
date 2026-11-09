// Development-only isolated compilation of the real repository against the
// experimental payload/membership representation. Production targets never use
// these SQL adapters.
#include "action_payload_schema.hpp"
#define sqlite3_exec spentit::storage::action_payload_probe::exec
#define sqlite3_prepare_v2 spentit::storage::action_payload_probe::prepare
#include "../../src/storage/import_store.cpp"
#undef sqlite3_prepare_v2
#undef sqlite3_exec
