#include "file_exchange_codec.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <set>
namespace spentit::ui::exchange {
namespace {
QString q(std::string_view s){return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
std::string s(const QString& v){return v.toUtf8().toStdString();}
std::string id(){return s(QUuid::createUuid().toString(QUuid::WithoutBraces));}
[[noreturn]] void invalid(){throw std::invalid_argument("This is not a supported SpentIt export file.");}
QString string(const QJsonObject& o,const QString& key){if(!o.value(key).isString() || o.value(key).toString().size()>65536)invalid();return o.value(key).toString();}
bool boolean(const QJsonObject& o,const QString& key){if(!o.value(key).isBool())invalid();return o.value(key).toBool();}
QJsonArray array(const QJsonObject& o,const QString& key,qsizetype max=4096){if(!o.value(key).isArray())invalid();auto a=o.value(key).toArray();if(a.size()>max)invalid();return a;}
QJsonObject object(const QJsonValue& v){if(!v.isObject())invalid();return v.toObject();}
void keys(const QJsonObject& o,std::initializer_list<QString> allowed){for(auto i=o.begin();i!=o.end();++i)if(std::find(allowed.begin(),allowed.end(),i.key())==allowed.end())invalid();}
// Qt's JSON reader accepts repeated keys; exchange files must not silently pick
// the last definition of a field (including differently escaped identical keys).
void uniqueKeys(const QByteArray& bytes) {
    struct Frame {bool object,key;std::set<QString> seen;};
    std::vector<Frame> stack;
    for(qsizetype i=0;i<bytes.size();++i){
        const auto c=bytes[i];
        if(c=='"'){
            const auto begin=i++;bool escaped=false;
            for(;i<bytes.size();++i){if(escaped){escaped=false;continue;}if(bytes[i]=='\\'){escaped=true;continue;}if(bytes[i]=='"')break;}
            if(!stack.empty() && stack.back().object && stack.back().key){const auto token=bytes.mid(begin,i-begin+1);const auto key=QJsonDocument::fromJson("["+token+"]").array().first().toString();if(!stack.back().seen.insert(key).second)invalid();}
        } else if(c=='{' || c=='['){if(stack.size()>=32)invalid();stack.push_back({c=='{',true,{}});}
        else if(c=='}' || c==']'){if(stack.empty())invalid();stack.pop_back();}
        else if(c==':' && !stack.empty())stack.back().key=false;
        else if(c==',' && !stack.empty())stack.back().key=true;
    }
}
void unicode(const QJsonValue& value){
    if(value.isString()){const auto text=value.toString();for(qsizetype i=0;i<text.size();++i){if(text[i].isHighSurrogate()){if(++i==text.size() || !text[i].isLowSurrogate())invalid();}else if(text[i].isLowSurrogate())invalid();}}
    else if(value.isArray())for(const auto& v:value.toArray())unicode(v);
    else if(value.isObject()){const auto o=value.toObject();for(auto i=o.begin();i!=o.end();++i){unicode(QJsonValue(i.key()));unicode(i.value());}}
}
std::vector<app::Account> accounts(app::ImportRepository& repo){auto a=repo.accounts();const auto b=repo.archived_accounts();a.insert(a.end(),b.begin(),b.end());return a;}
QJsonValue optional(const std::optional<std::string>& v){return v?QJsonValue(q(*v)):QJsonValue(QJsonValue::Null);}
std::optional<std::string> optional(const QJsonObject& o,const QString& key){if(!o.contains(key))invalid();if(o.value(key).isNull())return {};return s(string(o,key));}
std::vector<QString> strings(const QJsonArray& a){std::vector<QString> result;std::set<QString> seen;for(const auto& v:a){if(!v.isString() || !seen.insert(v.toString()).second)invalid();result.push_back(v.toString());}return result;}
std::set<int> selections(const QVariantMap& input,const char* key,qsizetype count){std::set<int> result;for(const auto& v:input.value(key).toList()){bool ok=false;const auto i=v.toInt(&ok);if(!ok || i<0 || i>=count || !result.insert(i).second)invalid();}return result;}
}
QByteArray encode(app::ImportRepository& repo,bool rules,bool reports,bool categories){
    if(!rules && !reports && !categories)throw std::invalid_argument("Select something to export.");
    QJsonArray rs,ps,cs,ar,cr;std::set<std::string> categoryRefs;std::set<std::int64_t> accountRefs;
    if(rules){if(!repo.rules_supported())throw std::invalid_argument("Rules are unavailable in this file.");for(const auto& r:repo.rule_image().ordered){
        if(r.actions.category)categoryRefs.insert(*r.actions.category);
        rs.append(QJsonObject{{"name",q(r.label)},{"enabled",r.enabled},{"scope",r.scope==rules::Scope::split?"split":"transaction"},{"input",r.input==rules::InputField::memo?"memo":r.input==rules::InputField::category?"category":"payee"},{"expression",q(r.expression)},{"payee",optional(r.actions.payee_template)},{"memo",optional(r.actions.memo_template)},{"category",optional(r.actions.category)}});
    }}
    if(reports)for(const auto& r:repo.report_catalog().definitions){
        QJsonArray aa,cc,pp;for(auto a:r.accounts){aa.append(QString::number(a));accountRefs.insert(a);}for(const auto& c:r.categories){cc.append(q(c));if(!c.empty())categoryRefs.insert(c);}for(const auto& p:r.periods)pp.append(QJsonObject{{"preset",q(p.preset)},{"start",q(p.start)},{"end",q(p.end)}});
        ps.append(QJsonObject{{"name",q(r.name)},{"graph",q(r.graph)},{"accounts",aa},{"categories",cc},{"periods",pp}});
    }
    const auto catalog=repo.category_catalog();
    for(const auto& c:catalog.categories)if(!c.deleted){
        if(categories){cs.append(QJsonObject{{"key",q(c.identity)},{"name",q(c.name)},{"income",c.income}});categoryRefs.insert(c.identity);}
        if(categoryRefs.contains(c.identity))cr.append(QJsonObject{{"key",q(c.identity)},{"name",q(c.name)}});
    }
    if(static_cast<std::size_t>(cr.size())!=categoryRefs.size())throw std::invalid_argument("An exported definition refers to a missing category. Edit it before exporting.");
    for(const auto& a:accounts(repo))if(accountRefs.contains(a.identity))ar.append(QJsonObject{{"key",QString::number(a.identity)},{"name",q(a.name)},{"currency",q(a.currency.definition_id())}});
    if(static_cast<std::size_t>(ar.size())!=accountRefs.size())throw std::invalid_argument("An exported report refers to a missing account. Edit it before exporting.");
    const auto bytes=QJsonDocument(QJsonObject{{"format","SpentIt"},{"version",1},{"rules",rs},{"reports",ps},{"categories",cs},{"categoryReferences",cr},{"accountReferences",ar}}).toJson();
    (void)decode(bytes);return bytes;
}
QJsonObject decode(const QByteArray& bytes){
    if(bytes.size()>16*1024*1024 || !qif::valid_utf8(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size()))))invalid();
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError || !doc.isObject())invalid();
    uniqueKeys(bytes);unicode(doc.object());
    const auto o=doc.object();keys(o,{"format","version","rules","reports","categories","categoryReferences","accountReferences"});
    if(string(o,"format")!="SpentIt" || o.value("version")!=QJsonValue(1))invalid();
    std::set<QString> cats,acs,defined;std::map<QString,QString> catNames;
    for(const auto& v:array(o,"categoryReferences")){const auto c=object(v);keys(c,{"key","name"});const auto key=string(c,"key"),name=string(c,"name");validate_category_name(s(name));if(key.isEmpty() || !cats.insert(key).second)invalid();catNames[key]=name;}
    for(const auto& v:array(o,"accountReferences",10000)){const auto a=object(v);keys(a,{"key","name","currency"});const auto key=string(a,"key");if(key.isEmpty() || string(a,"name").isEmpty() || !acs.insert(key).second)invalid();(void)Currency::from_definition(s(string(a,"currency")));}
    for(const auto& v:array(o,"categories")){const auto c=object(v);keys(c,{"key","name","income"});const auto key=string(c,"key");if(!cats.contains(key) || !defined.insert(key).second || catNames.at(key)!=string(c,"name"))invalid();(void)boolean(c,"income");}
    for(const auto& v:array(o,"rules")){const auto r=object(v);keys(r,{"name","enabled","scope","input","expression","payee","memo","category"});if(string(r,"name").size()>256)invalid();(void)boolean(r,"enabled");const auto scope=string(r,"scope"),input=string(r,"input");if(scope!="transaction" && scope!="split")invalid();if(input!="payee" && input!="memo" && input!="category")invalid();(void)string(r,"expression");(void)optional(r,"payee");(void)optional(r,"memo");const auto cat=optional(r,"category");if(!cat || !cats.contains(q(*cat)))invalid();}
    for(const auto& v:array(o,"reports",1000)){const auto r=object(v);keys(r,{"name","graph","accounts","categories","periods"});reports::Definition d;d.id="validation";d.name=s(string(r,"name"));d.graph=s(string(r,"graph"));for(const auto& a:strings(array(r,"accounts",10000)))if(!acs.contains(a))invalid();for(const auto& c:strings(array(r,"categories")))if(!c.isEmpty() && !cats.contains(c))invalid();for(const auto& p:array(r,"periods",32)){const auto period=object(p);keys(period,{"preset","start","end"});d.periods.push_back({s(string(period,"preset")),s(string(period,"start")),s(string(period,"end"))});}reports::validate(d);}
    return o;
}
QVariantMap preview(app::ImportRepository& repo,const QJsonObject& doc){
    QVariantMap result;for(const auto* section:{"rules","reports","categories"}){QVariantList rows;int index=0;for(const auto& v:doc.value(section).toArray()){const auto item=v.toObject();rows.append(QVariantMap{{"index",index++},{"name",item.value("name").toString()},{"checked",true},{"incomeChoice",0}});}result[section]=rows;}
    const auto catalog=repo.category_catalog();QVariantList categoryTargets{QVariantMap{{"id",""},{"name","Use matching or selected category"}}};
    for(const auto& c:catalog.categories)if(!c.deleted && !c.archived)categoryTargets.append(QVariantMap{{"id",q(c.identity)},{"name",q(c.name)}});
    result["categoryTargets"]=categoryTargets;
    QVariantList refs;for(const auto& v:doc.value("categoryReferences").toArray()){const auto c=v.toObject();QString match;for(const auto& existing:catalog.categories)if(!existing.deleted && !existing.archived && app::category_name_matches(existing.name,s(c.value("name").toString())))match=q(existing.identity);refs.append(QVariantMap{{"key",c.value("key").toString()},{"name",c.value("name").toString()},{"target",match},{"matched",!match.isEmpty()}});}result["categoryReferences"]=refs;
    auto rows=result["categories"].toList();const auto categories=doc.value("categories").toArray();for(int i=0;i<rows.size();++i){auto row=rows[i].toMap();const auto c=categories[i].toObject();for(const auto& existing:catalog.categories)if(!existing.deleted && app::category_name_matches(existing.name,s(c.value("name").toString()))){row["incomeConflict"]=existing.income!=c.value("income").toBool();row["existingIncome"]=existing.income;row["incomingIncome"]=c.value("income").toBool();}rows[i]=row;}result["categories"]=rows;
    QVariantList accountTargets{QVariantMap{{"id",""},{"name","Choose an account…"},{"currency",""}}};for(const auto& a:accounts(repo))accountTargets.append(QVariantMap{{"id",QString::number(a.identity)},{"name",q(a.name)+" ("+q(a.currency.code())+")"},{"currency",q(a.currency.definition_id())}});result["accountTargets"]=accountTargets;
    refs.clear();for(const auto& v:doc.value("accountReferences").toArray()){const auto a=v.toObject();QString match;int matches=0;for(const auto& existing:accounts(repo))if(q(existing.currency.definition_id())==a.value("currency").toString() && app::account_name_matches(existing.name,s(a.value("name").toString()))){match=QString::number(existing.identity);++matches;}if(matches!=1)match.clear();refs.append(QVariantMap{{"key",a.value("key").toString()},{"name",a.value("name").toString()},{"currency",a.value("currency").toString()},{"target",match}});}result["accountReferences"]=refs;
    return result;
}
app::DefinitionImport plan(app::ImportRepository& repo,const QJsonObject& doc,const QVariantMap& input,const app::ActionToken& token){
    if(repo.action_token()!=token)throw std::invalid_argument("The file changed. Open the import again.");
    const auto rs=doc.value("rules").toArray(),ps=doc.value("reports").toArray(),cs=doc.value("categories").toArray();
    const auto selectedRules=selections(input,"rules",rs.size()),selectedReports=selections(input,"reports",ps.size()),selectedCategories=selections(input,"categories",cs.size());
    if(selectedRules.empty() && selectedReports.empty() && selectedCategories.empty())throw std::invalid_argument("Select something to import.");
    app::DefinitionImport out;out.token=token;const auto catalog=repo.category_catalog();
    std::map<QString,std::string> resolved;std::map<QString,QString> names;
    for(const auto& v:doc.value("categoryReferences").toArray()){const auto c=v.toObject();names[c.value("key").toString()]=c.value("name").toString();}
    const auto choices=input.value("incomeChoices").toMap();
    for(auto i:selectedCategories){const auto c=cs[i].toObject();const auto name=s(c.value("name").toString());const auto key=c.value("key").toString();const auto income=c.value("income").toBool();
        const auto existing=std::find_if(catalog.categories.begin(),catalog.categories.end(),[&](const auto& e){return !e.deleted && app::category_name_matches(e.name,name);});
        if(existing!=catalog.categories.end()){
            if(existing->archived)throw std::invalid_argument("A selected category is archived. Unarchive it before importing that category.");
            resolved[key]=existing->identity;
            if(existing->income!=income){const int choice=choices.value(QString::number(i)).toInt();if(choice!=1 && choice!=2)throw std::invalid_argument("Choose which Income setting to keep for each conflicting category.");if(choice==2){app::CategoryRequest change{app::CategoryAction::set_income,existing->identity,"",""};change.income=income;out.categories.push_back(change);}}
        }else{const auto identity=id();resolved[key]=identity;app::CategoryRequest create{app::CategoryAction::create,identity,name,""};out.categories.push_back(create);if(income){app::CategoryRequest flag{app::CategoryAction::set_income,identity,"",""};flag.income=true;out.categories.push_back(flag);}}
    }
    const auto mappings=input.value("categoryMappings").toMap();
    const auto category=[&](const QString& key)->std::string{
        if(key.isEmpty())return {};
        const auto explicitId=mappings.value(key).toString();
        if(!explicitId.isEmpty()){for(const auto& c:catalog.categories)if(q(c.identity)==explicitId && !c.deleted && !c.archived)return c.identity;throw std::invalid_argument("Choose an available category.");}
        if(resolved.contains(key))return resolved.at(key);
        for(const auto& c:catalog.categories)if(!c.deleted && !c.archived && app::category_name_matches(c.name,s(names.at(key))))return c.identity;
        throw std::invalid_argument("Select or map the missing category: "+s(names.at(key)));
    };
    if(!selectedRules.empty()){
        if(!repo.rules_supported())throw std::invalid_argument("Rules are unavailable in this file. Deselect them to import the other items.");
        const auto current=repo.rule_image();out.rules_revision=current.revision;out.rules=app::RuleSaveRequest{current.ordered};
        for(auto i:selectedRules){const auto r=rs[i].toObject();app::RuleDefinitionImage d;d.identity=id();d.label=s(r.value("name").toString());d.enabled=r.value("enabled").toBool();d.scope=r.value("scope").toString()=="split"?rules::Scope::split:rules::Scope::transaction;const auto inputField=r.value("input").toString();d.input=inputField=="memo"?rules::InputField::memo:inputField=="category"?rules::InputField::category:rules::InputField::payee;d.expression=s(r.value("expression").toString());d.actions.payee_template=optional(r,"payee");d.actions.memo_template=optional(r,"memo");d.actions.category=category(r.value("category").toString());out.rules->ordered.push_back(std::move(d));}
    }
    if(!selectedReports.empty()){
        out.reports=repo.report_catalog();const auto available=accounts(repo);const auto mappingsAccounts=input.value("accountMappings").toMap();std::map<QString,QJsonObject> refs;for(const auto& v:doc.value("accountReferences").toArray())refs[v.toObject().value("key").toString()]=v.toObject();
        for(auto i:selectedReports){const auto r=ps[i].toObject();reports::Definition d;d.id=id();d.name=s(r.value("name").toString());d.graph=s(r.value("graph").toString());
            for(const auto& v:r.value("categories").toArray())d.categories.push_back(category(v.toString()));
            for(const auto& v:r.value("accounts").toArray()){
                const auto ref=refs.at(v.toString());const auto explicitId=mappingsAccounts.value(v.toString()).toString();std::vector<std::int64_t> matches;
                for(const auto& a:available)if(q(a.currency.definition_id())==ref.value("currency").toString() && (explicitId.isEmpty()?app::account_name_matches(a.name,s(ref.value("name").toString())):QString::number(a.identity)==explicitId))matches.push_back(a.identity);
                if(matches.size()!=1)throw std::invalid_argument("Choose an account with the matching currency for: "+s(ref.value("name").toString()));d.accounts.push_back(matches.front());
            }
            for(const auto& v:r.value("periods").toArray()){const auto p=v.toObject();d.periods.push_back({s(p.value("preset").toString()),s(p.value("start").toString()),s(p.value("end").toString())});}
            reports::validate(d);out.reports->definitions.push_back(std::move(d));
        }
    }
    // All selected categories may already match; keep this a successful no-op.
    return out;
}
void publish(const QString& path,const QByteArray& bytes,const QString& activePath,bool qif,const app::OperationContext& context){
    const QFileInfo target(path),active(activePath);
    if(path.isEmpty() || target.isSymLink() || target.isDir())throw std::invalid_argument("Choose a regular export file.");
    const auto resolved=target.absolutePath()+"/"+target.fileName();const auto live=active.canonicalFilePath();
    if(target.canonicalFilePath()==live || resolved==active.absoluteFilePath() || resolved.startsWith(active.absoluteFilePath()+"-"))throw std::invalid_argument("Choose a destination other than the open SpentIt file or its working files.");
    if(qif?!path.endsWith(".qif",Qt::CaseInsensitive):!path.endsWith(".spentit.json",Qt::CaseInsensitive))throw std::invalid_argument(qif?"Use a .qif filename.":"Use a .spentit.json filename.");
    if(target.exists()){
        QFile old(path);if(!old.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read the existing export file.");const auto header=old.read(64);
        if(qif){if(!header.startsWith("!Account\n") && !header.startsWith("!Type:Cat\n"))throw std::invalid_argument("Choose a new filename; the existing file is not a SpentIt QIF export.");}
        else {if(old.size()>16*1024*1024)invalid();old.seek(0);(void)decode(old.readAll());}
    }
    context.checkpoint();QSaveFile output(path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot create the export file.");
    if(!output.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || output.write(bytes)!=bytes.size())throw std::runtime_error("The export could not be written. Check free space and destination permissions.");
    context.seal();if(!output.commit())throw std::runtime_error("The export could not be saved. The previous destination was retained.");
}
}
