// Private deterministic encoding for encrypted transfer history; never a source
// file format. Lengths and every numeric field are bounded and checked on read.
namespace transfer_codec {
struct Writer {
    std::string bytes;
    void text(std::string_view value){if(value.size()>qif::max_field)fail(SQLITE_TOOBIG);bytes+=std::to_string(value.size());bytes+=':';bytes+=value;}
    void number(std::int64_t value){text(std::to_string(value));}
    void money(const Money& value){number(value.units());text(value.currency().definition_id());}
    void endpoint(const transfers::Endpoint& value){number(value.account);text(value.entry.batch);number(integer(value.entry.ordinal));text(value.allocation);}
    void evidence(const std::optional<transfers::BankEvidence>& value){number(value?1:0);if(value){endpoint(value->endpoint);money(value->amount);text(value->posting_date);}}
    void side(const transfers::Side& value){endpoint(value.endpoint);money(value.amount);text(value.date);number(static_cast<int>(value.state));number(value.independent_evidence);number(value.provisional_date);number(value.manually_entered);number(value.accepted_unmatched);evidence(value.attached_evidence);evidence(value.proposed_evidence);}
};
struct Reader {
    std::string_view bytes;
    std::string text(){const auto end=bytes.find(':');if(end==bytes.npos || end==0 || end>8)fail(SQLITE_CORRUPT);std::size_t size=0;const auto parsed=std::from_chars(bytes.data(),bytes.data()+end,size);if(parsed.ec!=std::errc{} || parsed.ptr!=bytes.data()+end || size>qif::max_field || size>bytes.size()-end-1)fail(SQLITE_CORRUPT);bytes.remove_prefix(end+1);std::string out(bytes.substr(0,size));bytes.remove_prefix(size);return out;}
    std::int64_t number(){const auto value=text();std::int64_t out=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),out);if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || std::to_string(out)!=value)fail(SQLITE_CORRUPT);return out;}
    bool boolean(){auto n=number();if(n!=0 && n!=1)fail(SQLITE_CORRUPT);return n==1;}
    Money money(){auto units=number();return Money(units,Currency::from_definition(text()));}
    transfers::Endpoint endpoint(){auto account=number();auto batch=text();auto ordinal=number();if(ordinal<0 || ordinal>=static_cast<std::int64_t>(qif::max_records))fail(SQLITE_CORRUPT);return {account,{batch,static_cast<std::size_t>(ordinal)},text()};}
    std::optional<transfers::BankEvidence> evidence(){if(!boolean())return {};auto id=endpoint();auto value=money();return transfers::BankEvidence{id,value,text()};}
    transfers::Side side(){auto id=endpoint();auto amount=money();auto date=text();const auto state=number();if(state<0 || state>2)fail(SQLITE_CORRUPT);transfers::Side out{id,amount,date,static_cast<transfers::State>(state)};out.independent_evidence=boolean();out.provisional_date=boolean();out.manually_entered=boolean();out.accepted_unmatched=boolean();out.attached_evidence=evidence();out.proposed_evidence=evidence();return out;}
    void finish(){if(!bytes.empty())fail(SQLITE_CORRUPT);}
};
std::string encode(const app::TransferView& value){Writer out;out.text(value.link.identity);out.side(value.link.origin);out.side(value.link.destination);out.text(value.link.generated_date);out.text(value.payee);out.text(value.memo);out.number(value.created_sequence);return out.bytes;}
app::TransferView view(std::string_view bytes){Reader in{bytes};auto id=in.text();auto origin=in.side();auto destination=in.side();auto date=in.text();auto payee=in.text();auto memo=in.text();auto sequence=in.number();in.finish();app::TransferView out{{id,origin,destination,date},payee,memo,sequence};transfers::validate(out.link);safe_text(payee);safe_text(memo);if(sequence<=0)fail(SQLITE_CORRUPT);return out;}
std::string encode(const app::TransferRequest& value){Writer out;out.number(static_cast<int>(value.action));out.text(value.link);out.number(value.expected_sequence);out.number(value.origin?1:0);if(value.origin){out.text(value.origin->batch);out.number(integer(value.origin->ordinal));}out.text(value.allocation);out.number(value.batch_revision);out.number(value.destination);out.number(value.amount?1:0);if(value.amount)out.money(*value.amount);out.text(value.date);out.text(value.payee);out.text(value.memo);out.evidence(value.evidence);return out.bytes;}
app::TransferRequest request(std::string_view bytes){Reader in{bytes};auto action=in.number();if(action<0 || action>8)fail(SQLITE_CORRUPT);app::TransferRequest out{};out.action=static_cast<app::TransferAction>(action);out.link=in.text();out.expected_sequence=in.number();if(in.boolean()){auto batch=in.text();auto ordinal=in.number();if(ordinal<0 || ordinal>=static_cast<std::int64_t>(qif::max_records))fail(SQLITE_CORRUPT);out.origin=imports::Occurrence{batch,static_cast<std::size_t>(ordinal)};}out.allocation=in.text();out.batch_revision=in.number();out.destination=in.number();if(in.boolean())out.amount=in.money();out.date=in.text();out.payee=in.text();out.memo=in.text();out.evidence=in.evidence();in.finish();return out;}
}
