#include <spentit/import_store.hpp>
#include <spentit/diagnostics.hpp>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include "fault_vfs.hpp"
#include <sys/wait.h>
#include <fstream>
#include <chrono>
using namespace spentit;
using namespace spentit::app;
using spentit::storage::ImportStore;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::string source(std::string payee,std::string amount="-10.00",std::string memo="") {return "!Type:Bank\nD01/01/2026\nT"+amount+"\nP"+payee+"\nM"+memo+"\n^\n";}
void answer(ImportStore& store,std::string op,ReviewAction action,std::vector<imports::Occurrence> selected={},std::optional<imports::Occurrence> keep={}){
    const auto sheet=store.review_sheet();const auto kind=action==ReviewAction::merge || action==ReviewAction::separate?ReviewKind::same:ReviewKind::link;
    const auto it=std::find_if(sheet.questions.begin(),sheet.questions.end(),[&](const auto& q){return q.kind==kind;});check(it!=sheet.questions.end(),"Missing question");
    ReviewAnswer a;a.generation=sheet.generation;a.question=it->key;a.action=action;a.selected=std::move(selected);a.survivor=keep;
    store.answer_review(op,a);
}
std::vector<ReviewAnswer> batch_answers(ImportStore& store) {
    const auto sheet=store.review_sheet();std::vector<ReviewAnswer> answers;
    for(const auto& q:sheet.questions)if(q.kind==ReviewKind::same){ReviewAnswer a;a.generation=sheet.generation;a.question=q.key;a.action=ReviewAction::merge;for(const auto& row:q.rows)a.selected.push_back(row.identity);a.survivor=q.rows.front().identity;answers.push_back(std::move(a));}
    return answers;
}
void batch_tests(const std::filesystem::path& dir) {
    const Currency cad("CAD");const auto path=dir/"batch";
    {
        auto s=ImportStore::create(path,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("a",source("A")+source("A","-20.00").substr(11),qif::DateOrder::dmy);s.accept_all("accepted");
        s.ingest("b",source("B")+source("B","-20.00").substr(11),qif::DateOrder::dmy);
        auto answers=batch_answers(s);check(answers.size()==2,"Two independent groups");
        for(int mode=0;mode<6;++mode){auto bad=answers;OperationContext c;
            if(mode==0)bad[1].generation--;
            if(mode==1)bad[1].selected={{"absent",0},{"absent",1}};
            if(mode==2)bad.push_back(bad[0]);
            if(mode==3)bad[1].action=ReviewAction::link;
            if(mode==4)c.request();
            if(mode==5){int count=0;c.progress=[&,count](std::string_view phase) mutable {if(phase=="Merging selected matches" && ++count==2)c.request();};}
            bool refused=false;try{s.merge_review_batch("bad-"+std::to_string(mode),bad,c);}catch(const std::exception&){refused=true;}
            check(refused,"Invalid or cancelled batch refused");check(s.current_ledger().size()==4 && s.balance().units()==-6000,"No partial batch write");check(!s.review_outcome("bad-"+std::to_string(mode)),"No failed batch receipt");s.verify();
        }
        OperationContext cancelled;s.observer([&](auto phase){if(phase==storage::ImportPhase::before_commit)cancelled.request();});bool stopped=false;try{s.merge_review_batch("cancel-boundary",answers,cancelled);}catch(const OperationCancelled&){stopped=true;}s.observer({});check(stopped && !s.review_outcome("cancel-boundary"),"Cancellation at final boundary rolls back");s.verify();
        OperationContext c;s.merge_review_batch("batch",answers,c);s.merge_review_batch("batch",answers,c);check(s.current_ledger().size()==2 && s.balance().units()==-3000,"Atomic batch and retry exact balance");
        auto different=answers;different[0].survivor=imports::Occurrence{"b",0};bool refused=false;try{s.merge_review_batch("batch",different,c);}catch(const std::exception&){refused=true;}check(refused,"Changed retry refused");s.verify();s.backup_to(dir/"batch-backup","test");
    }
    {auto s=ImportStore::open(dir/"batch-backup","test");s.verify();check(s.review_sheet().can_undo,"Batch Undo retained in backup");s.undo_review("restored-undo");check(s.balance().units()==-6000,"Backup Undo restores exact balance");s.verify();OperationContext c;s.merge_review_batch("again",batch_answers(s),c);s.ingest("alternative",source("B")+source("B","-20.00").substr(11),qif::DateOrder::dmy);check(s.current_ledger().size()==2,"Batch retains alternative-source recognition");s.verify();}
    {auto s=ImportStore::open(path,"test");check(s.review_sheet().questions.empty(),"Batch survives reopen");s.undo_review("undo-batch");s.undo_review("undo-batch");check(s.review_sheet().questions.size()==2 && s.balance().units()==-6000,"One Undo restores whole batch");s.verify();
        // Both committed losers and pending rows are restored together.
        s.accept_all("accept-more",true);s.ingest("c",source("C")+source("C","-20.00").substr(11),qif::DateOrder::dmy);
        auto answers=batch_answers(s);OperationContext context;s.merge_review_batch("committed-batch",answers,context);check(s.balance().units()==-3000,"Multiple accepted survivor choices");s.verify();s.undo_review("undo-committed-batch");check(s.balance().units()==-9000,"Accepted and pending rows restored");s.verify();
        s.ingest("barrier",source("Unrelated","-99.00"),qif::DateOrder::dmy);answers=batch_answers(s);OperationContext next;s.merge_review_batch("after-barrier",answers,next);s.undo_review("undo-after-barrier");check(!s.review_sheet().can_undo,"Batch Undo preserves earlier mutation barrier");s.verify();
    }
    {
        auto s=ImportStore::create(dir/"multi-account-batch","test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Savings",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));
        for(int account=1;account<=2;++account){s.select_account(account);s.ingest("a"+std::to_string(account),source("A")+source("A").substr(11),qif::DateOrder::dmy);s.ingest("b"+std::to_string(account),source("B")+source("B").substr(11),qif::DateOrder::dmy);}
        auto answers=batch_answers(s);check(answers.size()==2,"Two account groups");for(auto& a:answers)a.selected.resize(2);
        OperationContext c;s.merge_review_batch("partial",answers,c);check(s.account().identity==2,"Account selection preserved");for(int account=1;account<=2;++account){s.select_account(account);check(s.balance().units()==-3000,"Partial group preserves unmatched postings");}check(s.review_sheet().questions.size()==2,"Two remainder pairs");s.verify();s.undo_review("undo-partial");for(int account=1;account<=2;++account){s.select_account(account);check(s.balance().units()==-4000,"Cross-account Undo");}s.verify();
    }
    {
        auto s=ImportStore::create(dir/"protected-batch","test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("a",source("A")+source("A","-20.00").substr(11),qif::DateOrder::dmy);s.ingest("b",source("B")+source("B","-20.00").substr(11),qif::DateOrder::dmy);
        auto edit=*s.row({"b",1}).draft;auto parts=edit.transaction.allocations();parts[0].amount=Money(-2100,cad);edit.transaction=edit.transaction.corrected(parts[0].amount,parts);s.save({"b",1},0,edit);
        bool refused=false;try{OperationContext c;s.merge_review_batch("different",batch_answers(s),c);}catch(const storage::ImportError&){refused=true;}check(refused && s.current_ledger().size()==4 && s.balance().units()==-6100,"Different effective totals roll back all groups");s.verify();
        s.create_account(2,"Savings",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        parts[0].amount=Money(-2000,cad);edit.transaction=edit.transaction.corrected(parts[0].amount,parts);s.save({"b",1},1,edit);
        s.decide({"b",1},2,{imports::Disposition::include,{}});
        TransferRequest transfer{};transfer.action=TransferAction::from_draft;transfer.link="linked";transfer.origin=imports::Occurrence{"b",1};transfer.batch_revision=3;transfer.allocation="source-0";transfer.destination=2;transfer.expected_sequence=s.transfer_sequence();s.apply_transfer("generate",transfer);s.ingest("c",source("C","-20.00"),qif::DateOrder::dmy);
        refused=false;try{OperationContext c;s.merge_review_batch("linked-loser",batch_answers(s),c);}catch(const storage::ImportError&){refused=true;}check(refused && !s.review_outcome("linked-loser") && s.balance().units()==-8000,"Linked loser refuses whole batch");s.verify();
    }
    {
        const auto path=dir/"clean-remainder";
        {auto s=ImportStore::create(path,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("a",source("Original")+source("Original").substr(11),qif::DateOrder::dmy);s.ingest("b",source("New")+source("New").substr(11),qif::DateOrder::dmy);
        auto answers=batch_answers(s);answers.at(0).selected={{"b",0},{"b",1}};answers.at(0).survivor=imports::Occurrence{"b",0};OperationContext c;s.merge_review_batch("partial-new",answers,c);
        auto sheet=s.review_sheet();check(sheet.questions.size()==1 && sheet.questions.front().kind==ReviewKind::same && sheet.questions.front().rows.size()==2,"Clean original remainder remains unanswered");s.verify();s.backup_to(dir/"remainder-backup","test");}
        {auto s=ImportStore::open(path,"test");check(s.review_sheet().questions.size()==1,"Remainder survives reopening");answer(s,"separate-remainder",ReviewAction::separate);check(s.review_sheet().questions.empty(),"Later answer retires pending remainder");s.undo_review("undo-separate-remainder");check(s.review_sheet().questions.size()==1,"Undo restores unanswered remainder");s.undo_review("undo-partial-new");check(s.review_sheet().questions.at(0).rows.size()==4 && s.balance().units()==-4000,"Undo restores original group");s.verify();}
        {auto s=ImportStore::open(dir/"remainder-backup","test");check(s.review_sheet().questions.size()==1,"Backup retains unanswered remainder");s.verify();}
    }
    std::cout<<"Batch Review tests passed\n";
}
void fault_tests(const std::filesystem::path& dir,int first_mode=0) {
    const Currency cad("CAD");fault_vfs::install();
    for(int mode=first_mode;mode<8;++mode){
        const auto base=dir/("base-"+std::to_string(mode));
        ReviewAnswer request;std::vector<ReviewAnswer> batch_request;
        {
            auto store=ImportStore::create(base,"test","Checking",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            if(mode<2 || mode>=4){store.ingest("a",source("SYNTHETIC-REVIEW-MARKER"),qif::DateOrder::dmy);store.accept_all("accept");store.ingest("b",source("Other"),qif::DateOrder::dmy);}
            else{store.create_account(2,"Wise",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));store.ingest("in",source("SYNTHETIC-REVIEW-MARKER","10.00"),qif::DateOrder::dmy);store.select_account(1);store.ingest("out",std::string("!Type:Bank\nD01/01/2026\nT-10.40\nPTransfer\nL[Wise]\n^\n"),qif::DateOrder::dmy);}
            if(mode>=4){store.accept_all("accept-b",true);store.ingest("c",source("Third"),qif::DateOrder::dmy);}
            if(mode>=6){store.ingest("extra-a",source("SYNTHETIC-REVIEW-MARKER","-20.00"),qif::DateOrder::dmy);store.ingest("extra-b",source("Other","-20.00"),qif::DateOrder::dmy);batch_request=batch_answers(store);}
            const auto sheet=store.review_sheet();request.generation=sheet.generation;request.question=sheet.questions.at(0).key;request.action=(mode<2 || mode>=4)?ReviewAction::merge:ReviewAction::link;request.selected=(mode<2 || mode>=4)?std::vector<imports::Occurrence>{{"a",0},{"b",0}}:std::vector<imports::Occurrence>{{"out",0},{"in",0}};request.split_difference=true;if(mode>=4){request.selected.push_back({"c",0});request.survivor=imports::Occurrence{"a",0};}
            if(mode%2){if(mode>=6){OperationContext context;store.merge_review_batch("answer",batch_request,context);}else store.answer_review("answer",request);}
        }
        const auto apply=[&](ImportStore& s){if(mode%2)s.undo_review("operation");else if(mode>=6){OperationContext context;s.merge_review_batch("operation",batch_request,context);}else s.answer_review("operation",request);};
        int positions=0;
        for(int nth=1;nth<2000;++nth){
            const auto trial=dir/"trial";std::filesystem::copy_file(base,trial,std::filesystem::copy_options::overwrite_existing);bool fired;
            {auto s=ImportStore::open(trial,"test");fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(s);}catch(const std::exception&){}fired=fault_vfs::fired;fault_vfs::disarm();}
            {auto s=ImportStore::open(trial,"test");s.verify();if(mode>=6){const bool complete=s.review_outcome("operation").has_value();const bool merged=mode==6?complete:!complete;check(s.current_ledger().size()==(merged?2:5) && s.balance().units()==(merged?-3000:-7000),"Fault leaves all or none of batch");}if(!s.review_outcome("operation"))apply(s);check(s.review_outcome("operation").has_value(),"Retry completed");if(mode>=6)check(s.current_ledger().size()==(mode==6?2:5) && s.balance().units()==(mode==6?-3000:-7000),"Retry exact batch result");s.verify();s.backup_to(dir/"restored","test");}
            {auto s=ImportStore::open(dir/"restored","test");s.verify();if(mode>=6)check(s.current_ledger().size()==(mode==6?2:5) && s.balance().units()==(mode==6?-3000:-7000),"Restored exact batch result");}
            std::filesystem::remove(dir/"restored");
            std::ifstream bytes(trial,std::ios::binary);const std::string raw{std::istreambuf_iterator<char>(bytes),{}};check(raw.find("SYNTHETIC-REVIEW-MARKER")==std::string::npos,"Encrypted Review storage");
            if(!fired){positions=nth-1;break;}
        }
        check(positions>0,"Fault sweep reached writes");std::cout<<"Review fault mode "<<mode<<": "<<positions<<" positions\n";
        for(const auto phase:{spentit::storage::ImportPhase::before_commit,spentit::storage::ImportPhase::after_commit}){
            const auto trial=dir/"crash";std::filesystem::copy_file(base,trial,std::filesystem::copy_options::overwrite_existing);
            const auto child=fork();check(child>=0,"fork");if(child==0){try{auto s=ImportStore::open(trial,"test");s.observer([&](auto p){if(p==phase)_exit(71);});apply(s);_exit(72);}catch(...){_exit(73);}}
            int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==71,"Crash boundary reached");
            auto s=ImportStore::open(trial,"test");check(s.review_outcome("operation").has_value()==(phase==spentit::storage::ImportPhase::after_commit),"Durable boundary");if(mode>=6){const bool merged=mode==6?(phase==spentit::storage::ImportPhase::after_commit):(phase==spentit::storage::ImportPhase::before_commit);check(s.balance().units()==(merged?-3000:-7000),"Interrupted batch exact balance");}if(!s.review_outcome("operation"))apply(s);s.verify();
        }
    }
}
void policy_tests(const std::filesystem::path& dir) {
    const Currency cad("CAD"),usd("USD");
    check(imports::posting_distance("2026-12-31","2027-01-07")==7,"Calendar window crosses years");
    check(imports::posting_distance("2024-02-28","2024-03-01")==2,"Leap day counted");
    check(imports::link_amount_candidate(Money(-10000,cad),Money(9600,cad),true),"Inclusive four percent");
    check(!imports::link_amount_candidate(Money(-10000,cad),Money(9599,cad),true),"Beyond four percent");
    check(!imports::link_amount_candidate(Money(-9600,cad),Money(10000,cad),true),"Incoming cannot exceed outgoing");
    check(!imports::link_amount_candidate(Money(-10000,cad),Money(9600,cad),false),"Difference needs destination evidence");
    check(!imports::link_amount_candidate(Money(-10000,cad),Money(10000,usd),true),"Currency boundary");
    check(imports::link_amount_candidate(Money(INT64_MIN,cad),Money(INT64_MAX,cad),true),"Signed limit without overflow");
    const auto path=dir/"policy";
    {
        auto s=ImportStore::create(path,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("receipt",source("CRA","10.00"),qif::DateOrder::dmy);s.select_account(1);s.ingest("payment",source("Tax","-10.00"),qif::DateOrder::dmy);
        check(s.review_sheet().questions.size()==1,"Equal nearby amounts suggested");
        LinkExclusion rule{"exclude-cra","payee","cra",0,false,false};
        auto preview=s.preview_link_exclusion(rule);check(preview.comparisons.size()==1,"Preview either endpoint");
        s.save_link_exclusion("save-exclusion",rule,preview.generation);s.save_link_exclusion("save-exclusion",rule,preview.generation);
        check(s.review_sheet().questions.empty(),"Dataset exclusion suppresses incoming endpoint");s.verify();
        rule.account=1;preview=s.preview_link_exclusion(rule);check(preview.comparisons.size()==1,"Account scope restores other-account endpoint");s.save_link_exclusion("edit-exclusion",rule,preview.generation);
        check(s.review_sheet().questions.size()==1,"Account scope is endpoint scope");
        answer(s,"reject",ReviewAction::not_linked);rule.removed=true;preview=s.preview_link_exclusion(rule);s.save_link_exclusion("remove-exclusion",rule,preview.generation);check(s.review_sheet().questions.empty(),"Saved No survives rule removal");s.verify();
        rule.expression="(";rule.regex=true;bool invalid=false;try{s.preview_link_exclusion(rule);}catch(const std::exception&){invalid=true;}check(invalid,"Malformed regex rejected");
    }
    {auto s=ImportStore::open(path,"test");check(s.review_sheet().questions.empty(),"Decisions survive reopen");s.verify();}
    {
        auto s=ImportStore::create(dir/"exclusion-fields","test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));
        s.ingest("receipt",source("CAFÉ","10.00"),qif::DateOrder::dmy);s.select_account(1);
        s.ingest("payment","!Type:Bank\nD01/01/2026\nT-10.00\nPPayment\nSUncategorized\nECRA reference 123\n$-10.00\n^\n",qif::DateOrder::dmy);
        LinkExclusion rule{"fields","payee","café",0,false,false};
        check(s.preview_link_exclusion(rule).comparisons.size()==1,"Unicode caseless whole-field exclusion");
        rule.expression="café ";check(s.preview_link_exclusion(rule).comparisons.empty(),"Exact preserves whitespace");
        rule.expression="caf";check(s.preview_link_exclusion(rule).comparisons.empty(),"Exact does not match substrings");
        rule.field="memo";rule.regex=true;rule.expression="^CRA reference [0-9]+$";
        auto preview=s.preview_link_exclusion(rule);check(preview.comparisons.size()==1,"Regex checks split memo");s.save_link_exclusion("split-memo",rule,preview.generation);
        check(s.review_sheet().questions.empty(),"Split-memo exclusion suppresses link");
        auto edited=*s.row({"payment",0}).draft;auto parts=edited.transaction.allocations();parts[0].memo="Different reference";edited.transaction=edited.transaction.corrected(Money(-1000,cad),parts);s.save({"payment",0},0,edited);
        check(s.review_sheet().questions.size()==1,"Memo edit refreshes excluded candidate");
        bool stale=false;try{s.save_link_exclusion("stale",rule,preview.generation);}catch(const std::exception&){stale=true;}check(stale,"Stale exclusion preview rejected");s.verify();
    }
    const auto comparisons=[](ImportStore& s){const auto sheet=s.review_sheet();return std::count_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==ReviewKind::link && q.rows.size()>1;});};
    struct CategoryCase {std::string outgoing,incoming,payee;bool eligible;bool opening_in=false;};
    const std::vector<CategoryCase> categories{{"Gifts","","Payment",false},{"","Gifts","Payment",false},{"Gifts","Gifts","Payment",false},{"","","Payment",true},{"uncategorized","Uncategorised","Payment",true},{"[Visa]","[Checking]","Payment",true},{"[Third]","","Payment",false},{"","[Third]","Payment",false},{"[Missing]","","Payment",false},{"","","Opening Balance",false},{"","","STARTING BALANCE",false},{"","","Payment",false,true}};
    for(std::size_t n=0;n<categories.size();++n){
        const auto& c=categories[n];auto s=ImportStore::create(dir/("category-"+std::to_string(n)),"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.create_account(3,"Third",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));
        if(n==2)s.apply_category_change("gifts",{CategoryAction::create,"gifts","Gifts",{},CategoryKind::leaf},0);
        s.ingest("in",std::string("!Type:Bank\nD02/01/2026\nT10.00\nP")+(c.opening_in?"Opening Balance":"Receipt")+"\nL"+c.incoming+"\n^\n",qif::DateOrder::dmy);s.select_account(1);
        s.ingest("out","!Type:Bank\nD01/01/2026\nT-10.00\nP"+c.payee+"\nL"+c.outgoing+"\n^\n",qif::DateOrder::dmy);
        if((comparisons(s)>0)!=c.eligible)throw std::runtime_error("Category eligibility case "+std::to_string(n)+" yielded "+std::to_string(comparisons(s))+" comparisons");
        if(n==0){
            auto edit=*s.row({"out",0}).draft;auto parts=edit.transaction.allocations();parts[0].category="";edit.transaction=edit.transaction.corrected(edit.transaction.effective_total(),parts);s.save({"out",0},0,edit);check(comparisons(s)==1,"Clearing category restores cached candidate");
            s.select_account(2);edit=*s.row({"in",0}).draft;parts=edit.transaction.allocations();parts[0].category="Gifts";edit.transaction=edit.transaction.corrected(edit.transaction.effective_total(),parts);s.save({"in",0},0,edit);check(comparisons(s)==0,"Categorising counterpart invalidates cached candidate");
            s.select_account(1);TransferRequest request{};request.action=TransferAction::merge;request.link="manual-category-link";request.kept="manual-category-kept";request.expected_sequence=s.transfer_sequence();request.batch_revision=1;request.right_revision=1;
            request.left=transfers::AllocationFact{{1,{"out",0},"source-0"},Money(-1000,cad),"2026-01-01",transfers::State::draft,true};request.right=transfers::AllocationFact{{2,{"in",0},"source-0"},Money(1000,cad),"2026-01-02",transfers::State::draft,true};s.apply_transfer("manual-category-operation",request);s.verify();
        }
    }
    {
        auto s=ImportStore::create(dir/"category-splits","test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.ingest("in",source("Receipt","10.00"),qif::DateOrder::dmy);s.select_account(1);
        s.ingest("out","!Type:Bank\nD01/01/2026\nT-20.00\nPPayment\nSGifts\n$-10.00\nSUncategorized\n$-10.00\n^\n",qif::DateOrder::dmy);check(comparisons(s)==1,"Uncategorized split remains eligible beside Gifts");
        const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==ReviewKind::link && q.rows.size()>1;});ReviewAnswer answer;answer.generation=sheet.generation;answer.question=q->key;answer.action=ReviewAction::link;answer.selected={{"out",0},{"in",0}};answer.left_allocation="source-0";answer.right_allocation="source-0";bool rejected=false;try{s.answer_review("wrong-allocation",answer);}catch(const std::exception&){rejected=true;}check(rejected,"Review cannot link disqualified split through another eligible allocation");answer.left_allocation="source-1";s.answer_review("eligible-allocation",answer);check(s.balance()==Money(-2000,cad),"Eligible split link preserves total");s.verify();
    }
    const auto waiting=dir/"waiting";
    {
        auto s=ImportStore::create(waiting,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("payment","!Type:Bank\nD01/01/2026\nT-100.00\nPCard payment\nL[Visa]\n^\n",qif::DateOrder::dmy);
        check(s.transfers().size()==1,"Explicit destination creates one generated counterpart");
        auto sheet=s.review_sheet();check(sheet.questions.size()==1 && !sheet.questions.front().waiting_link.empty(),"One waiting question");
        s.select_account(2);check(s.balance()==Money(10000,cad),"Generated amount counted exactly once");s.select_account(1);
        LinkExclusion rule{"hide-payments","payee","Card payment",0,false,false};auto preview=s.preview_link_exclusion(rule);s.save_link_exclusion("exclude",rule,preview.generation);check(s.review_sheet().questions.size()==1,"Exclusion cannot hide waiting transfer");
        sheet=s.review_sheet();ReviewAnswer accept;accept.generation=sheet.generation;accept.question=sheet.questions.front().key;accept.action=ReviewAction::accept_missing;s.answer_review("accept-missing",accept);check(s.review_sheet().questions.empty(),"Accept without bank match dismisses waiting item");s.verify();s.undo_review("undo-accept");check(s.review_sheet().questions.size()==1,"Undo restores waiting question");s.verify();
    }
    {auto s=ImportStore::open(waiting,"test");check(s.review_sheet().questions.size()==1,"Waiting survives reopening");s.verify();}
    for(const bool committed:{false,true}){
        const auto both=dir/(committed?"both-committed":"both-draft");
        {auto s=ImportStore::create(both,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("out","!Type:Bank\nD01/01/2026\nT-100.00\nPPayment\nL[Visa]\n^\n",qif::DateOrder::dmy);if(committed)s.accept_all("accept-out",true);
        s.select_account(2);s.ingest("in","!Type:Bank\nD03/01/2026\nT100.00\nPReceipt\nL[Checking]\n^\n",qif::DateOrder::dmy);if(committed)s.accept_all("accept-in",true);
        check(s.balance()==Money(20000,cad),"Both generated and independent destination visible before repair");
        const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& v){return v.kind==ReviewKind::link && v.rows.size()>1;});check(q!=sheet.questions.end(),"Two generated relationships are reviewable");
        ReviewAnswer link;link.question=q->key;link.generation=sheet.generation;link.action=ReviewAction::link;link.selected={{"out",0},{"in",0}};s.answer_review("join",link);
        s.select_account(1);check(s.balance()==Money(-10000,cad),"Checking counts once after joining");s.select_account(2);check(s.balance()==Money(10000,cad),"Visa counts once after joining");s.verify();
        s.undo_review("undo-join");s.select_account(1);check(s.balance()==Money(-20000,cad),"Undo restores Checking generated amount");s.select_account(2);check(s.balance()==Money(20000,cad),"Undo restores Visa generated amount");s.verify();}
        {auto s=ImportStore::open(both,"test");s.verify();}
    }
    std::cout<<"Review matching policy passed\n";
}

void generation_workload(const std::filesystem::path& dir,int count) {
    const Currency cad("CAD");const auto path=dir/"generation";
    std::string bytes="!Type:Bank\n";
    for(int i=0;i<count;++i)bytes+="D01/01/2026\nT-1.00\nPSynthetic "+std::to_string(i)+"\nS[Visa]\n$-2.00\nS[Visa]\n$1.00\n^\n";
    diagnostics::Collector::instance().enable();
    {
        auto s=ImportStore::create(path,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));
        s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
        s.ingest("accepted",source("Prior balance","25.00"),qif::DateOrder::dmy);s.accept_all("accept-prior",true);
        const auto start=std::chrono::steady_clock::now();s.ingest("payments",bytes,qif::DateOrder::dmy);
        const auto end=std::chrono::steady_clock::now();
        const auto metric=diagnostics::Collector::instance().snapshot().metrics[static_cast<unsigned>(diagnostics::Code::import_generate_transfers)];
        std::cout<<"Generation workload rows="<<count<<" links="<<count*2<<" import_ms="<<std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count()<<" generation_us="<<metric.inclusive<<std::endl;
        check(s.transfers().size()==static_cast<std::size_t>(count*2),"Both signed allocations generate distinct counterparts");
        check(s.committed_balance()==Money(2500,cad),"Generation preserves accepted balance");
        check(s.balance()==Money(2500-count*100,cad),"Source split totals reconcile");
        s.ingest("payments",bytes,qif::DateOrder::dmy);check(s.transfers().size()==static_cast<std::size_t>(count*2),"Retry creates no extra counterparts");
        s.select_account(2);check(s.balance()==Money(count*100,cad),"Signed generated totals reconcile");check(s.committed_balance()==Money(0,cad),"Destination remains unaccepted");s.verify();
    }
    {
        auto s=ImportStore::open(path,"test");s.select_account(1);check(s.transfers().size()==static_cast<std::size_t>(count*2),"All generated history reopens");
        s.accept_all("accept-payments",true);check(s.committed_balance()==Money(2500-count*100,cad),"Source acceptance reconciles");s.select_account(2);check(s.committed_balance()==Money(count*100,cad),"Generated acceptance reconciles");s.verify();
        s.backup_to(dir/"restored","test");
    }
    {auto s=ImportStore::open(dir/"restored","test");s.verify();s.select_account(2);check(s.balance()==Money(count*100,cad),"Restored generated balance reconciles");}
}

