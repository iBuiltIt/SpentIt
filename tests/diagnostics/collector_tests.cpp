#include <spentit/diagnostics.hpp>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <new>
#include <spentit/operation_context.hpp>
static std::atomic<bool> failAllocation{
    false
};
void* operator new(std::size_t n){
    if(failAllocation.exchange(false))throw std::bad_alloc();
    if(auto* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void* operator new[](std::size_t n){
    return ::operator new(n);
}
void operator delete[](void* p) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    ::operator delete(p);
}
using namespace spentit::diagnostics;
static void check(bool v){
    if(!v){
        std::cerr
        << "diagnostic contract failed\n";
        std::exit(1);
    }
}
int main(){
    auto& c=Collector::instance();
    check(!c.enabled());
    check(c.admit(Action::open)==0);
#ifndef SPENTIT_DIAGNOSTICS
    check(!c.enable());
    return 0;
#else
    check(c.enable());
    check(action("resolve_import_categories")==Action::resolve_import_categories);
    check(action("undo_action")==Action::undo_action);
    check(name(Code::import_category_assign)=="import_category_assign");
    check(action("CANARY-payee-/private/password-1234")==Action::unclassified);
    check(name(static_cast<Action>(9000))=="unclassified");
    auto id=c.admit(Action::open);
    c.start(id);
    c.finish(id,Result::succeeded,Persistence::confirmed_saved,Presentation::failed);
    c.applied(id,true);
    auto s=c.snapshot();
    check(s.latest.persistence==Persistence::confirmed_saved && s.latest.presentation==Presentation::failed);
    Ui ui;
    ui.generation=s.view-1;
    ui.interaction_ready=true;
    c.publish(ui);
    check(c.snapshot().ui.time==0);
    ui.generation=s.view;
    ui.native_exposed=false;
    c.publish(ui);
    c.frame(s.view);
    check(c.snapshot().latest.presentation==Presentation::failed);
    auto second=c.admit(Action::query);
    c.start(second);
    c.finish(second,Result::succeeded,Persistence::not_applicable,Presentation::awaiting_refresh);
    c.applied(second);
    ui.generation=c.snapshot().view;
    ui.interaction_ready=true;
    c.publish(ui);
    check(c.snapshot().latest.presentation==Presentation::unavailable);
    c.frame(ui.generation-1);
    check(c.snapshot().latest.presentation==Presentation::unavailable);
    ui.native_exposed=true;c.publish(ui);auto revision=c.snapshot().ui.revision;
    c.frame(ui.generation,revision-1);check(c.snapshot().latest.presentation==Presentation::ui_settled);
    c.frame(ui.generation,revision);check(c.snapshot().latest.presentation==Presentation::presentation_observed);
    ui.surface=Surface::settings;c.publish(ui);check(c.snapshot().ui.revision>revision);
    c.frame(ui.generation,revision);check(c.snapshot().latest.presentation==Presentation::ui_settled);
    std::vector<std::thread> threads;
    for(int n=0;n<4;++n)threads.emplace_back([&]{
        for(int i=0;i<5000;++i){
            Span span(Code::verify);c.event({});if(i%100==0)check(c.json().size()<4*1024*1024);
        }
    });
    for(auto& t:threads)t.join();
    for(int i=0;i<5000;++i)c.event({});
    s=c.snapshot();
    check(s.cursor_stale && s.truncated && s.event_count==128 && s.dropped>0);
    check(s.latest.id==second);
    spentit::app::OperationContext gate;
    gate.observeCancellation=[](auto){
        throw std::runtime_error("CANARY-exception");
    };
    check(gate.request());
    bool cancelled=false;
    try{
        gate.seal();
    } catch(const spentit::app::OperationCancelled&){
        cancelled=true;
    }
    check(cancelled);
    spentit::app::OperationContext sealed;
    sealed.seal();
    check(!sealed.request());
    const auto outcomeBefore=c.snapshot().latest;
    failAllocation=true;
    auto failure=c.json();
    check(failure.find("collector_failure")!=std::string::npos);
    check(c.snapshot().latest.id==outcomeBefore.id);
    auto json=c.json();
    check(json.find("CANARY")==std::string::npos);
    check(c.snapshot(UINT64_MAX).event_count==0);
    c.enable(false);
    auto sequence=c.snapshot().sequence;
    c.event({});
    check(c.snapshot().sequence==sequence);
    return 0;
#endif
}
