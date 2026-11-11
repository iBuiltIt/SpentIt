#include <spentit/diagnostics.hpp>
#include <spentit/storage_document_gateway.hpp>
#include <openssl/crypto.h>
#include <algorithm>
#include <mutex>
namespace spentit::storage {
namespace {
struct Secret {
    std::string text;
    explicit Secret(std::string value):text(std::move(value)){}
    Secret(const Secret&)=delete;
    Secret& operator=(const Secret&)=delete;
    Secret(Secret&& other) noexcept:text(std::move(other.text)){std::fill(other.text.begin(),other.text.end(),'\0');}
    ~Secret(){std::fill(text.begin(),text.end(),'\0');}
};
bool same_secret(std::string_view left,std::string_view right) {
    if(left.size()!=right.size())return false;
    return CRYPTO_memcmp(left.data(),right.data(),left.size())==0;
}
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
    mutable std::mutex key_mu_;
    std::filesystem::path cache_;
    void adopt(std::string key) {
        std::lock_guard lock(key_mu_);
        std::fill(key_.begin(),key_.end(),'\0');
        key_=std::move(key);
    }
    std::string copy_key() const {
        std::lock_guard lock(key_mu_);
        return key_;
    }
public:
    explicit Gateway(std::filesystem::path cache):cache_(std::move(cache)) {}
    bool lazy_review() const override {return !cache_.empty();}
    void close_verified(const app::OperationContext& context) override {
        if(store_)store_->certify(cache_,&context);
        close();
    }
    ~Gateway() override { close(); }
    void close() override {
        {std::lock_guard lock(key_mu_);std::fill(key_.begin(),key_.end(),'\0');key_.clear();}
        store_.reset();
    }
    bool password_protected() const override {
        std::lock_guard lock(key_mu_);
        return !key_.empty() && key_!=public_document_key;
    }
    bool password_matches(std::string_view password) const override {
        Secret candidate{std::string{}};
        try {candidate.text=password_key(password);}
        catch(const std::exception&) {return false;}
        std::lock_guard lock(key_mu_);
        return same_secret(candidate.text,key_);
    }
    app::PasswordUpdate change_password(std::string_view current,std::string_view next,bool remove) override {
        if(!store_)throw std::logic_error("No dataset is open");
        Secret old{copy_key()};
        const bool protected_mode=!old.text.empty() && old.text!=public_document_key;
        if(protected_mode) {
            Secret typed{std::string{}};
            try {typed.text=password_key(current);}
            catch(const std::invalid_argument&) {throw std::invalid_argument("Current password does not match.");}
            if(!same_secret(typed.text,old.text))throw std::invalid_argument("Current password does not match.");
        }
        if(remove) {
            if(!protected_mode)throw std::invalid_argument("This file has no password.");
            if(!next.empty())throw std::invalid_argument("Enter a password, or leave both new passwords empty to remove protection.");
            Secret public_key{std::string(public_document_key)};
            const auto result=store_->rekey(public_key.text);
            adopt(public_key.text);
            if(result==ImportStore::RekeyResult::committed_unconfirmed)
                throw std::runtime_error("The password was changed, but SpentIt could not confirm the file. Close it and open it with the new password before continuing.");
            return app::PasswordUpdate::removed;
        }
        Secret next_key{password_key(next)};
        if(same_secret(next_key.text,old.text))return app::PasswordUpdate::unchanged;
        const auto result=store_->rekey(next_key.text);
        adopt(next_key.text);
        if(result==ImportStore::RekeyResult::committed_unconfirmed)
            throw std::runtime_error("The password was changed, but SpentIt could not confirm the file. Close it and open it with the new password before continuing.");
        return app::PasswordUpdate::changed;
    }
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
        Secret key{protected_mode?password_key(password):std::string(public_document_key)};
        auto created=ImportStore::create(document_path(p),key.text,name,currency,today,overwrite,options);
        store_=std::make_unique<ImportStore>(std::move(created)); adopt(key.text);
    }
    void open(const std::filesystem::path& p,std::optional<std::string> password) override {
        app::OperationContext context; open_controlled(p,std::move(password),context);
    }
    void open_controlled(const std::filesystem::path& p,std::optional<std::string> password,const app::OperationContext& context) override {
        diagnostics::Span span(diagnostics::Code::gateway);
        context.checkpoint("Opening file");
        if(store_) throw std::logic_error("Close the current dataset first");
        Secret key{password?password_key(*password):std::string(public_document_key)};
        try {
            auto opened=ImportStore::open_cached(document_path(p),key.text,cache_,&context);
            context.checkpoint();
            store_=std::make_unique<ImportStore>(std::move(opened)); adopt(key.text);
        } catch(const ImportAuthenticationError&) {
            throw app::UnlockRequired();
        }
    }
    void probe(const std::filesystem::path& path,const app::OperationContext& context) override {
        if(store_)throw std::logic_error("Close the current dataset first");
        try {ImportStore::probe(document_path(path),public_document_key,&context);}
        catch(const ImportAuthenticationError&){throw app::UnlockRequired();}
    }
    app::ImportRepository& repository() override {
        if(!store_) throw std::logic_error("No dataset is open");
        return *store_;
    }
    void check_file(const app::OperationContext& context) override {
        if(!store_)throw std::logic_error("No dataset is open");
        store_->verify(&context);
    }
    void backup_controlled(const std::filesystem::path& p,const app::OperationContext& context) override {
        diagnostics::Span span(diagnostics::Code::gateway);
        if(!store_) throw std::logic_error("No dataset is open");
        Secret key{copy_key()};
        try { store_->backup_to(document_path(p),key.text,{},&context); }
        catch(const app::OperationCancelled&) {
            throw app::OperationCancelled("Backup cancelled. An unfinished .partial file may remain. Use a new filename when you try again.");
        }
    }
    void backup(const std::filesystem::path& p) override {
        if(!store_) throw std::logic_error("No dataset is open");
        Secret key{copy_key()};store_->backup_to(document_path(p),key.text);
    }
};
}
std::unique_ptr<app::DocumentGateway> document_gateway(const std::filesystem::path& cache) { return std::make_unique<Gateway>(cache); }
}
