#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace spentit::rules {
enum class RegexFailure { invalid_pattern, invalid_input, resource_limit, cancelled, invalid_template, dependency, internal };
class RegexError : public std::runtime_error {
public:
    RegexError(RegexFailure kind, std::string message) : std::runtime_error(std::move(message)), kind_(kind) {}
    RegexFailure kind() const noexcept { return kind_; }
private:
    RegexFailure kind_;
};
struct RegexLimits {
    static constexpr std::size_t pattern_bytes = 4096;
    static constexpr std::size_t subject_bytes = 65536;
    static constexpr std::size_t output_bytes = 65536;
    static constexpr unsigned captures = 32;
    static constexpr unsigned match_steps = 100000;
};
struct RegexMatch {
    // Capture zero is the complete match; a nonparticipating optional capture is absent.
    std::vector<std::optional<std::string>> captures;
};
void validate_source_text(std::string_view text);
class Pattern {
public:
    explicit Pattern(std::string_view expression);
    ~Pattern();
    Pattern(Pattern&&) noexcept;
    Pattern& operator=(Pattern&&) noexcept;
    Pattern(const Pattern&) = delete;
    Pattern& operator=(const Pattern&) = delete;
    // Search, UTF-8 + Unicode properties, case-insensitive by default. Explicit
    // pattern flags may override case behavior. No normalization or JIT.
    std::optional<RegexMatch> match(std::string_view subject,
                                  const std::function<bool()>& cancelled = {}) const;
    unsigned capture_count() const noexcept;
    std::size_t compiled_bytes() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Literal text, $$ for a literal dollar, ${0}..${32} for captures. Unknown or
// malformed references fail. A valid nonparticipating capture expands empty.
void validate_template(std::string_view text, unsigned capture_count);
std::string expand_template(std::string_view text, const RegexMatch& match);
} // namespace spentit::rules
