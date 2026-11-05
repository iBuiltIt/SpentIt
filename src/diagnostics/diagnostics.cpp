#include <spentit/diagnostics.hpp>
#include <algorithm>
#include <chrono>
#include <exception>
#include <sstream>
namespace spentit::diagnostics {
    Id now() noexcept {
        return static_cast<Id>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    static constexpr std::string_view actions[]={
        "unclassified","apply_bulk","apply_category","apply_change","archive_account","backup","cancel_bulk","cancel_category_preview","cancel_change","cancel_import_mapping","cancel_merge","cancel_reapply","category_list","category_select","close","commit","correct_opening","create","create_account","decide","delete_account","discard","entry_history","evidence","group_abort","group_balance","group_resolve_transfers","history","import","inspect","map_import","open","opening","prepare_create","preview_bulk","preview_category","preview_change","probe","query","reactivate_account","reassign_currency","rename_account","resume","rules_apply","rules_discard","rules_load","rules_prefill","rules_preview","rules_reapply_apply","rules_reapply_preview","rules_save","save","select","select_account","transfer_accept","transfer_candidates","transfer_clear","transfer_clear_row","transfer_commit","transfer_from_draft","transfer_manual","transfer_match","transfer_merge","transfer_merge_apply","transfer_retarget","transfer_unmatch","undo_account","unlock","review_answer","review_undo","review_batch","accept_edit","undo_edit","resolve_import_categories"
    };
    static constexpr std::string_view codes[]={
        "unclassified","startup","qml_creation","worker","snapshot","model_apply","cleanup","verify","integrity","accounts","groups","evidence","categories","rules","transfers","committed","bulk","reapply","ledger","search","catalog","metadata","group_cache","statement_prepare","statement_reuse","transaction","gateway","available","requested","sealed","capture","prepare","import_capture","import_prepare","import_populate","import_resolve_transfers","import_generate_transfers","review_build","review_write","review_refresh","import_category_resolution","import_category_assign","import_category_rules"
    };
    std::string_view name(Action a) noexcept {
        auto n=static_cast<unsigned>(a);
        return n<std::size(actions)?actions[n]:"unclassified";
    }
    std::string_view name(Code a) noexcept {
        auto n=static_cast<unsigned>(a);
        return n<std::size(codes)?codes[n]:"unclassified";
    }
    Action action(std::string_view s) noexcept {
        for(unsigned i=1;i<std::size(actions);++i)if(actions[i]==s)return static_cast<Action>(i);
        return Action::unclassified;
    }
    Surface surface(std::string_view s) noexcept {
        constexpr std::string_view names[]={
            "unknown","welcome","setup","unlock","ledger","categories","rules","history","settings","reports","review"
        };
        for(unsigned i=0;i<std::size(names);++i)if(names[i]==s)return static_cast<Surface>(i);
        return Surface::unknown;
    }
    Collector& Collector::instance() noexcept {
        static Collector c;
        return c;
    }
    bool Collector::enable(bool v) noexcept {
#ifdef SPENTIT_DIAGNOSTICS
        try {
            std::lock_guard lock(mutex_);
            if(v && !state_.session)state_.session=now();
            enabled_.store(v);return true;
        } catch(...) {++failed_;return false;}
#else
        (void)v;
        return false;
#endif
    }
    Id Collector::admit(Action a,EventKind kind,Id reuse) noexcept {
        if(!enabled())return 0;
        try {
            Id id;
            {
                std::lock_guard lock(mutex_);
                id=reuse?reuse:++next_;
                if(!state_.session)state_.session=now();
                if(kind==EventKind::admitted){
                    state_.active={};
                    state_.active.id=id;
                    state_.active.action=a;
                    state_.active.queued=now();
                    state_.active.document=state_.document;
                    state_.active.view=state_.view+1;
                } else{
                    Operation attempt{};
                    attempt.id=id;
                    attempt.action=a;
                    attempt.queued=now();
                    attempt.document=state_.document;
                    attempt.view=state_.view;
                    attempt.result=kind==EventKind::deferred?Result::pending:kind==EventKind::superseded?Result::superseded:Result::not_executed;
                    attempt.persistence=Persistence::not_attempted;
                    attempt.ended=kind==EventKind::deferred?0:now();
                    state_.outcomes[id%32]=attempt;
                }
            }
            event({
                0,0,id,0,0,0,kind,Code::unclassified,a
            });
            return id;
        } catch(...){
            ++failed_;
            return 0;
        }
    }
    void Collector::start(Id id) noexcept {
        if(!id)return;
        try{
            {
                std::lock_guard l(mutex_);
                if(state_.active.id==id)state_.active.started=state_.worker_time=now();
            }
            event({
                0,0,id,0,0,0,EventKind::started
            });
        } catch(...){
            ++failed_;
        }
    }
    void Collector::finish(Id id,Result r,Persistence p,Presentation v) noexcept {
        if(!id)return;
        try{
            {
                std::lock_guard l(mutex_);
                if(state_.active.id!=id)return;
                auto& a=state_.active;
                a.ended=state_.worker_time=now();
                a.result=r;
                a.persistence=p;
                a.presentation=v;
                state_.latest=a;
                state_.outcomes[id%32]=a;
            }
            event({
                0,0,id,0,0,0,EventKind::finished
            });
        } catch(...){
            ++failed_;
        }
    }
    void Collector::applied(Id id,bool doc) noexcept {
        if(!id)return;
        try{
            {
                std::lock_guard l(mutex_);
                if(state_.active.id!=id)return;
                if(doc)++state_.document;
                ++state_.view;
                generation_.store(state_.view,std::memory_order_relaxed);
                auto& a=state_.active;
                a.document=state_.document;
                a.view=state_.view;
                a.delivered=now();
                if(a.presentation!=Presentation::failed)a.presentation=Presentation::model_applied;
                state_.latest=a;
                state_.outcomes[id%32]=a;
            }
            event({
                0,0,id,0,0,0,EventKind::model_applied
            });
        } catch(...){
            ++failed_;
        }
    }
    void Collector::cancel(Id id,Code c) noexcept {
        if(!id)return;
        try{
            {
                std::lock_guard l(mutex_);
                if(state_.active.id==id)state_.active.cancellation=c;
            }
            event({
                0,0,id,0,0,0,EventKind::cancellation,c
            });
        } catch(...){
            ++failed_;
        }
    }
    void Collector::job(bool start) noexcept {
        if(!enabled())return;
        try{
            std::lock_guard l(mutex_);
            if(start)++state_.cleanup;
            else if(state_.cleanup)--state_.cleanup;
        } catch(...){
            ++failed_;
        }
    }
    void Collector::deferred(Id id) noexcept {
        if(!enabled())return;
        try{
            std::lock_guard l(mutex_);
            state_.deferred=id;
        } catch(...){
            ++failed_;
        }
    }
    void Collector::publish(Ui ui) noexcept {
        if(!enabled())return;
        try{
            std::lock_guard l(mutex_);
            if(ui.generation!=state_.view)return;
            ui.modal_count=std::min(ui.modal_count,ui.modal_stack.size());
            const auto& old=state_.ui;
            const bool changed=old.generation!=ui.generation || old.surface!=ui.surface || old.modal_mask!=ui.modal_mask || old.pending_mask!=ui.pending_mask || old.control_enabled!=ui.control_enabled || old.control_visible!=ui.control_visible || old.dirty!=ui.dirty || old.first!=ui.first || old.last!=ui.last || old.interaction_ready!=ui.interaction_ready || old.native_exposed!=ui.native_exposed;
            ui.revision=old.revision+(changed || !old.time?1:0);
            if(changed && state_.latest.presentation!=Presentation::failed)state_.latest.presentation=Presentation::model_applied;
            ui.time=now();
            if(state_.ui.time)state_.ui_gap=std::max(state_.ui_gap,ui.time-state_.ui.time);
            state_.ui=ui;
            if(state_.latest.view==ui.generation && state_.latest.presentation!=Presentation::failed && ui.interaction_ready){
                if(!ui.native_exposed)state_.latest.presentation=Presentation::unavailable;
                else if(state_.latest.presentation!=Presentation::presentation_observed)state_.latest.presentation=Presentation::ui_settled;
                state_.outcomes[state_.latest.id%32]=state_.latest;
            }
        } catch(...){
            ++failed_;
        }
    }
    void Collector::frame(Id generation,Id revision) noexcept {
        if(!enabled())return;
        try{
            std::lock_guard l(mutex_);
            if(state_.ui.generation==generation && state_.ui.revision==revision && state_.view==generation && state_.ui.native_exposed && state_.ui.interaction_ready && state_.latest.presentation!=Presentation::failed){
                state_.latest.presentation=Presentation::presentation_observed;
                state_.outcomes[state_.latest.id%32]=state_.latest;
            }
        } catch(...){
            ++failed_;
        }
    }
    void Collector::event(Event e) noexcept {
        if(!enabled())return;
        try{
            std::unique_lock l(mutex_,std::try_to_lock);
            if(!l.owns_lock()){
                ++failed_;
                return;
            }
            e.sequence=++state_.sequence;
            e.time=now();
            if(!e.operation)e.operation=context().operation;
            ring_[(e.sequence-1)%ring_.size()]=e;
            if(e.sequence>ring_.size())++state_.dropped;
        } catch(...){
            ++failed_;
        }
    }
    void Collector::saved(Code code) noexcept {
        if(!enabled())return;
        try {std::lock_guard lock(mutex_);if(context().operation && state_.active.id==context().operation)state_.active.saved_stages|=code==Code::capture?1:code==Code::prepare?2:4;}catch(...){++failed_;}
    }
    void Collector::statement(bool reused) noexcept {if(!enabled())return;(reused?reuses_:prepares_).fetch_add(1,std::memory_order_relaxed);}
    void Collector::enter(Code c) noexcept {
        if(!enabled())return;
        try{
            std::lock_guard l(mutex_);
            auto n=static_cast<unsigned>(c);
            if(n>=state_.metrics.size())n=0;
            ++state_.metrics[n].running;
            if(c!=Code::model_apply && c!=Code::startup && c!=Code::qml_creation)state_.worker_time=now();
        } catch(...){
            ++failed_;
        }
    }
    void Collector::metric(Code c,Id duration,Id exclusive,bool failed,bool span) noexcept {
        if(!enabled())return;
        try{
            std::lock_guard l(mutex_);
            auto n=static_cast<unsigned>(c);
            if(n>=state_.metrics.size())n=0;
            auto& m=state_.metrics[n];
            ++m.calls;
            m.inclusive+=duration;
            m.exclusive+=exclusive;
            m.failed+=failed;
            if(span && m.running)--m.running;
            if(c!=Code::model_apply && c!=Code::startup && c!=Code::qml_creation)state_.worker_time=now();
        } catch(...){
            ++failed_;
        }
    }
    Snapshot Collector::snapshot(Id after) const noexcept {
        try{
            std::lock_guard l(mutex_);
            auto s=state_;
            s.metrics[static_cast<unsigned>(Code::statement_prepare)].calls=prepares_.load(std::memory_order_relaxed);
            s.metrics[static_cast<unsigned>(Code::statement_reuse)].calls=reuses_.load(std::memory_order_relaxed);
            s.enabled=enabled();
            s.now=now();
            s.health_failures=failed_.load();
            const auto first=s.sequence>4096?s.sequence-4096+1:1;
            s.cursor_stale=after<first-1;
            auto begin=std::max(first,after==UINT64_MAX?after:after+1);
            for(auto i=begin;i<=s.sequence && s.event_count<s.events.size();++i)s.events[s.event_count++]=ring_[(i-1)%ring_.size()];
            s.truncated=s.event_count && s.events[s.event_count-1].sequence<s.sequence;
            return s;
        } catch(...){
            return {};
        }
    }
    std::string Collector::json(Id after) const noexcept {
        try{
            const auto s=snapshot(after);
            std::ostringstream o;
            o.exceptions(std::ios::badbit | std::ios::failbit);
            auto op=[&](const Operation& a){
                o
                << "{\"id\":"
                << a.id
                << ",\"action\":\""
                << name(a.action)
                << "\",\"document\":"
                << a.document
                << ",\"view\":"
                << a.view
                << ",\"queued_us\":"
                << a.queued
                << ",\"started_us\":"
                << a.started
                << ",\"ended_us\":"
                << a.ended
                << ",\"delivered_us\":"
                << a.delivered << ",\"saved_stages\":" << a.saved_stages
                << ",\"result\":"
                << static_cast<int>(a.result)
                << ",\"persistence\":"
                << static_cast<int>(a.persistence)
                << ",\"presentation\":"
                << static_cast<int>(a.presentation)
                << ",\"cancellation\":\""
                << name(a.cancellation)
                << "\",\"recovery\":\""
                << ((a.presentation==Presentation::failed || a.persistence==Persistence::unknown || (a.persistence==Persistence::confirmed_saved && a.result!=Result::succeeded))?"reopen_and_inspect":"none")
                << "\",\"error\":\""
                << (a.result==Result::failed?"unclassified":"none")
                << "\"}";
            };
            o
            << "{\"schema\":1,\"session\":"
            << s.session
            << ",\"enabled\":"
            << s.enabled
            << ",\"sequence\":"
            << s.sequence
            << ",\"now_us\":"
            << s.now
            << ",\"document\":"
            << s.document
            << ",\"view\":"
            << s.view
            << ",\"worker_age_us\":"
            << (s.worker_time?s.now-s.worker_time:0)
            << ",\"ui_age_us\":"
            << (s.ui.time?s.now-s.ui.time:0)
            << ",\"worker_known\":"
            << (s.worker_time!=0)
            << ",\"ui_known\":"
            << (s.ui.time!=0)
            << ",\"dropped\":"
            << s.dropped
            << ",\"health_failures\":"
            << s.health_failures
            << ",\"cleanup_jobs\":"
            << s.cleanup
            << ",\"deferred\":"
            << s.deferred
            << ",\"ui_max_sample_gap_us\":"
            << s.ui_gap
            << ",\"active\":";
            op(s.active);
            o
            << ",\"latest\":";
            op(s.latest);
            o
            << ",\"outcomes\":[";
            bool comma=false;
            for(const auto& a:s.outcomes)if(a.id){
                if(comma)o
                << ',';
                comma=true;
                op(a);
            }
            o
            << "],\"ui\":{\"generation\":"
            << s.ui.generation << ",\"revision\":" << s.ui.revision
            << ",\"surface\":"
            << static_cast<int>(s.ui.surface)
            << ",\"modal_mask\":"
            << s.ui.modal_mask
            << ",\"pending_mask\":"
            << s.ui.pending_mask
            << ",\"control_visible\":"
            << s.ui.control_visible
            << ",\"control_enabled\":"
            << s.ui.control_enabled
            << ",\"selection\":"
            << s.ui.selection
            << ",\"filtered_rows\":"
            << s.ui.filtered
            << ",\"loaded_rows\":"
            << s.ui.loaded
            << ",\"delegates\":"
            << s.ui.delegates
            << ",\"first\":"
            << s.ui.first
            << ",\"last\":"
            << s.ui.last
            << ",\"sort\":"
            << s.ui.sort
            << ",\"sidebar\":"
            << s.ui.sidebar
            << ",\"dirty\":"
            << s.ui.dirty
            << ",\"search_present\":"
            << s.ui.search
            << ",\"search_valid\":"
            << s.ui.search_valid
            << ",\"projected\":"
            << s.ui.projected
            << ",\"descending\":"
            << s.ui.descending
            << ",\"selected_visible\":"
            << s.ui.selected_visible
            << ",\"interaction_ready\":"
            << s.ui.interaction_ready
            << ",\"native_exposed\":"
            << s.ui.native_exposed
            << ",\"blockers_displayed\":"
            << s.ui.blockers
            << ",\"blockers_total\":"
            << s.ui.blockers_total
            << ",\"focus_role\":"
            << s.ui.focus_role
            << ",\"editor_kind\":" << s.ui.editor_kind << ",\"modal_stack\":[";
            for(std::size_t i=0;i<s.ui.modal_count;++i){if(i)o<<',';o<<s.ui.modal_stack[i];}
            o << "]},\"metrics\":[";
            comma=false;
            for(unsigned i=0;i<s.metrics.size();++i){
                auto m=s.metrics[i];
                if(!m.calls && !m.running)continue;
                if(comma)o
                << ',';
                comma=true;
                o
                << "{\"code\":\""
                << name(static_cast<Code>(i))
                << "\",\"calls\":"
                << m.calls
                << ",\"inclusive_us\":"
                << m.inclusive
                << ",\"exclusive_us\":"
                << m.exclusive
                << ",\"failed\":"
                << m.failed
                << ",\"running\":"
                << m.running
                << '}';
            }
            o
            << "],\"cursor_stale\":"
            << s.cursor_stale
            << ",\"truncated\":"
            << s.truncated
            << ",\"events\":[";
            for(std::size_t i=0;i<s.event_count;++i){
                if(i)o
                << ',';
                auto e=s.events[i];
                o
                << "{\"sequence\":"
                << e.sequence
                << ",\"time_us\":"
                << e.time
                << ",\"operation\":"
                << e.operation
                << ",\"parent\":"
                << e.parent
                << ",\"kind\":"
                << static_cast<int>(e.kind)
                << ",\"code\":\""
                << name(e.code)
                << "\",\"action\":\""
                << name(e.action)
                << "\",\"duration_us\":"
                << e.duration
                << ",\"exclusive_us\":"
                << e.exclusive
                << ",\"span_id\":"
                << e.span_id
                << ",\"failed\":"
                << e.failed
                << '}';
            }
            o
            << "],\"unsupported\":[\"page_metrics\",\"gpu_latency\",\"physical_presentation\",\"unregistered_controls\",\"delegate_reuse\",\"sql_timing\"],\"sampling\":\"ui_10hz\"}";
            auto out=o.str();
            if(out.size()>4*1024*1024)return "{\"error\":\"response_limit\"}";
            return out;
        } catch(...){
            return "{\"error\":\"collector_failure\"}";
        }
    }
    Context& context() noexcept {
        thread_local Context c;
        return c;
    }
    Scope::Scope(Id id) noexcept:prior_(context()){
        context()={
            id,0
        };
    }
    Scope::~Scope(){
        context()=prior_;
    }
    static thread_local Span* current_span=nullptr;
    static std::atomic<Id> next_span{0};
    Span::Span(Code c) noexcept:code_(c){
        if(!Collector::instance().enabled())return;
        Collector::instance().enter(c);
        start_=now();
        id_=next_span.fetch_add(1,std::memory_order_relaxed)+1;
        exceptions_=std::uncaught_exceptions();
        parent_=current_span;
        current_span=this;
    }
    Span::~Span(){
        if(!start_)return;
        auto duration=now()-start_;
        current_span=parent_;
        if(parent_)parent_->child_+=duration;
        const auto exclusive=duration-std::min(duration,child_);
        Collector::instance().metric(code_,duration,exclusive,std::uncaught_exceptions()>exceptions_,true);
        Event event{
            0,0,context().operation,parent_?parent_->id_:0,duration,exclusive,EventKind::span,code_
        };
        event.span_id=id_;
        event.failed=std::uncaught_exceptions()>exceptions_;
        Collector::instance().event(event);
    }
    void durable(Code c) noexcept {
        if(!Collector::instance().enabled())return;
        context().saved=true;Collector::instance().saved(c);
        Collector::instance().event({
            0,0,context().operation,0,0,0,EventKind::durable_stage,c
        });
    }
    void cache(Code c,EventKind k) noexcept {
        if(!Collector::instance().enabled())return;
        Collector::instance().event({
            0,0,context().operation,0,0,0,k,c
        });
    }
}
