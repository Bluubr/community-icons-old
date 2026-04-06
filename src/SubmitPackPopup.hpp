#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <chrono>
#include <algorithm>
#include <cctype>

using namespace geode::prelude;

// Popup that lets any logged-in player submit an icon pack for moderator review.
//
// Two anti-abuse layers are applied before the Firestore POST:
//   1. Banned-word filter  — pack name and author are rejected if they contain any
//      word from the bannedWords() list (case-insensitive substring match).
//      Extend that list in the source to cover slurs / additional profanity.
//
//   2. Submission cooldown — at most one submission per GD account per hour.
//      The timestamp of the last successful submit is stored in Geode's per-mod
//      save data (key: "last_pack_submit_epoch").
//
// Every accepted submission writes a `submittedBy` field (GD Account ID as a
// string) to the Firestore document so moderators can track and ban repeat
// abusers from the Firebase Console.
class SubmitPackPopup : public geode::Popup {
protected:
    TextInput*     m_nameInput     = nullptr;
    TextInput*     m_authorInput   = nullptr;
    TextInput*     m_gamemodeInput = nullptr;
    TextInput*     m_imageUrlInput = nullptr;
    TextInput*     m_plistUrlInput = nullptr;
    CCLabelBMFont* m_statusLabel   = nullptr;

    std::optional<arc::TaskHandle<void>> m_submitHandle;

    // One hour between submissions from the same device / account.
    static constexpr int64_t     COOLDOWN_SECS = 3600;
    static constexpr const char* COOLDOWN_KEY  = "last_pack_submit_epoch";

    // ── Banned-word filter ────────────────────────────────────────────────────
    // All words are lower-case; matching is case-insensitive.
    // Add slurs and additional profanity to this list as needed.
    static std::vector<std::string> const& bannedWords() {
        static const std::vector<std::string> words = {
            "fuck", "shit", "bitch", "cunt", "dick", "cock",
            "pussy", "bastard", "whore", "slut", "retard"
        };
        return words;
    }

    static bool containsBanned(std::string const& text) {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        for (auto const& w : bannedWords())
            if (lower.find(w) != std::string::npos) return true;
        return false;
    }

