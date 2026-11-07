// One ordered, bounded stream per snapshot. UNION ALL keeps every metadata row
// visible; separate tags distinguish absent fields from optional rule marks.
app::EntrySnapshot audit_snapshot(sqlite3* db,const imports::Occurrence& id,std::int64_t revision,const qif::Record& source,const Currency& currency,const app::OperationContext* context){
    constexpr auto base=
        "SELECT 0 AS tag,0 AS position,active AS a,date AS b,payee AS c,memo AS d,total AS e,NULL AS f FROM entry_values WHERE batch=?1 AND ordinal=?2 AND revision=?3 "
        "UNION ALL SELECT 1,0,payee_owner,memo_owner,manual_split_amounts,remainder_allocation,NULL,NULL FROM entry_fields WHERE batch=?1 AND ordinal=?2 AND revision=?3 "
        "UNION ALL SELECT 2,position,category,memo,units,NULL,NULL,NULL FROM entry_allocation_values WHERE batch=?1 AND ordinal=?2 AND revision=?3 "
        "UNION ALL SELECT 3,position,identity,source_ordinal,category_id,catalog_revision,category_owner,memo_owner FROM entry_allocation_meta WHERE batch=?1 AND ordinal=?2 AND revision=?3 ";
    const bool rules=rules_format(db);
    std::string sql=base;
    if(rules)sql+=
        "UNION ALL SELECT 4,0,payee_rule,payee_version,memo_rule,memo_version,NULL,NULL FROM entry_rule_fields WHERE batch=?1 AND ordinal=?2 AND revision=?3 "
        "UNION ALL SELECT 5,position,category_rule,category_version,memo_rule,memo_version,NULL,NULL FROM entry_rule_allocations WHERE batch=?1 AND ordinal=?2 AND revision=?3 ";
    sql+="ORDER BY tag,position";
    Statement rows(db,sql.c_str());rows.text(1,id.batch);rows.integer(2,integer(id.ordinal));rows.integer(3,revision);
    bool snapshot=false,fields=false,rule_fields=false,active=false,manual=false;
    std::string date,payee,memo,remainder;std::int64_t total=0;
    FieldOwnership payee_owner{},memo_owner{};std::optional<RuleMark> payee_rule,memo_rule;
    std::vector<Allocation> allocations;std::size_t metadata=0;std::int64_t last_rule=-1;
    const auto mark=[&](int name,int version)->std::optional<RuleMark>{
        if(rows.null(name)!=rows.null(version))fail(SQLITE_CORRUPT,"Rule provenance does not match its ownership");
        if(rows.null(name))return {};return RuleMark{rows.bytes(name),rows.integer(version)};
    };
    while(rows.row()){
        if(context)context->checkpoint();
        switch(rows.integer(0)){
        case 0:
            if(snapshot)fail(SQLITE_CORRUPT);snapshot=true;active=rows.integer(2)!=0;
            date=rows.bytes(3);payee=rows.bytes(4);memo=rows.bytes(5);total=rows.integer(6);break;
        case 1:
            if(fields)fail(SQLITE_CORRUPT);fields=true;
            payee_owner=static_cast<FieldOwnership>(rows.integer(2));memo_owner=static_cast<FieldOwnership>(rows.integer(3));
            if(rows.integer(4)!=0 && rows.integer(4)!=1)fail(SQLITE_CORRUPT);manual=rows.integer(4)!=0;
            remainder=rows.bytes(5);if(!remainder.empty())identifier(remainder);break;
        case 2:
            if(rows.integer(1)!=integer(allocations.size()) || allocations.size()>=qif::max_allocations)fail(SQLITE_CORRUPT);
            allocations.push_back({rows.bytes(2),rows.bytes(3),Money(rows.integer(4),currency)});break;
        case 3:{
            if(metadata>=allocations.size() || rows.integer(1)!=integer(metadata))fail(SQLITE_CORRUPT);
            auto& a=allocations[metadata++];a.identity=rows.bytes(2);
            if(!rows.null(3))a.source_ordinal=static_cast<std::size_t>(rows.integer(3));
            if(!rows.null(4))a.category_binding=CategoryBinding{rows.bytes(4),rows.integer(5)};
            a.category_owner=static_cast<FieldOwnership>(rows.integer(6));a.memo_owner=static_cast<FieldOwnership>(rows.integer(7));
            validate_binding(db,a);break;
        }
        case 4:
            if(rule_fields)fail(SQLITE_CORRUPT);rule_fields=true;payee_rule=mark(2,3);memo_rule=mark(4,5);break;
        case 5:{
            const auto position=rows.integer(1);
            if(position<=last_rule || position<0 || static_cast<std::size_t>(position)>=allocations.size())fail(SQLITE_CORRUPT);
            last_rule=position;auto& a=allocations[static_cast<std::size_t>(position)];a.category_rule=mark(2,3);a.memo_rule=mark(4,5);break;
        }
        default:fail(SQLITE_CORRUPT);
        }
    }
    if(!snapshot)fail(SQLITE_NOTFOUND,"Committed revision does not exist");
    if(!fields || metadata!=allocations.size())fail(SQLITE_CORRUPT);
    if(rules){accept_rule_owner(payee_owner,payee_rule,true);accept_rule_owner(memo_owner,memo_rule,true);}
    else {validate_owner(payee_owner);validate_owner(memo_owner);}
    for(const auto& a:allocations){
        if(rules){accept_rule_owner(a.category_owner,a.category_rule,true);accept_rule_owner(a.memo_owner,a.memo_rule,true);}
        else {validate_owner(a.category_owner);validate_owner(a.memo_owner);}
    }
    const bool reassigned=source.source_currency && *source.source_currency!=currency;
    imports::DraftRow draft{date,payee,memo,Transaction::repaired_import(source.original_amount,Money(total,currency),std::move(allocations),reassigned)};
    draft.payee_owner=payee_owner;draft.memo_owner=memo_owner;draft.payee_rule=payee_rule;draft.memo_rule=memo_rule;
    draft.manual_split_amounts=manual;draft.remainder_allocation=std::move(remainder);
    imports::validate_draft(source,draft,reassigned);
    return {revision,active,std::move(draft)};
}
