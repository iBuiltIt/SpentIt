namespace {
// Acceptance owns lifecycle transitions, not a second copy of transaction data.
// Its existing members retain publication/draft references; this recipe stores
// only changed account, batch and group metadata.
struct AcceptanceBaseline {
    struct Account {std::int64_t id;bool frozen;};
    struct Batch {std::string id;std::int64_t state,revision;};
    struct Group {std::string id;std::int64_t state;std::string publication;};
    std::vector<Account> accounts;
    std::vector<Batch> batches;
    std::vector<Group> groups;
    std::vector<app::TransferView> links;
};
AcceptanceBaseline acceptance_baseline(sqlite3* db,const std::vector<app::TransferView>& links) {
    AcceptanceBaseline result;result.links=links;
    Statement accounts(db,"SELECT singleton,frozen FROM account");while(accounts.row())result.accounts.push_back({accounts.integer(0),accounts.integer(1)!=0});
    Statement batches(db,"SELECT id,state,revision FROM batches WHERE state IN(0,4)");while(batches.row())result.batches.push_back({batches.bytes(0),batches.integer(1),batches.integer(2)});
    Statement groups(db,"SELECT id,state,coalesce(publication,'') FROM import_groups WHERE state=1");while(groups.row())result.groups.push_back({groups.bytes(0),groups.integer(1),groups.bytes(2)});
    return result;
}
std::string acceptance_recipe(sqlite3* db,const AcceptanceBaseline& before) {
    transfer_codec::Writer out;out.number(1);
    std::vector<AcceptanceBaseline::Account> accounts;
    for(const auto& old:before.accounts){Statement row(db,"SELECT frozen FROM account WHERE singleton=?");row.integer(1,old.id);if(!row.row())fail(SQLITE_CORRUPT);if((row.integer(0)!=0)!=old.frozen)accounts.push_back(old);}
    out.number(integer(accounts.size()));for(const auto& old:accounts){out.number(old.id);out.number(old.frozen);}
    std::vector<AcceptanceBaseline::Batch> batches;
    for(const auto& old:before.batches){Statement row(db,"SELECT state,revision FROM batches WHERE id=?");row.text(1,old.id);if(!row.row())fail(SQLITE_CORRUPT);if(row.integer(0)!=old.state || row.integer(1)!=old.revision)batches.push_back(old);}
    out.number(integer(batches.size()));for(const auto& old:batches){out.text(old.id);out.number(old.state);out.number(old.revision);}
    std::vector<AcceptanceBaseline::Group> groups;
    for(const auto& old:before.groups){Statement row(db,"SELECT state,coalesce(publication,'') FROM import_groups WHERE id=?");row.text(1,old.id);if(!row.row())fail(SQLITE_CORRUPT);if(row.integer(0)!=old.state || row.bytes(1)!=old.publication)groups.push_back(old);}
    out.number(integer(groups.size()));for(const auto& old:groups){out.text(old.id);out.number(old.state);out.text(old.publication);}
    return out.bytes;
}
}
