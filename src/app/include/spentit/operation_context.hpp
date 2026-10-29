#pragma once
#include <atomic>
#include <functional>
#include <stdexcept>
#include <string_view>
namespace spentit::app {
class OperationCancelled final : public std::runtime_error {
public: explicit OperationCancelled(const char* message="Operation cancelled. Existing saved work is unchanged."):std::runtime_error(message){}
};
class OperationOutcomeUnknown final : public std::runtime_error {
public: OperationOutcomeUnknown():std::runtime_error("Import outcome is uncertain. Reopen this dataset to inspect retained evidence before retrying."){}
};
// Shared cancellation gate. Sealing and requesting race atomically: a request
// accepted before sealing must be observed before any durable command begins.
struct OperationContext {
    enum State { available, requested, sealed };
    mutable std::atomic<State> state{available};
    std::function<void(std::string_view)> progress;
    // Optional observation is contained: it cannot veto or acknowledge work.
    std::function<void(State)> observeCancellation;
    void observeState() const noexcept {try{if(observeCancellation)observeCancellation(state.load());}catch(...) {}}
    bool request() const { auto expected=available; const bool accepted=state.compare_exchange_strong(expected,requested);if(accepted)observeState();return accepted; }
    void checkpoint(std::string_view phase={}) const {
        if(!phase.empty() && progress) progress(phase);
        if(state.load()==requested) throw OperationCancelled();
    }
    // state is mutable so a const command can seal the shared gate.
    void seal() const {
        auto expected=available;
        if(!state.compare_exchange_strong(expected,sealed) && expected==requested) throw OperationCancelled();
        observeState();
    }
};
}
