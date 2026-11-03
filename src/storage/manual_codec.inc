// Encrypted creation payload for a manually entered transaction. This is an
// internal journal encoding, never a bank file or original imported amount.
namespace manual_codec {
std::string encode(const imports::DraftRow& row) {
    transfer_codec::Writer out;
    out.text("manual-v1");out.text(row.date);out.text(row.payee);out.text(row.memo);
    out.money(row.transaction.effective_total());out.number(integer(row.transaction.allocations().size()));
    for(const auto& a:row.transaction.allocations()) {
        out.text(a.category);out.text(a.memo);out.money(a.amount);out.text(a.identity);
        out.number(a.category_binding.has_value());
        if(a.category_binding){out.text(a.category_binding->identity);out.number(a.category_binding->catalog_revision);}
    }
    out.number(row.manual_split_amounts);out.text(row.remainder_allocation);
    if(out.bytes.size()>qif::max_record)fail(SQLITE_TOOBIG,"Transaction is too large");
    return out.bytes;
}
imports::DraftRow decode(std::string_view bytes,const Currency& currency) {
    if(bytes.size()>qif::max_record)fail(SQLITE_CORRUPT);
    transfer_codec::Reader in{bytes};if(in.text()!="manual-v1")fail(SQLITE_CORRUPT);
    auto date=in.text(),payee=in.text(),memo=in.text();auto total=in.money();total.require_compatible(Money(0,currency));
    const auto count=in.number();if(count<1 || count>static_cast<std::int64_t>(qif::max_allocations))fail(SQLITE_CORRUPT);
    std::vector<Allocation> allocations;
    for(std::int64_t i=0;i<count;++i){auto category=in.text(),note=in.text();auto amount=in.money();auto id=in.text();
        Allocation a{category,note,amount,{},id};a.category_owner=a.memo_owner=FieldOwnership::manual;
        if(in.boolean()){auto category_id=in.text();a.category_binding=CategoryBinding{category_id,in.number()};}
        allocations.push_back(std::move(a));
    }
    imports::DraftRow row{date,payee,memo,Transaction::manual(total,std::move(allocations)),FieldOwnership::manual,FieldOwnership::manual};
    row.manual_split_amounts=in.boolean();row.remainder_allocation=in.text();in.finish();return row;
}
qif::Result parse(std::string_view bytes,const Currency& currency) {
    const auto row=decode(bytes,currency);qif::Record record{};
    record.begin=0;record.end=bytes.size();record.date=row.date;record.payee=row.payee;record.memo=row.memo;
    record.transaction=row.transaction;record.source_currency=currency;record.format="manual";record.grammar="manual";
    imports::validate_draft(record,row);
    qif::Result result;result.records.push_back(std::move(record));return result;
}
}
