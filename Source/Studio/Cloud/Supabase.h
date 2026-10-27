#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <functional>
#include <stdexcept>
namespace wingman::studio::cloud {
struct Request { juce::String method, path; juce::StringPairArray headers; juce::var json; juce::MemoryBlock bytes; juce::String contentType; };
struct Response { int status = 0; juce::var json; juce::MemoryBlock bytes; };
using Transport = std::function<Response(const Request&)>;
inline juce::var object(std::initializer_list<std::pair<juce::Identifier, juce::var>> fields) {
    auto* o = new juce::DynamicObject; for (const auto& p : fields) o->setProperty(p.first, p.second); return o;
}
struct Session { juce::String user, access, refresh; int64_t expires = 0; };
struct Config {
    juce::String url, publishableKey;
    static Config environment() { return {juce::SystemStats::getEnvironmentVariable("SUPABASE_URL", {}), juce::SystemStats::getEnvironmentVariable("SUPABASE_PUBLISHABLE_KEY", {})}; }
    bool configured() const { auto parsed = juce::URL(url); return url.startsWith("https://") && !publishableKey.isEmpty() && !publishableKey.containsAnyOf("\r\n") && !url.containsAnyOf("@?#\r\n") && parsed.getDomain().isNotEmpty() && parsed.getSubPath().trimCharactersAtEnd("/").isEmpty(); }
};
// A transport is injected so offline builds/tests never need cloud credentials.
// Session tokens live in memory only; this class never logs response bodies.
class Supabase {
    Config config;
    Transport transport;
    Session session;
    Response send(Request request, bool authenticated = true) {
        if (!config.configured()) throw std::runtime_error("Supabase is not configured; offline operation remains available");
        if (authenticated && (session.access.isEmpty() || session.expires <= juce::Time::currentTimeMillis() / 1000)) throw std::runtime_error("Cloud session absent or expired; sign in or refresh");
        request.headers.set("apikey", config.publishableKey);
        if (authenticated) request.headers.set("Authorization", "Bearer " + session.access);
        auto result = transport(request);
        if (result.status == 409 || (result.json["code"].toString() == "40001")) throw std::runtime_error("Sync revision conflict; refresh and resolve explicitly");
        if (result.status < 200 || result.status >= 300) throw std::runtime_error(("Supabase request failed (HTTP " + juce::String(result.status) + ")").toStdString());
        return result;
    }
    void acceptSession(const juce::var& json) {
        Session next; next.user = json["user"]["id"].toString(); next.access = json["access_token"].toString(); next.refresh = json["refresh_token"].toString();
        next.expires = juce::Time::currentTimeMillis() / 1000 + static_cast<int>(json["expires_in"]);
        if (next.user.length() != 36 || !next.user.removeCharacters("-").containsOnly("0123456789abcdefABCDEF") || next.access.containsAnyOf("\r\n") || next.access.isEmpty() || next.refresh.isEmpty() || next.expires <= juce::Time::currentTimeMillis() / 1000) throw std::runtime_error("Invalid authentication response");
        session = std::move(next);
    }
    static bool hashValid(const juce::String& h) { return h.length() == 64 && h.containsOnly("0123456789abcdef"); }
public:
    Supabase(Config c, Transport t) : config(std::move(c)), transport(std::move(t)) { if (!transport) throw std::runtime_error("Cloud transport required"); }
    const Session& currentSession() const { return session; }
    bool configured() const { return config.configured(); }
    void signIn(const juce::String& email, const juce::String& password) {
        Request r; r.method = "POST"; r.path = "/auth/v1/token?grant_type=password"; r.json = object({{"email", email}, {"password", password}}); acceptSession(send(r, false).json);
    }
    void refresh() { if (session.refresh.isEmpty()) throw std::runtime_error("No refresh session"); Request r; r.method = "POST"; r.path = "/auth/v1/token?grant_type=refresh_token"; r.json = object({{"refresh_token", session.refresh}}); acceptSession(send(r, false).json); }
    void signOut() { if (!session.access.isEmpty()) { Request r; r.method = "POST"; r.path = "/auth/v1/logout"; try { send(r); } catch (...) { session = {}; throw; } } session = {}; }
    juce::var push(const juce::String& hash, const juce::var& metadata, int64_t expectedRevision, bool deleted = false) {
        if (!hashValid(hash) || !metadata.isObject() || expectedRevision < 0 || juce::JSON::toString(metadata).getNumBytesAsUTF8() > 16384) throw std::runtime_error("Invalid sample sync record");
        Request r; r.method = "POST"; r.path = "/rest/v1/rpc/sync_sample"; r.json = object({{"p_hash", hash}, {"p_metadata", metadata}, {"p_revision", static_cast<juce::int64>(expectedRevision)}, {"p_deleted", deleted}}); return send(r).json;
    }
    juce::var pullPage(int offset = 0) {
        if (offset < 0) throw std::runtime_error("Invalid sync page");
        Request r; r.method = "GET"; r.path = "/rest/v1/sample_metadata?select=*&order=content_hash.asc&limit=100&offset=" + juce::String(offset); return send(r).json;
    }
    void upload(const juce::String& hash, const juce::File& source, bool consent) {
        if (!consent) throw std::runtime_error("Explicit licensed-audio upload consent required");
        if (!hashValid(hash) || !source.existsAsFile() || source.getSize() > 256 * 1024 * 1024 || !source.hasFileExtension("wav;aif;aiff;flac")) throw std::runtime_error("Invalid upload file/hash/size");
        Request r; r.method = "POST"; r.path = "/storage/v1/object/jaspro-private/" + session.user + "/" + hash + "/" + juce::URL::addEscapeChars(source.getFileName(), false);
        if (!source.loadFileAsData(r.bytes)) throw std::runtime_error("Cannot read upload");
        if (juce::SHA256(r.bytes).toHexString() != hash) throw std::runtime_error("Source hash changed before upload");
        r.contentType = source.hasFileExtension("flac") ? "audio/flac" : source.hasFileExtension("aif;aiff") ? "audio/aiff" : "audio/wav"; send(r);
    }
    juce::MemoryBlock download(const juce::String& hash, const juce::String& filename) {
        if (!hashValid(hash) || filename.isEmpty() || filename.containsAnyOf("/\\") || filename == "." || filename == "..") throw std::runtime_error("Invalid storage path");
        Request r; r.method = "GET"; r.path = "/storage/v1/object/authenticated/jaspro-private/" + session.user + "/" + hash + "/" + juce::URL::addEscapeChars(filename, false); auto bytes = send(r).bytes; if (juce::SHA256(bytes).toHexString() != hash) throw std::runtime_error("Downloaded audio checksum mismatch"); return bytes;
    }
};
inline Transport httpTransport(Config config) {
    return [config](const Request& request) {
        if (!config.configured()) throw std::runtime_error("Supabase is not configured");
        auto base = config.url.trimCharactersAtEnd("/"); juce::URL url(base + request.path);
        juce::String headers; for (int i = 0; i < request.headers.size(); ++i) headers += request.headers.getAllKeys()[i] + ": " + request.headers.getAllValues()[i] + "\r\n";
        if (request.json.isObject() || request.json.isArray()) { url = url.withPOSTData(juce::JSON::toString(request.json)); headers += "Content-Type: application/json\r\n"; }
        else if (request.bytes.getSize()) { url = url.withPOSTData(request.bytes); headers += "Content-Type: " + request.contentType + "\r\n"; }
        Response response; auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(10000).withHttpRequestCmd(request.method).withExtraHeaders(headers).withStatusCode(&response.status).withNumRedirectsToFollow(0);
        auto stream = url.createInputStream(options); if (!stream) throw std::runtime_error("Cloud connection failed; local operation is unaffected");
        char block[65536]; while (!stream->isExhausted()) { int n = stream->read(block, sizeof(block)); if (n <= 0) break; if (response.bytes.getSize() + static_cast<std::size_t>(n) > 256 * 1024 * 1024) throw std::runtime_error("Cloud response exceeded size limit"); response.bytes.append(block, static_cast<std::size_t>(n)); }
        if (!request.path.startsWith("/storage/") || response.status >= 300) {
            if (response.bytes.getSize() > 4 * 1024 * 1024) throw std::runtime_error("Oversized cloud metadata response");
            response.json = juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(response.bytes.getData()), static_cast<int>(response.bytes.getSize())));
        }
        return response;
    };
}
// Persistent local outbox/remote revision cache. Pending writes survive offline
// restarts. Conflicts are retained rather than silently overwriting other devices.
struct SyncRecord { juce::String hash; juce::var metadata; int64_t revision = 0; bool deleted = false, pending = false; };
class SyncStore {
    juce::File file;
    std::vector<SyncRecord> records;
    juce::String owner;
public:
    explicit SyncStore(juce::File f) : file(std::move(f)) {
        if (!file.existsAsFile()) return;
        if (file.getSize() > 16 * 1024 * 1024) throw std::runtime_error("Oversized sync outbox");
        auto data = juce::JSON::parse(file.loadFileAsString()); auto* array = data["records"].getArray();
        if (static_cast<int>(data["version"]) != 1 || !array) throw std::runtime_error("Invalid sync outbox");
        owner = data["owner"].toString();
        for (const auto& r : *array) { SyncRecord item{r["hash"].toString(), r["metadata"], static_cast<int64_t>(static_cast<juce::int64>(r["revision"])), static_cast<bool>(r["deleted"]), static_cast<bool>(r["pending"])}; if (item.hash.length() != 64 || !item.hash.containsOnly("0123456789abcdef") || item.revision < 0 || !item.metadata.isObject()) throw std::runtime_error("Invalid outbox record"); records.push_back(item); }
    }
    const std::vector<SyncRecord>& entries() const { return records; }
    void save() {
        juce::Array<juce::var> rows; for (const auto& r : records) rows.add(object({{"hash", r.hash}, {"metadata", r.metadata}, {"revision", static_cast<juce::int64>(r.revision)}, {"deleted", r.deleted}, {"pending", r.pending}}));
        if (file.getParentDirectory().createDirectory().failed()) throw std::runtime_error("Cannot create outbox directory"); juce::TemporaryFile temp(file);
        if (!temp.getFile().replaceWithText(juce::JSON::toString(object({{"version", 1}, {"owner", owner}, {"records", rows}}))) || !temp.overwriteTargetFileWithTemporary()) throw std::runtime_error("Cannot save outbox");
    }
    void queue(const juce::String& hash, juce::var metadata, bool deleted = false) {
        if (hash.length() != 64 || !hash.containsOnly("0123456789abcdef") || !metadata.isObject()) throw std::runtime_error("Invalid offline record");
        for (auto& r : records) if (r.hash == hash) { r.metadata = metadata; r.deleted = deleted; r.pending = true; save(); return; }
        records.push_back({hash, metadata, 0, deleted, true}); save();
    }
    void discardPending() { records.clear(); save(); }
    void synchronize(Supabase& remote) {
        auto user = remote.currentSession().user;
        if (user.isEmpty()) throw std::runtime_error("Sign in before synchronizing");
        if (owner.isNotEmpty() && owner != user) throw std::runtime_error("Outbox belongs to another account; use a separate account store");
        if (owner.isEmpty()) { owner = user; save(); }
        for (auto& r : records) if (r.pending) { auto ack = remote.push(r.hash, r.metadata, r.revision, r.deleted); auto revision = static_cast<int64_t>(static_cast<juce::int64>(ack["revision"])); if (revision <= r.revision || ack["content_hash"].toString() != r.hash) throw std::runtime_error("Invalid sync acknowledgement"); r.revision = revision; r.pending = false; save(); }
        for (int page = 0; page < 10000; ++page) {
            auto response = remote.pullPage(page * 100); auto* rows = response.getArray(); if (!rows) throw std::runtime_error("Invalid cloud catalogue response");
            for (const auto& item : *rows) { auto hash = item["content_hash"].toString(); auto revision = static_cast<int64_t>(static_cast<juce::int64>(item["revision"])); if (hash.length() != 64 || !hash.containsOnly("0123456789abcdef") || revision < 1 || !item["metadata"].isObject()) throw std::runtime_error("Invalid remote record"); bool found = false; for (auto& r : records) if (r.hash == hash) { found = true; if (!r.pending && revision >= r.revision) { r.revision = revision; r.metadata = item["metadata"]; r.deleted = item["deleted"]; } break; } if (!found) records.push_back({hash, item["metadata"], revision, static_cast<bool>(item["deleted"]), false}); }
            save(); if (rows->size() < 100) return;
        }
        throw std::runtime_error("Sync exceeded page safety limit");
    }
};
// Future authorized commercial catalogues implement this interface. A provider
// must enforce its own entitlement/download terms; no built-in commercial URL.
struct ProviderItem { juce::String id, title, license; };
class SampleProvider {
public:
    virtual ~SampleProvider() = default;
    virtual std::vector<ProviderItem> search(const juce::String&) = 0;
    virtual juce::Result downloadLicensed(const juce::String& item, const juce::File& destination) = 0;
};
}
