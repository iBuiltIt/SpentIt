// Only command request digests survive ordinary history expiry. Snapshot
// interpretation belongs to the retained payload, never to a parallel archive.
namespace session_snapshot {
std::string digest(std::string_view bytes) {
    unsigned char result[EVP_MAX_MD_SIZE];unsigned length=0;
    if(EVP_Digest(bytes.data(),bytes.size(),result,&length,EVP_sha256(),nullptr)!=1)fail(SQLITE_ERROR,"Cannot record command completion");
    return {reinterpret_cast<const char*>(result),length};
}
void mark(transfer_codec::Writer& out,const std::optional<RuleMark>& value){out.number(value.has_value());if(value){out.text(value->identity);out.number(value->version);}}
void draft(transfer_codec::Writer& out,const imports::DraftRow& d) {
    out.text(d.date);out.text(d.payee);out.text(d.memo);out.money(d.transaction.effective_total());
    out.number(static_cast<int>(d.payee_owner));out.number(static_cast<int>(d.memo_owner));mark(out,d.payee_rule);mark(out,d.memo_rule);
    out.number(d.manual_split_amounts);out.text(d.remainder_allocation);out.number(integer(d.transaction.allocations().size()));
    for(const auto& a:d.transaction.allocations()){
        out.text(a.category);out.text(a.memo);out.money(a.amount);out.text(a.identity);
        out.number(a.source_ordinal.has_value());if(a.source_ordinal)out.number(integer(*a.source_ordinal));
        out.number(a.category_binding.has_value());if(a.category_binding){out.text(a.category_binding->identity);out.number(a.category_binding->catalog_revision);}
        out.number(static_cast<int>(a.category_owner));out.number(static_cast<int>(a.memo_owner));mark(out,a.category_rule);mark(out,a.memo_rule);
    }
}
std::string image(const imports::DraftRow& d){transfer_codec::Writer out;draft(out,d);return digest(out.bytes);}
void token(transfer_codec::Writer& out,const app::EntryToken& t){out.text(t.identity.batch);out.number(integer(t.identity.ordinal));out.number(t.revision);out.number(integer(t.dependencies.size()));for(const auto& d:t.dependencies){out.text(d.batch);out.number(d.revision);out.number(static_cast<int>(d.state));}}
void request_draft(transfer_codec::Writer& out,const imports::DraftRow& d){draft(out,d);out.number(d.transaction.original_imported_amount().has_value());if(d.transaction.original_imported_amount())out.money(*d.transaction.original_imported_amount());}
std::string change(const app::ChangeRequest& r,const app::EntryToken& expected,std::int64_t preview,std::int64_t catalog){transfer_codec::Writer out;out.number(static_cast<int>(r.kind));out.text(r.identity.batch);out.number(integer(r.identity.ordinal));out.text(r.undo_of);out.number(r.correction.has_value());if(r.correction)request_draft(out,*r.correction);token(out,expected);out.number(preview);out.number(catalog);return digest(out.bytes);}
void retry(sqlite3* db,std::string_view id,std::string_view domain,std::string_view hash){
    Statement row(db,"SELECT domain,digest,expired FROM command_completions WHERE id=?");row.text(1,id);
    if(!row.row())return;
    if(row.bytes(0)!=domain || row.bytes(1)!=hash)fail(SQLITE_CONSTRAINT,"Operation identity was reused with different content");
    if(row.integer(2))throw app::OperationAlreadyCompleted(std::string(id));
}
void expired(sqlite3* db,std::string_view id,std::string_view domain){
    Statement row(db,"SELECT domain FROM command_completions WHERE id=? AND expired=1");row.text(1,id);
    if(row.row()){if(row.bytes(0)!=domain)fail(SQLITE_CONSTRAINT,"Operation identity belongs to another command");throw app::OperationAlreadyCompleted(std::string(id));}
}
void completed(sqlite3* db,std::string_view id,std::string_view domain,std::string_view hash){Statement row(db,"INSERT INTO command_completions VALUES(?,?,?,0)");row.text(1,id);row.text(2,domain);row.blob(3,hash);row.done();}
bool baseline(sqlite3* db,bool entry,const imports::Occurrence& id,std::int64_t revision){
    Statement row(db,entry?"SELECT baseline FROM entry_fields WHERE batch=? AND ordinal=? AND revision=?":"SELECT baseline FROM draft_fields WHERE batch=? AND ordinal=? AND revision=?");
    row.text(1,id.batch);row.integer(2,integer(id.ordinal));row.integer(3,revision);
    if(!row.row())fail(SQLITE_CORRUPT,"Snapshot metadata is missing");return row.integer(0)!=0;
}
}
