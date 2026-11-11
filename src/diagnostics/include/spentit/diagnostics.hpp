#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
namespace spentit::diagnostics {
    using Id=std::uint64_t;
    enum class Action {
        unclassified, apply_bulk, apply_category, apply_change, archive_account, backup, cancel_bulk, cancel_category_preview, cancel_change, cancel_import_mapping, cancel_merge, cancel_reapply, category_list, category_select, close, commit, correct_opening, create, create_account, decide, delete_account, discard, entry_history, evidence, group_abort, group_balance, group_resolve_transfers, history, import_file, inspect, map_import, open, opening, prepare_create, preview_bulk, preview_category, preview_change, probe, query, reactivate_account, reassign_currency, rename_account, resume, rules_apply, rules_discard, rules_load, rules_prefill, rules_preview, rules_reapply_apply, rules_reapply_preview, rules_save, save, select, select_account, transfer_accept, transfer_candidates, transfer_clear, transfer_clear_row, transfer_commit, transfer_from_draft, transfer_manual, transfer_match, transfer_merge, transfer_merge_apply, transfer_retarget, transfer_unmatch, undo_account, unlock, review_answer, review_undo, review_batch, accept_edit, undo_edit, resolve_import_categories, undo_action, change_password, count
    };
    enum class Code {
        unclassified, startup, qml_creation, worker, snapshot, model_apply, cleanup, verify, integrity, accounts, groups, evidence, categories, rules, transfers, committed, bulk, reapply, ledger, search, catalog, metadata, group_cache, statement_prepare, statement_reuse, transaction, gateway, available, requested, sealed, capture, prepare, import_capture, import_prepare, import_populate, import_resolve_transfers, import_generate_transfers, review_build, review_write, review_refresh, import_category_resolution, import_category_assign, import_category_rules, count
    };
    enum class EventKind {
        admitted, rejected_busy, deferred, superseded, rejected_invalid, started, finished, model_applied, cancellation, span, cache_hit, cache_miss, cache_bypass, invalidated, durable_stage
    };
    enum class Result {
        pending, succeeded, failed, cancelled, superseded, not_executed
    };
    enum class Persistence {
        not_applicable, not_attempted, confirmed_unchanged, confirmed_saved, unknown
    };
    enum class Presentation {
        awaiting_refresh, model_applied, ui_settled, presentation_observed, unavailable, failed
    };
    enum class Surface {
        unknown, welcome, setup, unlock, ledger, categories, rules, history, settings, reports, review
    };
    struct Operation {
        Id id=0, document=0, view=0, queued=0, started=0, ended=0, delivered=0, saved_stages=0;
        Action action{};
        Result result{};
        Persistence persistence=Persistence::unknown;
        Presentation presentation{};
        Code cancellation=Code::available;
    };
    struct Event {
        Id sequence=0,time=0,operation=0,parent=0,duration=0,exclusive=0;
        EventKind kind{};
        Code code{};
        Action action{};
        Id span_id=0;
        bool failed=false;
    };
    struct Metric {
        Id calls=0,inclusive=0,exclusive=0,failed=0,running=0;
    };
    struct Ui {
        Id generation=0,revision=0,time=0,modal_mask=0,pending_mask=0,control_visible=0,control_enabled=0;
        Surface surface{};
        std::uint32_t selection=0,filtered=0,loaded=0,delegates=0,blockers=0,blockers_total=0;
        int first=-1,last=-1,sort=0,focus_role=0,editor_kind=0;
        std::array<unsigned,32> modal_stack{};
        std::size_t modal_count=0;
        bool sidebar=false,dirty=false,search=false,search_valid=false,projected=false,descending=false,selected_visible=false,interaction_ready=false,native_exposed=false;
    };
    struct Snapshot {
        Id session=0,sequence=0,now=0,document=0,view=0,worker_time=0,dropped=0,health_failures=0,cleanup=0,deferred=0,ui_gap=0;
        bool enabled=false;
        Operation active{},latest{};
        std::array<Operation,32> outcomes{};
        Ui ui{};
        std::array<Metric,static_cast<unsigned>(Code::count)> metrics{};
        std::array<Event,128> events{};
        std::size_t event_count=0;
        bool cursor_stale=false,truncated=false;
    };
    Id now() noexcept;
    Action action(std::string_view) noexcept;
    Surface surface(std::string_view) noexcept;
    std::string_view name(Action) noexcept;
    std::string_view name(Code) noexcept;
    class Collector {
        public: static Collector& instance() noexcept;
        bool enable(bool=true) noexcept;
        void healthFailure() noexcept {failed_.fetch_add(1,std::memory_order_relaxed);}
        Id generation() const noexcept {
            return generation_.load(std::memory_order_relaxed);
        }
        bool enabled() const noexcept {
            return enabled_.load(std::memory_order_relaxed);
        }
        Id admit(Action,EventKind=EventKind::admitted,Id reuse=0) noexcept;
        void start(Id) noexcept;
        void finish(Id,Result,Persistence,Presentation) noexcept;
        void applied(Id,bool newDocument=false) noexcept;
        void cancel(Id,Code) noexcept;
        void job(bool starting) noexcept;
        void deferred(Id) noexcept;
        void publish(Ui) noexcept;
        void frame(Id,Id revision=0) noexcept;
        void event(Event) noexcept;
        void saved(Code) noexcept;
        void statement(bool reused) noexcept;
        void enter(Code) noexcept;
        void metric(Code,Id,Id,bool,bool span=false) noexcept;
        Snapshot snapshot(Id after=0) const noexcept;
        std::string json(Id after=0) const noexcept;
        private: mutable std::mutex mutex_;
        std::atomic<bool> enabled_{
            false
        };
        std::atomic<Id> generation_{
            0
        };
        std::atomic<Id> prepares_{0},reuses_{0};
        std::atomic<Id> failed_{
            0
        };
        Snapshot state_{};
        std::array<Event,4096> ring_{};
        Id next_=0;
    };
    struct Context {
        Id operation=0,parent=0;
        bool saved=false;
    };
    Context& context() noexcept;
    class Scope {
        Context prior_;
        public: explicit Scope(Id id) noexcept;
        ~Scope();
    };
    class Span {
        Code code_;
        Id start_=0,id_=0,child_=0;
        int exceptions_=0;
        Span* parent_=nullptr;
        public: explicit Span(Code) noexcept;
        ~Span();
        Span(const Span&)=delete;
        Span& operator=(const Span&)=delete;
    };
    void cache(Code,EventKind) noexcept;
    void durable(Code) noexcept;
}