    // ── Submission cooldown ───────────────────────────────────────────────────
    static int64_t nowSecs() {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    static bool onCooldown(int& secondsLeft) {
        int64_t last    = Mod::get()->getSavedValue<int64_t>(COOLDOWN_KEY, 0LL);
        int64_t elapsed = nowSecs() - last;
        if (elapsed < COOLDOWN_SECS) {
            secondsLeft = static_cast<int>(COOLDOWN_SECS - elapsed);
            return true;
        }
        return false;
    }

    static void recordSubmit() {
        Mod::get()->setSavedValue<int64_t>(COOLDOWN_KEY, nowSecs());
    }

    // ── Firestore helpers ─────────────────────────────────────────────────────
    static void appendApiKey(std::string& url) {
        auto k = Mod::get()->getSettingValue<std::string>("firebase-api-key");
        if (!k.empty())
            url += (url.find('?') == std::string::npos ? "?" : "&") + ("key=" + k);
    }

    static std::string escJson(std::string const& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (unsigned char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20) {
                        char b[8];
                        snprintf(b, sizeof(b), "\\u%04x", static_cast<unsigned int>(c));
                        out += b;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
        return out;
    }

    static std::string buildDoc(
        std::string const& name, std::string const& author,
        std::string const& gamemode, std::string const& imageUrl,
        std::string const& plistUrl, std::string const& submittedBy)
    {
        return std::string("{\"fields\":{")
            + "\"name\":{\"stringValue\":\""        + escJson(name)        + "\"},"
            + "\"author\":{\"stringValue\":\""      + escJson(author)      + "\"},"
            + "\"gamemode\":{\"stringValue\":\""    + escJson(gamemode)    + "\"},"
            + "\"imageUrl\":{\"stringValue\":\""    + escJson(imageUrl)    + "\"},"
            + "\"plistUrl\":{\"stringValue\":\""    + escJson(plistUrl)    + "\"},"
            + "\"submittedBy\":{\"stringValue\":\"" + escJson(submittedBy) + "\"}"
            + "}}";
    }

    // ── UI ────────────────────────────────────────────────────────────────────
    bool init() {
        if (!Popup::init(420.f, 340.f)) return false;
        this->setTitle("Submit Icon Pack");
        m_bgSprite->setColor({30, 25, 45});

        auto winSize = m_mainLayer->getContentSize();
        const float labelX = 118.f;
        const float inputX = winSize.width / 2.f + 20.f;
        const float inputW = 200.f;
        const float startY = winSize.height - 55.f;
        const float rowH   = 40.f;

        auto addRow = [&](const char* lbl, float y, const char* placeholder) -> TextInput* {
            auto l = CCLabelBMFont::create(lbl, "chatFont.fnt");
            l->setScale(0.45f);
            l->setColor({200, 200, 225});
            l->setAnchorPoint({1.f, 0.5f});
            l->setPosition({labelX, y});
            m_mainLayer->addChild(l);

            auto inp = TextInput::create(inputW, placeholder);
            inp->setPosition({inputX, y});
            m_mainLayer->addChild(inp);
            return inp;
        };

        m_nameInput     = addRow("Pack Name", startY,          "My Cool Icons");
        m_authorInput   = addRow("Author",    startY - rowH,   "YourUsername");
        m_gamemodeInput = addRow("Gamemode",  startY - rowH*2, "cube");
        m_imageUrlInput = addRow("Image URL", startY - rowH*3, "https://...");
        m_plistUrlInput = addRow("Plist URL", startY - rowH*4, "https://...");

        // Small hint under the Gamemode field
        auto hint = CCLabelBMFont::create(
            "cube / ship / ball / ufo / wave / robot / spider / swing",
            "chatFont.fnt");
        hint->setScale(0.26f);
        hint->setColor({120, 120, 155});
        hint->setPosition({inputX, startY - rowH * 2.f - 13.f});
        m_mainLayer->addChild(hint);

        // Status / error label
        m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_statusLabel->setScale(0.35f);
        m_statusLabel->setColor({220, 80, 80});
        m_statusLabel->setPosition({winSize.width / 2.f, 32.f});
        m_mainLayer->addChild(m_statusLabel);

        // Submit button
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(menu);

        auto spr = ButtonSprite::create("Submit");
        spr->setScale(0.8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(SubmitPackPopup::onSubmit));
        btn->setPosition({winSize.width / 2.f, 14.f});
        menu->addChild(btn);

        return true;
    }

    void setStatus(const char* msg, bool error) {
        if (!m_statusLabel) return;
        m_statusLabel->setString(msg);
        m_statusLabel->setColor(
            error ? ccColor3B{220, 80, 80} : ccColor3B{80, 200, 100});
    }

    void onSubmit(CCObject*) {
        // Prevent double-submit while a request is in-flight
        if (m_submitHandle) return;

        std::string name     = m_nameInput     ? m_nameInput->getString()     : "";
        std::string author   = m_authorInput   ? m_authorInput->getString()   : "";
        std::string gamemode = m_gamemodeInput ? m_gamemodeInput->getString() : "";
        std::string imageUrl = m_imageUrlInput ? m_imageUrlInput->getString() : "";
        std::string plistUrl = m_plistUrlInput ? m_plistUrlInput->getString() : "";

        // Required fields
        if (name.empty())     { setStatus("Pack name is required.",  true); return; }
        if (gamemode.empty()) { setStatus("Gamemode is required.",   true); return; }

        // Profanity / slur filter
        if (containsBanned(name)) {
            setStatus("Pack name contains prohibited words.", true);
            return;
        }
        if (!author.empty() && containsBanned(author)) {
            setStatus("Author name contains prohibited words.", true);
            return;
        }

        // Rate limiting: at most one submission per hour
        int secondsLeft = 0;
        if (onCooldown(secondsLeft)) {
            int minutes = secondsLeft / 60, seconds = secondsLeft % 60;
            std::string msg =
                "Please wait " + std::to_string(minutes) + "m " +
                std::to_string(seconds) + "s before submitting again.";
            setStatus(msg.c_str(), true);
            return;
        }

        auto projectId = Mod::get()->getSettingValue<std::string>("firebase-project-id");
        if (projectId.empty()) {
            setStatus("Firebase Project ID is not configured.", true);
            return;
        }

        // Block submissions from unauthenticated accounts to prevent anonymous spam
        auto* acc = GJAccountManager::sharedState();
        if (!acc || acc->m_accountID <= 0) {
            setStatus("You must be logged into Geometry Dash to submit.", true);
            return;
        }
        std::string submittedBy = std::to_string(acc->m_accountID);

        std::string url =
            "https://firestore.googleapis.com/v1/projects/" + projectId +
            "/databases/(default)/documents/pendingIconPacks";
        appendApiKey(url);

        setStatus("Submitting...", false);

        std::string body = buildDoc(name, author, gamemode, imageUrl, plistUrl, submittedBy);
        Ref<SubmitPackPopup> selfRef(this);

        m_submitHandle = geode::async::spawn(
            web::WebRequest()
                .bodyString(body)
                .header("Content-Type", "application/json")
                .post(url),
            [selfRef](web::WebResponse response) mutable {
                if (!selfRef) return;
                selfRef->m_submitHandle.reset();
                if (response.ok()) {
                    recordSubmit();
                    selfRef->setStatus(
                        "Submitted! A moderator will review it soon.", false);
                } else {
                    selfRef->setStatus(
                        ("Submit failed (" +
                         std::to_string(response.code()) + ").").c_str(),
                        true);
                }
            });
    }

public:
    static SubmitPackPopup* create() {
        auto ret = new SubmitPackPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
