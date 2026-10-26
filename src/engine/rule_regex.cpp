#include <spentit/rule_regex.hpp>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <utility>

namespace spentit::rules {
namespace {
constexpr std::size_t memory_limit = 8U * 1024U * 1024U;
struct Budget { std::size_t used = 0; };
struct alignas(std::max_align_t) AllocationHeader { std::size_t size; };
void* allocate(PCRE2_SIZE size, void* data) noexcept {
    auto& budget = *static_cast<Budget*>(data);
    if (size > memory_limit - budget.used || size > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader)) return nullptr;
    auto* header = static_cast<AllocationHeader*>(std::malloc(size + sizeof(AllocationHeader)));
    if (!header) return nullptr;
    header->size = size; budget.used += size;
    return header + 1;
}
void release(void* pointer, void* data) noexcept {
    if (!pointer) return;
    auto* header = static_cast<AllocationHeader*>(pointer) - 1;
    static_cast<Budget*>(data)->used -= header->size;
    std::free(header);
}
template<class T, void (*Free)(T*)> using Handle = std::unique_ptr<T, decltype(Free)>;
struct StopState {
    const std::function<bool()>* check;
    unsigned calls = 0;
    bool stopped = false, failed = false, exhausted = false;
};
int callout(pcre2_callout_block*, void* data) noexcept {
    auto& state = *static_cast<StopState*>(data);
    if (++state.calls > RegexLimits::match_steps) { state.exhausted = true; return PCRE2_ERROR_CALLOUT; }
    try { if (*state.check && (*state.check)()) { state.stopped = true; return PCRE2_ERROR_CALLOUT; } }
    catch (...) { state.failed = true; return PCRE2_ERROR_CALLOUT; }
    return 0;
}
bool valid_utf8(std::string_view s) {
    for (std::size_t i=0;i<s.size();) {
        auto c=static_cast<unsigned char>(s[i++]);
        if(c<0x80) { if(c==0) return false; continue; }
        unsigned count; unsigned value;
        if(c>=0xc2 && c<=0xdf) {count=1;value=c&0x1fU;}
        else if(c>=0xe0 && c<=0xef) {count=2;value=c&0x0fU;}
        else if(c>=0xf0 && c<=0xf4) {count=3;value=c&0x07U;}
        else return false;
        if(count>s.size()-i) return false;
        for(unsigned n=0;n<count;++n) { auto next=static_cast<unsigned char>(s[i++]);if((next&0xc0U)!=0x80U)return false;value=(value<<6U)|(next&0x3fU); }
        if((count==1&&value<0x80)||(count==2&&value<0x800)||(count==3&&value<0x10000)||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
    }
    return true;
}
template<class Literal, class Capture>
void visit_template(std::string_view text, unsigned count, Literal literal, Capture capture) {
    if(text.size()>RegexLimits::output_bytes || !valid_utf8(text)) throw RegexError(RegexFailure::invalid_template,"Template must be bounded valid UTF-8 without NUL");
    for(std::size_t i=0;i<text.size();) {
        if(text[i]!='$') {literal(text.substr(i++,1));continue;}
        ++i;
        if(i<text.size()&&text[i]=='$') {literal("$");++i;continue;}
        if(i==text.size()||text[i++]!='{') throw RegexError(RegexFailure::invalid_template,"Use $$ or ${capture number} in templates");
        auto begin=i;unsigned number=0;
        while(i<text.size()&&text[i]>='0'&&text[i]<='9') {
            if(i-begin>=2) throw RegexError(RegexFailure::invalid_template,"Capture reference is out of range");
            number=number*10U+static_cast<unsigned>(text[i++]-'0');
        }
        if(i==begin||i==text.size()||text[i++]!='}'||number>count) throw RegexError(RegexFailure::invalid_template,"Unknown or malformed capture reference");
        capture(number);
    }
}
}
struct Pattern::Impl {
    Budget budget;
    Handle<pcre2_general_context,pcre2_general_context_free> general{nullptr,pcre2_general_context_free};
    Handle<pcre2_code,pcre2_code_free> code{nullptr,pcre2_code_free};
    unsigned captures=0;
    PCRE2_SIZE bytes=0;
};
Pattern::Pattern(std::string_view expression) : impl_(std::make_unique<Impl>()) {
    if(expression.size()>RegexLimits::pattern_bytes||!valid_utf8(expression)) throw RegexError(RegexFailure::invalid_pattern,"Pattern must be bounded valid UTF-8 without NUL");
    std::array<char,64> version{};int unicode=0;
    if(pcre2_config(PCRE2_CONFIG_VERSION,version.data())<0 || std::string_view(version.data()).substr(0,6)!="10.49 " || pcre2_config(PCRE2_CONFIG_UNICODE,&unicode)!=0 || !unicode)
        throw RegexError(RegexFailure::dependency,"Rules require PCRE2 10.49 with Unicode support");
    impl_->general.reset(pcre2_general_context_create(allocate,release,&impl_->budget));
    if(!impl_->general)throw RegexError(RegexFailure::resource_limit,"Pattern memory limit reached");
    Handle<pcre2_compile_context,pcre2_compile_context_free> context(pcre2_compile_context_create(impl_->general.get()),pcre2_compile_context_free);
    if(!context)throw RegexError(RegexFailure::resource_limit,"Pattern memory limit reached");
    pcre2_set_max_pattern_length(context.get(),RegexLimits::pattern_bytes);
    pcre2_set_max_pattern_compiled_length(context.get(),1024U*1024U);
    pcre2_set_parens_nest_limit(context.get(),64);
    int error=0;PCRE2_SIZE offset=0;
    impl_->code.reset(pcre2_compile(reinterpret_cast<PCRE2_SPTR>(expression.data()),expression.size(),PCRE2_UTF|PCRE2_UCP|PCRE2_CASELESS|PCRE2_AUTO_CALLOUT|PCRE2_NEVER_BACKSLASH_C,&error,&offset,context.get()));
    if(!impl_->code) {
        const bool limited=error==PCRE2_ERROR_HEAP_FAILED||error==PCRE2_ERROR_PATTERN_TOO_LARGE||error==PCRE2_ERROR_PARENTHESES_NEST_TOO_DEEP;
        throw RegexError(limited?RegexFailure::resource_limit:RegexFailure::invalid_pattern,"Pattern compilation failed at byte "+std::to_string(offset)+" (code "+std::to_string(error)+")");
    }
    pcre2_pattern_info(impl_->code.get(),PCRE2_INFO_CAPTURECOUNT,&impl_->captures);
    pcre2_pattern_info(impl_->code.get(),PCRE2_INFO_SIZE,&impl_->bytes);
    if(impl_->captures>RegexLimits::captures)throw RegexError(RegexFailure::resource_limit,"Pattern has too many captures");
}
Pattern::~Pattern()=default;
Pattern::Pattern(Pattern&&) noexcept=default;
Pattern& Pattern::operator=(Pattern&&) noexcept=default;
unsigned Pattern::capture_count() const noexcept {return impl_->captures;}
std::size_t Pattern::compiled_bytes() const noexcept {return impl_->bytes;}
void validate_source_text(std::string_view subject) {
    if(subject.size()>RegexLimits::subject_bytes)throw RegexError(RegexFailure::resource_limit,"Rule source field exceeds evaluation limit");
    if(!valid_utf8(subject))throw RegexError(RegexFailure::invalid_input,"Rule source field is not valid UTF-8 text");
}
std::optional<RegexMatch> Pattern::match(std::string_view subject,const std::function<bool()>& cancelled) const {
    validate_source_text(subject);
    bool stopped=false;
    try {stopped=cancelled&&cancelled();}
    catch (...) {throw RegexError(RegexFailure::internal,"Rule cancellation check failed");}
    if(stopped)throw RegexError(RegexFailure::cancelled,"Rule evaluation cancelled");
    // Match allocations are independent, so the compiled pattern is safely shareable.
    Budget budget;
    Handle<pcre2_general_context,pcre2_general_context_free> general(pcre2_general_context_create(allocate,release,&budget),pcre2_general_context_free);
    if(!general)throw RegexError(RegexFailure::resource_limit,"Match memory limit reached");
    Handle<pcre2_match_context,pcre2_match_context_free> context(pcre2_match_context_create(general.get()),pcre2_match_context_free);
    Handle<pcre2_match_data,pcre2_match_data_free> data(pcre2_match_data_create_from_pattern(impl_->code.get(),general.get()),pcre2_match_data_free);
    if(!context||!data)throw RegexError(RegexFailure::resource_limit,"Match memory limit reached");
    pcre2_set_match_limit(context.get(),RegexLimits::match_steps);pcre2_set_depth_limit(context.get(),128);pcre2_set_heap_limit(context.get(),4096);
    StopState state{&cancelled};pcre2_set_callout(context.get(),callout,&state);
    int result=pcre2_match(impl_->code.get(),reinterpret_cast<PCRE2_SPTR>(subject.data()),subject.size(),0,0,data.get(),context.get());
    if(state.stopped)throw RegexError(RegexFailure::cancelled,"Rule evaluation cancelled");
    if(state.failed)throw RegexError(RegexFailure::internal,"Rule cancellation check failed");
    if(state.exhausted||result==PCRE2_ERROR_MATCHLIMIT||result==PCRE2_ERROR_DEPTHLIMIT||result==PCRE2_ERROR_HEAPLIMIT||result==PCRE2_ERROR_NOMEMORY)throw RegexError(RegexFailure::resource_limit,"Rule matching resource limit reached");
    if(result==PCRE2_ERROR_NOMATCH)return std::nullopt;
    if(result<=0)throw RegexError(RegexFailure::internal,"Rule matching failed (code "+std::to_string(result)+")");
    RegexMatch match;auto* offsets=pcre2_get_ovector_pointer(data.get());
    for(unsigned i=0;i<=impl_->captures;++i) {
        auto begin=offsets[2*i],end=offsets[2*i+1];
        if(begin==PCRE2_UNSET)match.captures.emplace_back(std::nullopt);
        else if(begin<=end&&end<=subject.size())match.captures.emplace_back(subject.substr(begin,end-begin));
        else throw RegexError(RegexFailure::internal,"Invalid capture boundaries");
    }
    return match;
}
void validate_template(std::string_view text,unsigned count) {
    if(count>RegexLimits::captures)throw RegexError(RegexFailure::invalid_template,"Invalid capture count");
    visit_template(text,count,[](std::string_view){},[](unsigned){});
}
std::string expand_template(std::string_view text,const RegexMatch& match) {
    if(match.captures.empty()||match.captures.size()>RegexLimits::captures+1)throw RegexError(RegexFailure::invalid_template,"Invalid match captures");
    std::string result;
    auto append=[&](std::string_view value) {if(value.size()>RegexLimits::output_bytes-result.size())throw RegexError(RegexFailure::resource_limit,"Rule output exceeds limit");result+=value;};
    visit_template(text,static_cast<unsigned>(match.captures.size()-1),append,[&](unsigned n){if(match.captures[n])append(*match.captures[n]);});
    return result;
}
} // namespace spentit::rules
