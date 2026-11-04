#include <spentit/diagnostics.hpp>
#include <spentit/storage_document_gateway.hpp>
#include <algorithm>
namespace spentit::storage {
namespace {
std::string password_key(std::string_view password) {
    if(password.empty() || password.size()>1000 || !qif::valid_utf8(password) || password.find('\0')!=password.npos)
        throw std::invalid_argument("Password must contain 1–1000 UTF-8 bytes without NUL");
    // Domain separation prevents user input from being interpreted as a raw key.
    return "SpentIt/password/v1:" + std::string(password);
}
std::filesystem::path document_path(const std::filesystem::path& path) {
    // Resolve platform aliases in parent directories, but never follow a document symlink.
    std::error_code error;
    const auto absolute=std::filesystem::absolute(path,error);
    if(error) throw std::runtime_error("Cannot resolve dataset location");
    const auto parent=std::filesystem::canonical(absolute.parent_path(),error);
    if(error) throw std::runtime_error("Dataset file not found or inaccessible");
    return parent/absolute.filename();
}
class Gateway final : public app::DocumentGateway {
    std::unique_ptr<ImportStore> store_;
    std::string key_;
    std::filesystem::path cache_;
public:
    explicit Gateway(std::filesystem::path cache):cache_(std::move(cache)) {}
    bool lazy_review() const override {return !cache_.empty();}
    void close_verified(const app::OperationContext& context) override {
        if(store_)store_->certify(cache_,&context);
        close();
    }
    ~Gateway() override { close(); }
    void close() override { store_.reset(); std::fill(key_.begin(),key_.end(),'\0'); key_.clear(); }
    void create(const std::filesystem::path& p,bool protected_mode,std::string_view password,
                std::string_view name,const Currency& currency,std::string_view today) override {
        create_selected(p,false,protected_mode,password,name,currency,today);
    }
    void create_selected(const std::filesystem::path& p,bool overwrite,bool protected_mode,std::string_view password,
                         std::string_view name,const Currency& currency,std::string_view today) override {
        create_configured(p,overwrite,protected_mode,password,name,currency,today,{});
    }
    void create_configured(const std::filesystem::path& p,bool overwrite,bool protected_mode,std::string_view password,
                          std::string_view name,const Currency& currency,std::string_view today,const app::DatasetOptions& options) override {
        if(store_) throw std::logic_error("Close the current dataset first");
        auto key=protected_mode?password_key(password):std::string(public_document_key);
        auto created=ImportStore::create(document_path(p),key,name,currency,today,overwrite,options);
        store_=std::make_unique<ImportStore>(std::move(created)); key_=std::move(key);
    }
    void open(const std::filesystem::path& p,std::optional<std::string> password) override {
        app::OperationContext context; open_controlled(p,std::move(password),context);
    }
    void open_controlled(const std::filesystem::path& p,std::optional<std::string> password,const app::OperationContext& context) override {
        diagnostics::Span span(diagnostics::Code::gateway);
        context.checkpoint("Opening file");
        if(store_) throw std::logic_error("Close the current dataset first");
        auto key=password?password_key(*password):std::string(public_document_key);
        try {
            auto opened=ImportStore::open_cached(document_path(p),key,cache_,&context);
            context.checkpoint();
            store_=std::make_unique<ImportStore>(std::move(opened)); key_=std::move(key);
        } catch(const ImportAuthenticationError&) {
            throw app::UnlockRequired();
        }
    }
    app::ImportRepository& repository() override {
        if(!store_) throw std::logic_error("No dataset is open");
        return *store_;
    }
    void backup_controlled(const std::filesystem::path& p,const app::OperationContext& context) override {
        diagnostics::Span span(diagnostics::Code::gateway);
        if(!store_) throw std::logic_error("No dataset is open");
        try { store_->backup_to(document_path(p),key_,{},&context); }
        catch(const app::OperationCancelled&) {
            throw app::OperationCancelled("Backup cancelled. An unfinished .partial file may remain. Use a new filename when you try again.");
        }
    }
    void backup(const std::filesystem::path& p) override {
        if(!store_) throw std::logic_error("No dataset is open");
        store_->backup_to(document_path(p),key_);
    }
};
}
std::unique_ptr<app::DocumentGateway> document_gateway(const std::filesystem::path& cache) { return std::make_unique<Gateway>(cache); }
}