void decision_workload(const std::filesystem::path& dir,int count) {
    diagnostics::Collector::instance().enable();
    const Currency cad("CAD");auto s=ImportStore::create(dir/"decisions","test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));
    for(int pass=0;pass<2;++pass){std::string bytes="!Type:Bank\n";for(int i=0;i<count;++i)bytes+="D01/01/2026\nT-"+std::to_string(i+1)+".00\nP"+std::to_string(pass)+" synthetic\n^\n";s.ingest(pass?"b":"a",bytes,qif::DateOrder::dmy);}
    auto sheet=s.review_sheet();check(sheet.questions.size()==static_cast<std::size_t>(count),"Expected duplicate groups");
    auto before=diagnostics::Collector::instance().snapshot();const auto start=std::chrono::steady_clock::now();
    answer(s,"measured",ReviewAction::merge,{{"a",0},{"b",0}},imports::Occurrence{"a",0});
    check(s.review_sheet().questions.size()==static_cast<std::size_t>(count-1),"One question resolved");
    const auto after=diagnostics::Collector::instance().snapshot();
    std::cout<<"Single Review rows="<<count*2<<" elapsed_us="<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()<<"\n";
    for(unsigned i=0;i<static_cast<unsigned>(diagnostics::Code::count);++i){const auto& a=after.metrics[i];const auto& b=before.metrics[i];if(a.calls>b.calls)std::cout<<diagnostics::name(static_cast<diagnostics::Code>(i))<<" calls="<<a.calls-b.calls<<" inclusive_us="<<a.inclusive-b.inclusive<<" exclusive_us="<<a.exclusive-b.exclusive<<"\n";}
    check(s.current_ledger().size()==static_cast<std::size_t>(count*2-1),"One posting removed");s.verify();s.undo_review("undo");check(s.current_ledger().size()==static_cast<std::size_t>(count*2),"Undo restores postings");s.verify();
    for(const bool batch:{false,true}){
        const auto path=dir/(batch?"timed-batch":"timed-single");s.backup_to(path,"test");auto trial=ImportStore::open(path,"test");auto answers=batch_answers(trial);answers.resize(std::min<std::size_t>(34,answers.size()));
        auto expected=trial.balance();const auto questions=trial.review_sheet();for(const auto& a:answers){const auto q=std::find_if(questions.questions.begin(),questions.questions.end(),[&](const auto& q){return q.key==a.question;});expected=expected-q->rows.front().draft.transaction.effective_total();}
        const auto start=std::chrono::steady_clock::now();
        if(batch){OperationContext c;trial.merge_review_batch("timed",answers,c);trial.review_sheet();}
        else for(std::size_t i=0;i<answers.size();++i){answers[i].generation=trial.review_sheet().generation;trial.answer_review("timed-"+std::to_string(i),answers[i]);trial.review_sheet();}
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        check(trial.balance()==expected,"Timed decisions reconcile exact balance");check(trial.current_ledger().size()==static_cast<std::size_t>(count*2)-answers.size(),"Timed decisions remove only selected postings");trial.verify();
        std::cout<<(batch?"Batch":"One-by-one")<<" groups="<<answers.size()<<" elapsed_us="<<elapsed<<std::endl;
    }

}
void review_benchmark(const std::filesystem::path& dir) {
    const Currency cad("CAD");const auto path=dir/"benchmark";
    const auto start=std::chrono::steady_clock::now();
    auto s=ImportStore::create(path,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Savings",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));
    constexpr int count=10000;
    for(int account=1;account<=2;++account){
        s.select_account(account);std::string bytes="!Type:Bank\n";
        for(int i=0;i<count;++i){const std::chrono::year_month_day date(std::chrono::sys_days(std::chrono::year(1990)/1/1)+std::chrono::days(i));
            const auto pad=[](unsigned n){return (n<10?"0":"")+std::to_string(n);};
            bytes+="D"+std::to_string(int(date.year()))+"-"+pad(unsigned(date.month()))+"-"+pad(unsigned(date.day()))+"\nT"+(account==1?"-":"")+std::to_string(i+1)+".00\nPTransfer "+std::to_string(i)+"\n^\n";
        }
        s.ingest(account==1?"out":"in",bytes,qif::DateOrder::dmy);
    }
    const auto imported=std::chrono::steady_clock::now();auto sheet=s.review_sheet();const auto built=std::chrono::steady_clock::now();check(sheet.questions.size()==count,"Every unique exact pair is suggested once");
    for(int i=0;i<5;++i)check(s.review_sheet().questions.size()==count,"Read-only opens preserve count");const auto opened=std::chrono::steady_clock::now();
    s.select_account(1);auto edit=*s.row({"out",5000}).draft;auto aa=edit.transaction.allocations();aa[0].amount=Money(-123456789,cad);edit.transaction=edit.transaction.corrected(aa[0].amount,aa);s.save({"out",5000},0,edit);
    check(s.review_sheet().questions.size()==count-1,"Amount edit removes only its pair");const auto changed=std::chrono::steady_clock::now();
    edit.date="1980-01-01";aa=edit.transaction.allocations();aa[0].amount=Money(-500100,cad);edit.transaction=edit.transaction.corrected(aa[0].amount,aa);s.save({"out",5000},1,edit);check(s.review_sheet().questions.size()==count-1,"Equal amount years apart is not suggested");
    edit.date=s.row({"out",5000}).source.date;s.save({"out",5000},2,edit);check(s.review_sheet().questions.size()==count,"Date edit restores new neighborhood");
    const auto ms=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::milliseconds>(b-a).count();};
    std::cout<<"Review workload: "<<count*2<<" rows, "<<count<<" expected pairs, zero unexpected pairs; import "<<ms(start,imported)<<" ms, first Review "<<ms(imported,built)<<" ms, five cached opens "<<ms(built,opened)<<" ms, save and affected Review "<<ms(opened,changed)<<" ms\n";
}

void policy_faults(const std::filesystem::path& dir) {
    const Currency cad("CAD");fault_vfs::install();
    const std::string payment="!Type:Bank\nD01/01/2026\nT-100.00\nPSYNTHETIC-LINK-LEARNING-MARKER\nL[Visa]\n^\n";
    for(int mode=0;mode<6;++mode){
        const auto base=dir/("policy-base-"+std::to_string(mode));ReviewAnswer request;std::int64_t generation=0;
        const LinkExclusion exclusion{"exclude-marker","payee","SYNTHETIC-LINK-LEARNING-MARKER",0,false,false};
        {
            auto s=ImportStore::create(base,"test","Checking",cad,"2026-01-01");s.opening(0,"2026-01-01",Money(0,cad));s.create_account(2,"Visa",cad,"2026-01-01");s.select_account(2);s.opening(0,"2026-01-01",Money(0,cad));s.select_account(1);
            if(mode==0){s.ingest("out",source("SYNTHETIC-LINK-LEARNING-MARKER"),qif::DateOrder::dmy);s.select_account(2);s.ingest("in",source("Receipt","10.00"),qif::DateOrder::dmy);generation=s.preview_link_exclusion(exclusion).generation;}
            if(mode>=2){s.ingest("out",payment,qif::DateOrder::dmy);
                if(mode>=4){s.accept_all("commit-out",true);s.select_account(2);s.ingest("in","!Type:Bank\nD03/01/2026\nT100.00\nPReceipt\nL[Checking]\n^\n",qif::DateOrder::dmy);s.accept_all("commit-in",true);}
                const auto sheet=s.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[&](const auto& item){return mode<4?!item.waiting_link.empty():item.kind==ReviewKind::link && item.rows.size()>1;});check(q!=sheet.questions.end(),"Recovery baseline question");
                request.generation=sheet.generation;request.question=q->key;request.action=mode<4?ReviewAction::accept_missing:ReviewAction::link;if(mode>=4)request.selected={{"out",0},{"in",0}};
                if(mode==3 || mode==5)s.answer_review("baseline-answer",request);
            }
            s.verify();
        }
        const auto apply=[&](ImportStore& s){
            if(mode==0)s.save_link_exclusion("operation",exclusion,generation);
            else if(mode==1){s.select_account(1);s.ingest("operation",payment+"D02/01/2026\nT-1.00\nPSYNTHETIC-LINK-LEARNING-MARKER\nS[Visa]\n$-2.00\nS[Visa]\n$1.00\n^\n",qif::DateOrder::dmy);check(s.transfers().size()==3,"Recovery preserves all generated allocations");}
            else if(mode==3 || mode==5)s.undo_review("operation");
            else s.answer_review("operation",request);
        };
        const auto outcome=[&](ImportStore& s){if(mode!=1)return s.review_outcome("operation").has_value();s.select_account(1);const auto batches=s.batches();return std::any_of(batches.begin(),batches.end(),[](const auto& b){return b.id=="operation";});};
        int positions=0;
        for(int nth=1;nth<2500;++nth){
            const auto trial=dir/"policy-trial";std::filesystem::copy_file(base,trial,std::filesystem::copy_options::overwrite_existing);bool fired;
            {auto s=ImportStore::open(trial,"test");fault_vfs::arm(nth,nth%2?SQLITE_FULL:SQLITE_IOERR_WRITE);try{apply(s);}catch(const std::exception&){}fired=fault_vfs::fired;fault_vfs::disarm();}
            {auto s=ImportStore::open(trial,"test");s.verify();apply(s);check(outcome(s),"Recovery retry completed");s.verify();s.backup_to(dir/"policy-restored","test");}
            {auto s=ImportStore::open(dir/"policy-restored","test");s.verify();}
            std::filesystem::remove(dir/"policy-restored");
            std::ifstream file(trial,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(file),{}};check(bytes.find("SYNTHETIC-LINK-LEARNING-MARKER")==std::string::npos,"Encrypted exclusion and transfer storage");
            if(!fired){positions=nth-1;break;}
        }
        check(positions>0,"Recovery sweep reached writes");std::cout<<"Review policy fault mode "<<mode<<": "<<positions<<" positions\n"<<std::flush;
        for(const auto phase:{storage::ImportPhase::before_commit,storage::ImportPhase::after_commit}){
            const auto trial=dir/"policy-crash";std::filesystem::copy_file(base,trial,std::filesystem::copy_options::overwrite_existing);
            const auto child=fork();check(child>=0,"fork");if(child==0){try{auto s=ImportStore::open(trial,"test");s.observer([&](auto point){if(point==phase)_exit(75);});apply(s);_exit(2);}catch(...){_exit(3);}}
            int status=0;waitpid(child,&status,0);check(WIFEXITED(status)&&WEXITSTATUS(status)==75,"Interrupted at durable boundary");
            auto s=ImportStore::open(trial,"test");s.verify();check(outcome(s)==(phase==storage::ImportPhase::after_commit),"Receipt agrees with durable boundary");apply(s);s.verify();
        }
    }
}

