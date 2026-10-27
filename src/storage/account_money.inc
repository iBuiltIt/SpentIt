// Account-command helpers. Included inside the storage anonymous namespace.
constexpr const char* empty_account_deletion="Empty-account deletion requires an untouched zero opening, the creation opening date, no imports, no transfer participation, and another account remaining in the document.";
constexpr const char* archived_target="Archived accounts are not targets for new imports, commits, transactions, or transfer matches";
constexpr const char* reactivate_first="Reactivate the account before changing its opening or currency";
constexpr const char* later_undo_block="A later financial operation prevents Undo";

std::int64_t smallest_active_account(sqlite3* db) {
    Statement row(db,"SELECT singleton FROM account WHERE NOT EXISTS(SELECT 1 FROM retired_import_accounts r WHERE r.account_id=account.singleton) AND NOT EXISTS(SELECT 1 FROM account_retirements d WHERE d.account_id=account.singleton) AND NOT EXISTS(SELECT 1 FROM account_status s WHERE s.identity=account.singleton AND s.archived=1) ORDER BY singleton LIMIT 1");
    return row.row()?row.integer(0):0;
}
bool account_row_archived(sqlite3* db,std::int64_t account) {
    Statement row(db,"SELECT archived FROM account_status WHERE identity=?");row.integer(1,account);return row.row() && row.integer(0)!=0;
}
bool account_lifecycle_retired(sqlite3* db,std::int64_t account) {
    Statement row(db,"SELECT 1 FROM account_retirements WHERE account_id=?");row.integer(1,account);return row.row();
}
bool account_import_retired(sqlite3* db,std::int64_t account) {
    Statement row(db,"SELECT 1 FROM retired_import_accounts WHERE account_id=?");row.integer(1,account);return row.row();
}
void require_writable_account(sqlite3* db,std::int64_t account) {
    if(account_lifecycle_retired(db,account) || account_import_retired(db,account)) fail(SQLITE_CONSTRAINT,"Account identity was retired");
    if(account_row_archived(db,account)) fail(SQLITE_CONSTRAINT,archived_target);
}
std::string command_id(std::string_view id) {
    identifier(id);
    if(id.size()>120) fail(SQLITE_MISUSE,"Invalid operation identifier");
    return std::string(id);
}
bool blank_name(std::string_view name) {
    if(name.empty()) return true;
    for(char c:name) if(c!=' ' && c!='\t') return false;
    return true;
}
Currency pinned_currency(sqlite3* db,std::string_view batch) {
    Statement row(db,"SELECT definition FROM batch_source_currency WHERE batch=?");row.text(1,batch);
    if(!row.row()) fail(SQLITE_CORRUPT,"Source currency pin is missing");
    return Currency::from_definition(row.bytes(0));
}
void pin_source_currency(sqlite3* db,std::string_view batch,const Currency& currency) {
    Statement row(db,"INSERT INTO batch_source_currency VALUES(?,?)");row.text(1,batch);row.text(2,currency.definition_id());row.done();
}
Currency currency_as_of(sqlite3* db,std::int64_t account,std::int64_t sequence) {
    Statement created(db,"SELECT definition FROM account_creations WHERE identity=?");created.integer(1,account);
    if(!created.row()) fail(SQLITE_CORRUPT);
    auto id=created.bytes(0);
    Statement rows(db,"SELECT o.definition FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? AND h.sequence<=? AND (o.kind=5 OR (o.kind=6 AND EXISTS(SELECT 1 FROM account_operations u WHERE u.id=o.undo_of AND u.kind=5))) ORDER BY h.sequence");
    rows.integer(1,account);rows.integer(2,sequence);
    while(rows.row()) id=rows.bytes(0);
    return Currency::from_definition(id);
}
Currency version_currency(sqlite3* db,std::string_view batch,std::int64_t ordinal,std::int64_t revision) {
    Statement mark(db,"SELECT o.definition FROM account_amount_revisions a JOIN account_operations o ON o.id=a.operation WHERE a.kind=0 AND a.batch=? AND a.ordinal=? AND a.revision=? AND a.position=0 LIMIT 1");
    mark.text(1,batch);mark.integer(2,ordinal);mark.integer(3,revision);
    if(mark.row()) return Currency::from_definition(mark.bytes(0));
    Statement seq(db,"SELECT h.sequence,b.account_id FROM history_order h JOIN batches b ON b.id=h.batch WHERE h.batch=? AND h.event_revision=?");
    seq.text(1,batch);seq.integer(2,revision);if(!seq.row()) fail(SQLITE_CORRUPT);
    return currency_as_of(db,seq.integer(1),seq.integer(0));
}
Currency entry_currency(sqlite3* db,std::string_view batch,std::int64_t ordinal,std::int64_t revision) {
    Statement mark(db,"SELECT o.definition FROM account_amount_revisions a JOIN account_operations o ON o.id=a.operation WHERE a.kind=2 AND a.batch=? AND a.ordinal=? AND a.revision=? AND a.position=0 LIMIT 1");
    mark.text(1,batch);mark.integer(2,ordinal);mark.integer(3,revision);
    if(mark.row()) return Currency::from_definition(mark.bytes(0));
    Statement seq(db,"SELECT h.sequence,b.account_id FROM ledger_operations o JOIN history_order h ON h.operation=o.id JOIN batches b ON b.id=o.batch WHERE o.batch=? AND ((?=0 AND o.kind=0) OR (o.ordinal=? AND o.result_revision=? AND o.kind!=0))");
    seq.text(1,batch);seq.integer(2,revision);seq.integer(3,ordinal);seq.integer(4,revision);if(!seq.row()) fail(SQLITE_CORRUPT);
    return currency_as_of(db,seq.integer(1),seq.integer(0));
}
bool explicit_account_money(sqlite3* db,std::int64_t account) {
    Statement row(db,"SELECT 1 FROM account_operations WHERE account=? AND kind IN(4,5,6) LIMIT 1");row.integer(1,account);return row.row();
}
app::AccountCommandReceipt account_receipt(Statement& row) {
    const auto currency=Currency::from_definition(row.bytes(4));
    return {row.bytes(0),static_cast<int>(row.integer(2)),row.integer(1),row.bytes(3),currency,row.bytes(6),Money(row.integer(5),currency),row.integer(7)!=0,row.integer(8)!=0,row.integer(9),row.integer(10)!=0,row.integer(11)!=0};
}
constexpr const char* account_receipt_sql="SELECT id,account,kind,name,definition,opening,opening_date,archived,retired,revision,frozen,confirmed,content FROM account_operations WHERE id=?";
std::optional<app::AccountCommandReceipt> account_command_row(sqlite3* db,std::string_view operation) {
    Statement row(db,account_receipt_sql);row.text(1,operation);if(!row.row()) return {};
    return account_receipt(row);
}
std::optional<app::AccountCommandReceipt> matching_account_command(sqlite3* db,std::string_view operation,std::string_view content) {
    Statement row(db,account_receipt_sql);row.text(1,operation);
    if(row.row()) {
        if(row.bytes(12)!=content) fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
        return account_receipt(row);
    }
    Statement reg(db,"SELECT 1 FROM operation_registry WHERE id=? UNION ALL SELECT 1 FROM ledger_operations WHERE id=?");reg.text(1,operation);reg.text(2,operation);
    if(reg.row()) fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");
    return {};
}
Money opening_at_start(sqlite3* db,std::int64_t account) {
    Statement current(db,"SELECT opening,definition FROM account WHERE singleton=?");current.integer(1,account);if(!current.row()) fail(SQLITE_CORRUPT);
    auto units=current.integer(0);auto currency=Currency::from_definition(current.bytes(1));
    Statement rows(db,"SELECT o.kind,o.prior_opening,o.prior_definition,o.undo_of FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? AND o.kind IN(4,5,6) ORDER BY h.sequence DESC");
    rows.integer(1,account);
    while(rows.row()) {
        auto target=static_cast<int>(rows.integer(0));
        if(target==6) {Statement undone(db,"SELECT kind FROM account_operations WHERE id=?");undone.text(1,rows.bytes(3));if(!undone.row()) fail(SQLITE_CORRUPT);target=static_cast<int>(undone.integer(0));}
        if(target==4) units=rows.integer(1);
        else if(target==5) {units=rows.integer(1);currency=Currency::from_definition(rows.bytes(2));}
    }
    return Money(units,currency);
}
struct OpeningDelta {std::int64_t sequence; Money prior; Money next;};
std::vector<OpeningDelta> opening_deltas(sqlite3* db,std::int64_t account) {
    std::vector<OpeningDelta> out;
    Statement rows(db,"SELECT o.opening,o.prior_opening,h.sequence FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? AND (o.kind=4 OR (o.kind=6 AND EXISTS(SELECT 1 FROM account_operations u WHERE u.id=o.undo_of AND u.kind=4))) ORDER BY h.sequence");
    rows.integer(1,account);
    while(rows.row()) {const auto sequence=rows.integer(2);const auto currency=currency_as_of(db,account,sequence);out.push_back({sequence,Money(rows.integer(1),currency),Money(rows.integer(0),currency)});}
    return out;
}
bool side_mentions(const transfers::Side& side,std::int64_t account) {
    return side.endpoint.account==account || (side.attached_evidence && side.attached_evidence->endpoint.account==account) || (side.proposed_evidence && side.proposed_evidence->endpoint.account==account);
}
bool link_mentions(const transfers::Link& link,std::int64_t account) {return side_mentions(link.origin,account) || side_mentions(link.destination,account);}
void rescale_side(transfers::Side& side,const Currency& target) {
    side.amount=side.amount.rescale(target);
    if(side.attached_evidence) side.attached_evidence->amount=side.attached_evidence->amount.rescale(target);
    if(side.proposed_evidence) side.proposed_evidence->amount=side.proposed_evidence->amount.rescale(target);
}
void rescale_link_account(transfers::Link& link,std::int64_t account,const Currency& target) {
    if(link.origin.endpoint.account==account) rescale_side(link.origin,target);
    if(link.destination.endpoint.account==account) rescale_side(link.destination,target);
    link.preserved_amounts=link.origin.amount.currency()!=link.destination.amount.currency();
    transfers::validate(link);
}
imports::DraftRow rescale_draft(const imports::DraftRow& row,const Currency& target) {
    const auto total=row.transaction.effective_total().rescale(target);
    auto allocations=row.transaction.allocations();
    for(auto& allocation:allocations) allocation.amount=allocation.amount.rescale(target);
    const auto& original=row.transaction.original_imported_amount();
    bool reassigned=row.transaction.reassigned_currency();
    if((original && original->currency()!=total.currency()) || (!original && row.transaction.effective_total().currency()!=total.currency())) reassigned=true;
    auto copy=row;copy.transaction=Transaction::repaired_import(original,total,std::move(allocations),reassigned);return copy;
}
imports::DraftRow draft_at(sqlite3* db,const imports::Occurrence& id,std::int64_t revision,const qif::Record& source) {
    Statement fields(db,"SELECT date,payee,memo,total FROM versions WHERE batch=? AND ordinal=? AND revision=?");
    fields.text(1,id.batch);fields.integer(2,integer(id.ordinal));fields.integer(3,revision);if(!fields.row()) fail(SQLITE_CORRUPT);
    const auto currency=version_currency(db,id.batch,integer(id.ordinal),revision);
    const bool reassigned=source.source_currency && *source.source_currency!=currency;
    std::vector<Allocation> allocations;Statement values(db,"SELECT category,memo,units FROM allocations WHERE batch=? AND ordinal=? AND revision=? ORDER BY position");
    values.text(1,id.batch);values.integer(2,integer(id.ordinal));values.integer(3,revision);
    while(values.row()) allocations.push_back({values.bytes(0),values.bytes(1),Money(values.integer(2),currency)});
    imports::DraftRow draft{fields.bytes(0),fields.bytes(1),fields.bytes(2),Transaction::repaired_import(source.original_amount,Money(fields.integer(3),currency),std::move(allocations),reassigned)};
    load_metadata(db,id,revision,draft,false);return draft;
}
bool reassignment_authorizes(sqlite3* db,std::int64_t account,const Currency& effective,const Currency& source) {
    if(effective==source) return false;
    Statement current(db,"SELECT definition FROM account WHERE singleton=?");current.integer(1,account);if(!current.row() || current.bytes(0)!=effective.definition_id()) return false;
    Statement latest(db,"SELECT o.definition FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? AND o.kind IN(5,6) ORDER BY h.sequence DESC LIMIT 1");
    latest.integer(1,account);return latest.row() && latest.bytes(0)==effective.definition_id();
}
void record_amount(sqlite3* db,std::string_view operation,int kind,std::string_view batch,std::int64_t ordinal,std::int64_t revision,std::int64_t position,std::int64_t prior_units,std::int64_t units) {
    Statement row(db,"INSERT INTO account_amount_revisions VALUES(?,?,?,?,?,?,?,?)");
    row.text(1,operation);row.integer(2,kind);row.text(3,batch);row.integer(4,ordinal);row.integer(5,revision);row.integer(6,position);row.integer(7,prior_units);row.integer(8,units);row.done();
}
bool transfer_participation(sqlite3* db,std::int64_t account) {
    Statement versions(db,"SELECT payload FROM transfer_versions");
    while(versions.row()) if(link_mentions(transfer_codec::view(versions.bytes(0)).link,account)) return true;
    Statement events(db,"SELECT account,request FROM transfer_events");
    while(events.row()) {
        if(events.integer(0)==account) return true;
        const auto request=transfer_codec::request(events.bytes(1));
        if(request.destination==account || (request.evidence && request.evidence->endpoint.account==account)) return true;
        if(request.origin) {Statement owner(db,"SELECT account_id FROM batches WHERE id=?");owner.text(1,request.origin->batch);if(owner.row() && owner.integer(0)==account) return true;}
        if(!request.link.empty()) {Statement payload(db,"SELECT payload FROM transfer_versions WHERE identity=? LIMIT 1");payload.text(1,request.link);if(payload.row() && link_mentions(transfer_codec::view(payload.bytes(0)).link,account)) return true;}
    }
    return false;
}
bool later_financial(sqlite3* db,std::int64_t account,std::int64_t anchor) {
    Statement ops(db,"SELECT 1 FROM account_operations o JOIN history_order h ON h.account_operation=o.id WHERE o.account=? AND o.kind IN(3,4,5,6) AND h.sequence>? LIMIT 1");
    ops.integer(1,account);ops.integer(2,anchor);if(ops.row()) return true;
    Statement ledger(db,"SELECT 1 FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE h.sequence>? AND (o.batch IN(SELECT id FROM batches WHERE account_id=?) OR (o.kind=5 AND o.ordinal=?) OR EXISTS(SELECT 1 FROM transfer_events t WHERE t.operation=o.id AND t.account=?)) LIMIT 1");
    ledger.integer(1,anchor);ledger.integer(2,account);ledger.integer(3,account);ledger.integer(4,account);if(ledger.row()) return true;
    Statement events(db,"SELECT 1 FROM events e JOIN history_order h ON h.batch=e.batch AND h.event_revision=e.revision JOIN batches b ON b.id=e.batch WHERE b.account_id=? AND h.sequence>? LIMIT 1");
    events.integer(1,account);events.integer(2,anchor);if(events.row()) return true;
    Statement versions(db,"SELECT v.payload FROM transfer_versions v JOIN ledger_operations o ON o.sequence=v.sequence JOIN history_order h ON h.operation=o.id WHERE h.sequence>?");
    versions.integer(1,anchor);while(versions.row()) if(link_mentions(transfer_codec::view(versions.bytes(0)).link,account)) return true;
    Statement links(db,"SELECT e.request FROM transfer_events e JOIN ledger_operations o ON o.id=e.operation JOIN history_order h ON h.operation=o.id WHERE h.sequence>? AND e.account!=?");
    links.integer(1,anchor);links.integer(2,account);
    while(links.row()) {
        const auto request=transfer_codec::request(links.bytes(0));if(request.link.empty()) continue;
        Statement payload(db,"SELECT payload FROM transfer_versions WHERE identity=? ORDER BY sequence DESC LIMIT 1");payload.text(1,request.link);
        if(payload.row() && link_mentions(transfer_codec::view(payload.bytes(0)).link,account)) return true;
    }
    return false;
}
std::int64_t account_anchor(sqlite3* db,std::string_view operation,int kind) {
    if(kind==5) {
        Statement row(db,"SELECT h.sequence FROM ledger_operations o JOIN history_order h ON h.operation=o.id WHERE o.id=? AND o.kind=5");
        row.text(1,std::string(operation)+"#scale");if(!row.row()) fail(SQLITE_CORRUPT);return row.integer(0);
    }
    Statement row(db,"SELECT sequence FROM history_order WHERE account_operation=?");row.text(1,operation);if(!row.row()) fail(SQLITE_CORRUPT);return row.integer(0);
}
bool command_image_current(sqlite3* db,std::string_view operation) {
    Statement op(db,"SELECT account,definition,opening,opening_date FROM account_operations WHERE id=?");op.text(1,operation);if(!op.row()) return false;
    Statement acct(db,"SELECT definition,opening,opening_date FROM account WHERE singleton=?");acct.integer(1,op.integer(0));
    return acct.row() && acct.bytes(0)==op.bytes(1) && acct.integer(1)==op.integer(2) && acct.bytes(2)==op.bytes(3);
}
void require_record_rescale(const qif::Record& record,const Currency& target) {
    if(record.original_amount) (void)record.original_amount->rescale(target);
    if(!record.transaction) return;
    (void)record.transaction->effective_total().rescale(target);
    for(const auto& allocation:record.transaction->allocations()) (void)allocation.amount.rescale(target);
}
template<class F>
auto guard_money(F&& fn) -> decltype(fn()) {
    try {return fn();}
    catch(const FinancialError& error) {fail(SQLITE_CONSTRAINT,error.what());}
    catch(const transfers::TransferError& error) {fail(SQLITE_CONSTRAINT,error.what());}
}
void insert_account_op(sqlite3* db,std::string_view id,std::int64_t account,int kind,std::string_view name,std::string_view prior_name,std::string_view definition,std::string_view prior_definition,std::int64_t opening,std::int64_t prior_opening,std::string_view date,std::string_view prior_date,int archived,int prior_archived,int retired,std::string_view undo_of,std::string_view content,std::int64_t revision,int frozen,int confirmed) {
    Statement row(db,"INSERT INTO account_operations VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    row.text(1,id);row.integer(2,account);row.integer(3,kind);row.text(4,name);row.text(5,prior_name);row.text(6,definition);row.text(7,prior_definition);
    row.integer(8,opening);row.integer(9,prior_opening);row.text(10,date);row.text(11,prior_date);row.integer(12,archived);row.integer(13,prior_archived);
    row.integer(14,retired);row.text(15,undo_of);row.text(16,content);row.integer(17,revision);row.integer(18,frozen);row.integer(19,confirmed);row.done();
}
std::string scale_ledger_id(std::string_view id){return std::string(id)+"#scale";}
