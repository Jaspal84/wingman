#pragma once
#include "Supabase.h"
#include "Studio/Library/LibraryPanel.h"
namespace wingman::studio {
class CloudPanel final : public juce::Component {
    cloud::Config config = cloud::Config::environment();
    std::unique_ptr<cloud::Supabase> client;
    juce::File db;
    juce::TextEditor email, password, status;
    juce::TextButton signIn{"Sign in"}, refresh{"Refresh session"}, signOut{"Sign out"}, push{"Push local metadata"}, pull{"Use remote metadata"}, upload{"Upload selected audio"};
    juce::ToggleButton consent{"I own or have upload rights to this audio"};
    BackgroundThread worker;
    bool busy = false;
    void enabled() {
        bool connected = client && !client->currentSession().user.isEmpty(); signIn.setEnabled(!busy && config.configured());
        for (auto* b : {&refresh, &signOut, &push, &pull, &upload}) b->setEnabled(!busy && connected);
        email.setEnabled(!busy); password.setEnabled(!busy);
    }
    void run(std::function<juce::String()> operation) {
        if (busy) return; busy = true; enabled(); status.setText("Cloud operation in progress. Local editing remains available.", false);
        worker.start([safe = juce::Component::SafePointer<CloudPanel>(this), operation](CancellationToken) {
            juce::String text; try { text = operation(); } catch (const std::exception& e) { text = juce::String::fromUTF8(e.what()); }
            juce::MessageManager::callAsync([safe, text] { if (safe) { safe->busy = false; safe->enabled(); safe->status.setText(text, false); } });
        });
    }
    cloud::SyncStore store() const { return cloud::SyncStore(dataDirectory().getChildFile("sync").getChildFile(client->currentSession().user + ".json")); }
public:
    std::function<std::optional<Sample>()> selectedSample;
    explicit CloudPanel(juce::File libraryFile) : db(std::move(libraryFile)) {
        for (auto* c : std::initializer_list<juce::Component*>{&email, &password, &status, &signIn, &refresh, &signOut, &push, &pull, &upload, &consent}) addAndMakeVisible(c);
        email.setTextToShowWhenEmpty("Supabase account email", juce::Colours::grey); password.setPasswordCharacter('*'); password.setTextToShowWhenEmpty("Password (memory only)", juce::Colours::grey); status.setMultiLine(true); status.setReadOnly(true);
        client = std::make_unique<cloud::Supabase>(config, cloud::httpTransport(config));
        signIn.onClick = [this] { auto user = email.getText(), pass = password.getText(); password.clear(); run([this, user, pass] { client->signIn(user, pass); return juce::String("Signed in. Tokens remain in memory; sign in again after restart. Push explicitly sends local tags/favorites/collection metadata. Use remote explicitly replaces local annotations for matching content hashes."); }); };
        refresh.onClick = [this] { run([this] { client->refresh(); return juce::String("Session refreshed"); }); };
        signOut.onClick = [this] { run([this] { client->signOut(); return juce::String("Signed out; local audio and library are retained"); }); };
        push.onClick = [this] { run([this] {
            Library library(db); auto outbox = store(); Query query; query.limit = 100; int count = 0;
            do { query.offset = count; auto rows = library.search(query); if (rows.empty()) break;
                for (const auto& s : rows) { outbox.queue(s.hash, cloud::object({{"instrument", s.instrument}, {"genre", s.genre}, {"mood", s.mood}, {"collection", s.collection}, {"favorite", s.favorite}, {"bpm", s.bpm}, {"key", s.key}, {"mode", s.mode}})); ++count; }
            } while (count < 100000);
            outbox.synchronize(*client); return juce::String("Metadata synchronized. Audio was not uploaded. Revision conflicts retain the outbox; Use remote is the explicit remote-wins resolution.");
        }); };
        pull.onClick = [this] { run([this] {
            auto outbox = store(); outbox.discardPending(); outbox.synchronize(*client); Library library(db); Query q; q.limit = 100; int offset = 0, applied = 0;
            while (offset < library.count()) { q.offset = offset; auto samples = library.search(q); offset += static_cast<int>(samples.size()); if (samples.empty()) break;
                for (const auto& s : samples) for (const auto& r : outbox.entries()) if (r.hash == s.hash && !r.deleted) { auto m = r.metadata; library.annotate(s.id, m["instrument"].toString(), m["genre"].toString(), m["mood"].toString(), m["collection"].toString(), m["favorite"], m["bpm"], m["key"], m["mode"]); ++applied; break; }
            }
            return juce::String(applied) + " local annotations updated from the account. Source files were not changed. Reopen Library + Match to refresh search results.";
        }); };
        upload.onClick = [this] { auto sample = selectedSample ? selectedSample() : std::nullopt; if (!sample || !consent.getToggleState()) { status.setText("Select a source in Library and explicitly confirm upload rights first.", false); return; } auto chosen = *sample; run([this, chosen] { client->upload(chosen.hash, juce::File(chosen.path), true); return juce::String("Uploaded licensed audio to private owner-scoped storage. No purchase or commercial catalogue operation was performed."); }); };
        status.setText(config.configured() ? "Supabase configured. Sign in to the account you created in your project. Nothing is uploaded automatically. See Documentation/SUPABASE.md before deploying the schema." : "OFFLINE MODE\n\nSupabase is not configured. The entire local library/editor/generation/instrument workflow remains available.\n\nCreate your own Supabase project and apply Supabase/migrations/0001_library_sync.sql. Configure SUPABASE_URL and SUPABASE_PUBLISHABLE_KEY securely in environment settings, then restart the application. Never use a service-role key in a desktop client.\n\nNo remote service has been contacted. See Documentation/SUPABASE.md for setup, privacy and validation requirements.", false); enabled();
    }
    ~CloudPanel() override { worker.request_stop(); worker.join(); }
    void resized() override {
        email.setBounds(20, 70, 330, 30); password.setBounds(365, 70, 300, 30); signIn.setBounds(680, 70, 120, 30); refresh.setBounds(20, 115, 170, 30); signOut.setBounds(205, 115, 120, 30);
        push.setBounds(20, 180, 220, 32); pull.setBounds(255, 180, 220, 32); upload.setBounds(490, 180, 200, 32); consent.setBounds(20, 225, getWidth() - 40, 30);
        status.setBounds(20, 280, getWidth() - 40, std::max(100, getHeight() - 300));
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff151d29)); g.setColour(juce::Colours::white); g.setFont(20); g.drawText("OPTIONAL SUPABASE / account metadata + private licensed audio storage", 20, 15, getWidth() - 40, 35, juce::Justification::left); }
};
}