int main(int argc,char** argv){
    char folder[]="/tmp/spentit-review-sheet-XXXXXX";const auto dir=mkdtemp(folder);if(!dir)return 2;const auto path=std::filesystem::canonical(dir)/"data";
    try{
        if(argc>1 && std::string(argv[1])=="batch"){batch_tests(std::filesystem::canonical(dir));std::filesystem::remove_all(dir);return 0;}
        if(argc>1 && std::string(argv[1])=="decisions"){decision_workload(std::filesystem::canonical(dir),argc>2?std::stoi(argv[2]):1000);std::filesystem::remove_all(dir);return 0;}
        if(argc>1 && std::string(argv[1])=="generation"){generation_workload(std::filesystem::canonical(dir),argc>2?std::stoi(argv[2]):120);std::filesystem::remove_all(dir);return 0;}
        if(argc>1 && std::string(argv[1])=="policy_faults"){policy_faults(std::filesystem::canonical(dir));std::filesystem::remove_all(dir);return 0;}
        if(argc>1 && std::string(argv[1])=="benchmark"){review_benchmark(std::filesystem::canonical(dir));std::filesystem::remove_all(dir);return 0;}
        if(argc>1 && std::string(argv[1])=="policy"){policy_tests(std::filesystem::canonical(dir));std::filesystem::remove_all(dir);return 0;}
        if(argc>1){fault_tests(std::filesystem::canonical(dir),argc>2?std::stoi(argv[2]):0);std::filesystem::remove_all(dir);return 0;}
        const auto cad=Currency("CAD");
        {
            auto store=ImportStore::create(path,"test","Visa",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            store.ingest("a",source("One"),qif::DateOrder::dmy);store.accept_all("accept-a");
            store.ingest("b",source("Two"),qif::DateOrder::dmy);
            check(store.review_sheet().questions.size()==1,"One duplicate question");
            answer(store,"merge-one",ReviewAction::merge,{{"a",0},{"b",0}});
            check(store.current_ledger().size()==1,"Merge one posting");check(store.review_sheet().questions.empty(),"Merged question gone");store.verify();
            bool collision=false;try{store.undo_review("merge-one");}catch(const storage::ImportError&){collision=true;}check(collision,"Answer identity cannot be reused for Undo");
            store.undo_review("undo-one");check(store.current_ledger().size()==2,"Undo restores posting");store.verify();
            answer(store,"no-one",ReviewAction::separate,{},imports::Occurrence{"a",0});check(store.current_ledger().size()==2,"No never merges despite extraneous survivor");check(store.review_sheet().questions.empty(),"No saved");store.verify();
            store.undo_review("undo-no");check(store.review_sheet().questions.size()==1,"No undo restores question");
            answer(store,"merge-two",ReviewAction::merge,{{"a",0},{"b",0}});store.verify();
        }
        {
            auto store=ImportStore::open(path,"test");check(store.current_ledger().size()==1,"Reopen merged");
            store.ingest("c",source("Two","-10.00","changed export"),qif::DateOrder::dmy);check(store.import_result("c").already_present==1,"Alternative representation recognized");store.verify();
        }
        const auto many=std::filesystem::canonical(dir)/"many";std::size_t outstanding=0;
        {
            auto store=ImportStore::create(many,"test","Visa",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));store.create_account(2,"Checking",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));
            for(int account=1;account<=2;++account){store.select_account(account);std::string first="!Type:Bank\n",second=first;
                for(int i=0;i<26;++i){first+=source("A"+std::to_string(i),"-"+std::to_string(i+1)+".00").substr(11);second+=source("B"+std::to_string(i),"-"+std::to_string(i+1)+".00").substr(11);}
                store.ingest("a"+std::to_string(account),first,qif::DateOrder::dmy);store.ingest("b"+std::to_string(account),second,qif::DateOrder::dmy);
            }
            store.ingest("invalid",source("Invalid","bad"),qif::DateOrder::dmy);const auto sheet=store.review_sheet();outstanding=sheet.questions.size();check(outstanding==53,"52 comparisons and one failed record");
            store.select_account(1);check(store.review_sheet().questions.size()==outstanding,"Count independent of account");(void)store.row({"b1",0});check(store.review_sheet().questions.size()==outstanding,"Inspection preserves count");
            try{store.accept_all("blocked");check(false,"Commit must require decisions");}catch(const spentit::storage::ImportError&){}check(store.review_sheet().questions.size()==outstanding,"Commit preserves count");store.verify();
        }
        {auto store=ImportStore::open(many,"test");check(store.review_sheet().questions.size()==outstanding,"Reopen preserves count");}
        const auto groups=std::filesystem::canonical(dir)/"groups";
        {
            auto store=ImportStore::create(groups,"test","Visa",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            store.ingest("first",source("A")+source("A").substr(11),qif::DateOrder::dmy);
            store.ingest("second",source("B")+source("B").substr(11),qif::DateOrder::dmy);
            check(store.review_sheet().questions.at(0).rows.size()==4,"Four row group");
            answer(store,"partial",ReviewAction::merge,{{"first",1},{"second",1}},imports::Occurrence{"first",1});
            check(store.review_sheet().questions.at(0).rows.size()==2,"Two remaining rows stay");store.verify();
            answer(store,"remainder",ReviewAction::merge,{{"first",0},{"second",0}},imports::Occurrence{"first",0});
            check(store.review_sheet().questions.empty(),"Both groups resolved");store.verify();
            store.undo_review("undo-remainder");store.undo_review("undo-partial");check(store.review_sheet().questions.at(0).rows.size()==4,"Repeated Undo restores group");store.verify();
            answer(store,"all-separate",ReviewAction::separate);store.accept_all("accept-separate");
            store.ingest("third",source("C"),qif::DateOrder::dmy);
            const auto q=store.review_sheet().questions.at(0);std::vector<imports::Occurrence> ids;for(const auto& row:q.rows)ids.push_back(row.identity);
            answer(store,"committed-group",ReviewAction::merge,ids,imports::Occurrence{"first",0});check(store.current_ledger().size()==1,"Committed group survivor");store.verify();
            store.undo_review("undo-committed");check(store.current_ledger().size()==5,"Committed group Undo");store.verify();
        }
        {auto store=ImportStore::open(groups,"test");store.verify();}
        const auto barrier=std::filesystem::canonical(dir)/"undo-barrier";
        {
            auto store=ImportStore::create(barrier,"test","Visa",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            store.ingest("first",source("A")+source("A").substr(11),qif::DateOrder::dmy);
            store.ingest("second",source("B")+source("B").substr(11),qif::DateOrder::dmy);
            answer(store,"partial",ReviewAction::merge,{{"first",1},{"second",1}},imports::Occurrence{"first",1});
            store.ingest("intervening",source("Unrelated","-99.00"),qif::DateOrder::dmy);
            check(!store.review_sheet().can_undo,"Intervening mutation disables older Undo");
            answer(store,"remainder",ReviewAction::merge,{{"first",0},{"second",0}},imports::Occurrence{"first",0});
            store.undo_review("undo-remainder");check(!store.review_sheet().can_undo,"Undo must not cross an earlier mutation barrier");
            bool refused=false;try{store.undo_review("unsafe");}catch(const storage::ImportError&){refused=true;}check(refused,"Earlier Undo refused");store.verify();
        }
        const auto replacement=std::filesystem::canonical(dir)/"replacement";
        {
            auto store=ImportStore::create(replacement,"test","Checking",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));store.create_account(2,"Visa",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));store.select_account(1);
            store.ingest("origin",source("Payment")+"",qif::DateOrder::dmy);TransferRequest r{};r.action=TransferAction::from_draft;r.link="generated";r.origin=imports::Occurrence{"origin",0};r.allocation="source-0";r.destination=2;r.expected_sequence=store.transfer_sequence();store.apply_transfer("generate",r);store.accept_all("accept-generated",true);
            store.select_account(2);store.ingest("bank",source("Bank payment","10.00"),qif::DateOrder::dmy);check(store.balance().units()==2000,"Generated and bank before replacement");
            answer(store,"replace-generated",ReviewAction::link,{{"bank",0},{"origin",0}});check(store.balance().units()==1000,"Generated placeholder retired");store.verify();
            store.undo_review("undo-replacement");check(store.balance().units()==2000,"Replacement Undo restores generated");store.verify();
        }
        {auto store=ImportStore::open(replacement,"test");store.verify();}
        const auto links=std::filesystem::canonical(dir)/"links";
        {
            auto store=ImportStore::create(links,"test","Checking",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            store.create_account(2,"Wise",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));store.ingest("in",source("Transfer","10.00"),qif::DateOrder::dmy);
            store.select_account(1);store.ingest("out",std::string("!Type:Bank\nD01/01/2026\nT-10.40\nPTransfer\nL[Wise]\n^\n"),qif::DateOrder::dmy);
            const auto sheet=store.review_sheet();const auto it=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& q){return q.kind==ReviewKind::link;});check(it!=sheet.questions.end(),"Fee link question");
            ReviewAnswer a;a.question=it->key;a.generation=sheet.generation;a.action=ReviewAction::link;a.selected={{"out",0},{"in",0}};a.split_difference=true;store.answer_review("link-fee",a);
            check(store.balance().units()==-1040,"Fee total preserved");check(store.row({"out",0}).draft->transaction.allocations().size()==2,"Fee split created");store.verify();
            store.undo_review("undo-fee");check(store.row({"out",0}).draft->transaction.allocations().size()==1,"Fee split undone");store.verify();
        }
        {auto store=ImportStore::open(links,"test");store.verify();}
        for(const bool accepted:{false,true}) {
            const auto cleanup=std::filesystem::canonical(dir)/(accepted?"cleanup-accepted":"cleanup-drafts");
            {auto store=ImportStore::create(cleanup,"test","Visa",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            store.ingest("a",source("One"),qif::DateOrder::dmy);if(accepted)store.accept_all("accept");store.ingest("b",source("Two"),qif::DateOrder::dmy);
            answer(store,"merge-cleanup",ReviewAction::merge,{{"a",0},{"b",0}},imports::Occurrence{"a",0});
            store.rollback_account("rollback",store.rollback_review().token);check(store.current_ledger().size()==(accepted?1:0),"Rollback preserves accepted survivor only");check(!store.review_sheet().can_undo,"Rollback clears Review stack");store.verify();}
            auto store=ImportStore::open(cleanup,"test");store.verify();
        }
        const auto splits=std::filesystem::canonical(dir)/"split-exact";
        {
            auto store=ImportStore::create(splits,"test","Checking",cad,"2026-01-01");store.opening(0,"2026-01-01",Money(0,cad));
            store.ingest("out",source("Payment","-10.00"),qif::DateOrder::dmy);
            store.create_account(2,"Wise",cad,"2026-01-01");store.select_account(2);store.opening(0,"2026-01-01",Money(0,cad));
            store.ingest("in","!Type:Bank\nD01/01/2026\nT11.50\nPReceipt\nSUncategorized\n$10.00\nSOther\n$1.50\n^\n",qif::DateOrder::dmy);
            const auto before=*store.row({"in",0}).draft;const auto aa=before.transaction.allocations();
            const auto sheet=store.review_sheet();const auto q=std::find_if(sheet.questions.begin(),sheet.questions.end(),[](const auto& x){return x.kind==ReviewKind::link;});check(q!=sheet.questions.end(),"Exact split counterpart despite different totals");
            ReviewAnswer a;a.question=q->key;a.generation=sheet.generation;a.action=ReviewAction::link;a.selected={{"out",0},{"in",0}};
            if(q->rows.front().identity.batch=="in")a.left_allocation=aa[0].identity;else a.right_allocation=aa[0].identity;
            store.answer_review("split-link",a);check(store.balance().units()==1150,"Split total preserved");store.verify();store.undo_review("undo-split");store.verify();
        }
        std::filesystem::remove_all(dir);std::cout<<"Review sheet passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";std::filesystem::remove_all(dir);return 1;}
}
