#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include "IconPack.hpp"
#include "IconPackDetailPopup.hpp"
#include "SubmitPackPopup.hpp"

using namespace geode::prelude;

class GamemodeViewPopup : public geode::Popup {
protected:
    IconType    m_iconType;
    std::string m_gamemodeName;
    std::string m_searchText;
    int         m_currentPage = 0;
    bool        m_isLoading   = false;

    CCNode*        m_iconGrid     = nullptr;
    CCLabelBMFont* m_pageLabel    = nullptr;
    CCLabelBMFont* m_emptyLabel   = nullptr;
    CCLabelBMFont* m_loadingLabel = nullptr;

    // All packs for this gamemode fetched from Firestore
    std::vector<IconPack> m_allPacks;

    // Task handles kept alive for the duration of async operations;
    // destroying a handle cancels the in-flight request.
    std::optional<arc::TaskHandle<void>>   m_fetchHandle;
    std::vector<arc::TaskHandle<void>>     m_imageHandles;

    // 4 × 4 grid gives each icon-pack cell enough room for a thumbnail + text
    static constexpr int GRID_COLS      = 4;
    static constexpr int GRID_ROWS      = 4;
    static constexpr int PACKS_PER_PAGE = GRID_COLS * GRID_ROWS;

    // -----------------------------------------------------------------------
    bool init(IconType iconType, std::string const& name) {
        if (!Popup::init(520.f, 420.f)) return false;

        m_iconType    = iconType;
        m_gamemodeName = name;
        this->setTitle(name);

        // Dark background
        m_bgSprite->setColor({35, 35, 50});

        auto winSize = m_mainLayer->getContentSize();

        // Search bar
        auto searchBar = TextInput::create(260.f, "Search...");
        searchBar->setPosition({winSize.width / 2.f, winSize.height - 35.f});
        m_mainLayer->addChild(searchBar);
        searchBar->setCallback([this](std::string const& text) {
            m_searchText  = text;
            m_currentPage = 0;
            this->refreshIcons();
        });

        // Prev / Next navigation
        auto navMenu = CCMenu::create();
        navMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(navMenu);

        auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        prevSpr->setScale(0.6f);
        prevSpr->setFlipX(true);
        auto prevBtn = CCMenuItemSpriteExtra::create(
            prevSpr, this, menu_selector(GamemodeViewPopup::onPrevPage));
        prevBtn->setPosition({18.f, winSize.height / 2.f - 10.f});
        navMenu->addChild(prevBtn);

        auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        nextSpr->setScale(0.6f);
        auto nextBtn = CCMenuItemSpriteExtra::create(
            nextSpr, this, menu_selector(GamemodeViewPopup::onNextPage));
        nextBtn->setPosition({winSize.width - 18.f, winSize.height / 2.f - 10.f});
        navMenu->addChild(nextBtn);

        // Page indicator
        m_pageLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_pageLabel->setScale(0.55f);
        m_pageLabel->setColor({190, 190, 215});
        m_pageLabel->setPosition({winSize.width / 2.f, 16.f});
        m_pageLabel->setVisible(false);
        m_mainLayer->addChild(m_pageLabel);

        // Loading label
        m_loadingLabel = CCLabelBMFont::create("Loading...", "bigFont.fnt");
        m_loadingLabel->setScale(0.45f);
        m_loadingLabel->setColor({170, 170, 200});
        m_loadingLabel->setPosition({winSize.width / 2.f, winSize.height / 2.f - 10.f});
        m_loadingLabel->setVisible(false);
        m_mainLayer->addChild(m_loadingLabel);

        // Empty-state label
        m_emptyLabel = CCLabelBMFont::create("No icon packs found", "bigFont.fnt");
        m_emptyLabel->setScale(0.45f);
        m_emptyLabel->setColor({170, 170, 200});
        m_emptyLabel->setPosition({winSize.width / 2.f, winSize.height / 2.f - 10.f});
        m_emptyLabel->setVisible(false);
        m_mainLayer->addChild(m_emptyLabel);

        // Grid container; rebuilt on every page refresh
        m_iconGrid = CCNode::create();
        m_iconGrid->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_iconGrid);

        // Discord button — bottom-left corner
        auto discordMenu = CCMenu::create();
        discordMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(discordMenu);

        auto discordLbl = CCLabelBMFont::create(
            "Join the Discord to add\nyour own custom icons", "chatFont.fnt");
        discordLbl->setScale(0.38f);
        discordLbl->setColor({88, 101, 242}); // Discord blurple
        discordLbl->setAlignment(kCCTextAlignmentLeft);

        auto discordBtn = CCMenuItemSpriteExtra::create(
            discordLbl, this,
            menu_selector(GamemodeViewPopup::onDiscordBtn));
        discordBtn->setAnchorPoint({0.f, 0.5f});
        discordBtn->setPosition({8.f, 22.f});
        discordMenu->addChild(discordBtn);

        // "Submit Pack" button — bottom-right corner
        auto submitMenu = CCMenu::create();
        submitMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(submitMenu);

        auto submitSpr = ButtonSprite::create("Submit Pack");
        submitSpr->setScale(0.55f);
        auto submitBtn = CCMenuItemSpriteExtra::create(
            submitSpr, this,
            menu_selector(GamemodeViewPopup::onSubmitBtn));
        submitBtn->setAnchorPoint({1.f, 0.5f});
        submitBtn->setPosition({winSize.width - 8.f, 22.f});
        submitMenu->addChild(submitBtn);

        this->fetchPacks();
        return true;
    }

    // -----------------------------------------------------------------------
    // Firebase Firestore fetch
    // -----------------------------------------------------------------------

    void fetchPacks() {
        auto projectId = Mod::get()->getSettingValue<std::string>("firebase-project-id");
        if (projectId.empty()) {
            m_emptyLabel->setString("Set Firebase Project ID in mod settings");
            m_emptyLabel->setScale(0.3f);
            m_emptyLabel->setVisible(true);
            return;
        }

        m_isLoading = true;
        m_loadingLabel->setVisible(true);
        m_emptyLabel->setVisible(false);

        std::string url =
            "https://firestore.googleapis.com/v1/projects/" + projectId +
            "/databases/(default)/documents/iconPacks";

        Ref<GamemodeViewPopup> selfRef(this);
        m_fetchHandle = geode::async::spawn(
            web::WebRequest().get(url),
            [selfRef](web::WebResponse response) mutable {
                if (!selfRef) return;
                if (response.ok()) {
                    auto jsonResult = response.json();
                    if (jsonResult) {
                        auto packs = GamemodeViewPopup::parseFirestoreResponse(*jsonResult);
                        selfRef->onPacksFetched(std::move(packs));
                    } else {
                        selfRef->onFetchError("Failed to parse server response");
                    }
                } else {
                    selfRef->onFetchError(
                        "Network error (" + std::to_string(response.code()) + ")");
                }
            });
    }

    void onPacksFetched(std::vector<IconPack> packs) {
        // Keep only packs whose gamemode matches this popup (case-insensitive)
        auto toLower = [](unsigned char c) { return std::tolower(c); };

        std::string gmLower = m_gamemodeName;
        std::transform(gmLower.begin(), gmLower.end(), gmLower.begin(), toLower);

        m_allPacks.clear();
        for (auto& pack : packs) {
            std::string packGmLower = pack.gamemode;
            std::transform(
                packGmLower.begin(), packGmLower.end(), packGmLower.begin(), toLower);
            if (packGmLower == gmLower) {
                m_allPacks.push_back(std::move(pack));
            }
        }

        m_isLoading = false;
        m_loadingLabel->setVisible(false);
        this->refreshIcons();
    }

    void onFetchError(std::string const& msg) {
        m_isLoading = false;
        m_loadingLabel->setVisible(false);
        m_emptyLabel->setString(("Error: " + msg).c_str());
        m_emptyLabel->setScale(0.3f);
        m_emptyLabel->setVisible(true);
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    // Returns m_allPacks filtered by the current search text (name / author)
    std::vector<IconPack> getFilteredPacks() const {
        if (m_searchText.empty()) return m_allPacks;

        auto toLower = [](unsigned char c) { return std::tolower(c); };

        std::string searchLower = m_searchText;
        std::transform(
            searchLower.begin(), searchLower.end(), searchLower.begin(), toLower);

        std::vector<IconPack> out;
        for (auto const& pack : m_allPacks) {
            std::string nameLower = pack.name;
            std::transform(
                nameLower.begin(), nameLower.end(), nameLower.begin(), toLower);
            if (nameLower.find(searchLower) != std::string::npos) {
                out.push_back(pack);
            }
        }
        return out;
    }

    int calcTotalPages(int n) {
        return std::max(1, (n + PACKS_PER_PAGE - 1) / PACKS_PER_PAGE);
    }

    // -----------------------------------------------------------------------
    // Grid rendering
    // -----------------------------------------------------------------------

    void refreshIcons() {
        m_iconGrid->removeAllChildren();
        // Cancel any in-flight thumbnail downloads from the previous page
        m_imageHandles.clear();

        auto winSize = m_mainLayer->getContentSize();
        auto packs   = getFilteredPacks();

        bool isEmpty = packs.empty();
        m_emptyLabel->setVisible(isEmpty && !m_isLoading);
        m_pageLabel->setVisible(!isEmpty);

        if (isEmpty) return;

        int total      = static_cast<int>(packs.size());
        int totalPages = calcTotalPages(total);
        if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;
        if (m_currentPage < 0)          m_currentPage = 0;

        m_pageLabel->setString(
            (std::to_string(m_currentPage + 1) + " / " +
             std::to_string(totalPages)).c_str());

        // Cell dimensions — side margins leave room for nav arrows
        float cellW  = (winSize.width - 60.f) / GRID_COLS;
        float cellH  = (winSize.height - 80.f) / GRID_ROWS;
        float startX = 30.f + cellW / 2.f;
        float startY = winSize.height - 65.f;

        int startIdx = m_currentPage * PACKS_PER_PAGE;
        int endIdx   = std::min(startIdx + PACKS_PER_PAGE, total);

        for (int i = startIdx; i < endIdx; ++i) {
            int localIdx = i - startIdx;
            int col      = localIdx % GRID_COLS;
            int row      = localIdx / GRID_COLS;
            float x      = startX + col * cellW;
            float y      = startY - row * cellH;
            buildPackCell(packs[i], i, x, y, cellW, cellH);
        }
    }

    void buildPackCell(
        IconPack const& pack, int packIdx,
        float x, float y, float cellW, float cellH)
    {
        // ── Clickable cell background ────────────────────────────────────
        // The CCScale9Sprite is used directly as the CCMenuItemSpriteExtra
        // sprite so it dims on tap, providing built-in press feedback.
        auto cell = CCScale9Sprite::create("GJ_square02.png");
        cell->setContentSize({cellW - 4.f, cellH - 4.f});
        cell->setColor({55, 55, 75});
        cell->setOpacity(210);

        auto cellMenu = CCMenu::create();
        cellMenu->setPosition({0.f, 0.f});
        m_iconGrid->addChild(cellMenu);

        auto cellBtn = CCMenuItemSpriteExtra::create(
            cell, this,
            menu_selector(GamemodeViewPopup::onPackCellClicked));
        cellBtn->setPosition({x, y});
        cellBtn->setTag(packIdx);
        cellMenu->addChild(cellBtn);

        // ── Thumbnail placeholder (replaced when the image arrives) ──────
        float maxThumbW = cellW - 12.f;
        float maxThumbH = cellH * 0.52f;

        auto thumb = CCSprite::create();
        thumb->setPosition({x, y + (cellH - 4.f) * 0.18f});
        thumb->setVisible(false);
        m_iconGrid->addChild(thumb);

        // ── Pack name label ───────────────────────────────────────────────
        auto nameLabel = CCLabelBMFont::create(
            pack.name.empty() ? "Unnamed" : pack.name.c_str(), "chatFont.fnt");
        nameLabel->setScale(0.4f);
        nameLabel->setColor({220, 220, 255});
        nameLabel->limitLabelWidth(cellW - 8.f, 0.4f, 0.1f);
        nameLabel->setPosition({x, y - (cellH - 4.f) * 0.27f});
        m_iconGrid->addChild(nameLabel);

        // ── Author label ─────────────────────────────────────────────────
        if (!pack.author.empty()) {
            auto authorLabel = CCLabelBMFont::create(
                ("by " + pack.author).c_str(), "chatFont.fnt");
            authorLabel->setScale(0.3f);
            authorLabel->setColor({160, 160, 190});
            authorLabel->limitLabelWidth(cellW - 8.f, 0.3f, 0.08f);
            authorLabel->setPosition({x, y - (cellH - 4.f) * 0.39f});
            m_iconGrid->addChild(authorLabel);
        }

        // ── Async thumbnail download from Firebase Storage ───────────────
        if (pack.imageUrl.empty()) return;

        std::string imageUrl  = pack.imageUrl;
        std::string cacheKey  = "ci_thumb_" + pack.id;
        Ref<CCSprite> thumbRef(thumb);
        float capturedMaxW = maxThumbW;
        float capturedMaxH = maxThumbH;

        m_imageHandles.push_back(geode::async::spawn(
            web::WebRequest().get(imageUrl),
            [thumbRef, cacheKey, capturedMaxW, capturedMaxH]
            (web::WebResponse response) mutable
        {
            if (!response.ok()) return;

            auto const& bytes = response.data();
            if (bytes.empty()) return;

            // Re-use a previously decoded texture if available
            auto* existingTex =
                CCTextureCache::sharedTextureCache()->textureForKey(cacheKey.c_str());

            CCTexture2D* tex = existingTex;
            if (!tex) {
                auto* img = new CCImage();
                if (img->initWithImageData(
                        const_cast<unsigned char*>(bytes.data()),
                        static_cast<int>(bytes.size()))) {
                    tex = CCTextureCache::sharedTextureCache()->addUIImage(
                        img, cacheKey.c_str());
                }
                img->release();
            }

            if (!tex || !thumbRef) return;
            auto sz = tex->getContentSize();
            if (sz.width <= 0.f || sz.height <= 0.f) return;

            thumbRef->setTexture(tex);
            thumbRef->setTextureRect({0.f, 0.f, sz.width, sz.height});
            float scale = std::min(capturedMaxW / sz.width, capturedMaxH / sz.height);
            thumbRef->setScale(std::max(scale, 0.01f));
            thumbRef->setVisible(true);
        }));
    }

    // -----------------------------------------------------------------------
    // Navigation
    // -----------------------------------------------------------------------

    void onPrevPage(CCObject*) {
        if (m_currentPage > 0) {
            --m_currentPage;
            refreshIcons();
        }
    }

    void onNextPage(CCObject*) {
        auto packs = getFilteredPacks();
        if (m_currentPage < calcTotalPages(static_cast<int>(packs.size())) - 1) {
            ++m_currentPage;
            refreshIcons();
        }
    }

    void onPackCellClicked(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag();
        auto packs = getFilteredPacks();
        if (idx < 0 || idx >= static_cast<int>(packs.size())) return;
        IconPackDetailPopup::create(packs[idx])->show();
    }

    void onDiscordBtn(CCObject*) {
        auto url = Mod::get()->getSettingValue<std::string>("discord-url");
        if (!url.empty()) {
            CCApplication::sharedApplication()->openURL(url.c_str());
        }
    }

    void onSubmitBtn(CCObject*) {
        SubmitPackPopup::create()->show();
    }

    // -----------------------------------------------------------------------
    // Firestore REST API response parser
    // -----------------------------------------------------------------------

    // Safely reads a Firestore string field: { "stringValue": "..." }
    static std::string parseStringField(
        matjson::Value const& fields, std::string const& key)
    {
        if (!fields.contains(key)) return "";
        auto f = fields[key];
        if (!f.contains("stringValue")) return "";
        auto r = f["stringValue"].asString();
        return r ? *r : "";
    }

    // Safely reads a Firestore integer field: { "integerValue": "5" }
    // (Firestore REST API encodes integers as strings.)
    static int parseIntField(
        matjson::Value const& fields, std::string const& key)
    {
        if (!fields.contains(key)) return 0;
        auto f = fields[key];
        if (f.contains("integerValue")) {
            auto s = f["integerValue"].asString();
            if (s) {
                try { return std::stoi(*s); } catch (...) {}
            }
        }
        return 0;
    }

    // Safely reads a Firestore timestamp field: { "timestampValue": "2026-04-03T04:00:00Z" }
    // Returns the raw RFC 3339 string (e.g. "2026-04-03T04:00:00Z").
    static std::string parseTimestampField(
        matjson::Value const& fields, std::string const& key)
    {
        if (!fields.contains(key)) return "";
        auto f = fields[key];
        if (!f.contains("timestampValue")) return "";
        auto r = f["timestampValue"].asString();
        return r ? *r : "";
    }

    // Extracts the document ID from a full Firestore resource name.
    // e.g. "projects/p/databases/(default)/documents/iconPacks/DOC_ID" → "DOC_ID"
    static std::string extractDocId(std::string const& docName) {
        auto pos = docName.rfind('/');
        return (pos != std::string::npos) ? docName.substr(pos + 1) : docName;
    }

    // Converts a raw Firestore REST response into a list of IconPack structs.
    static std::vector<IconPack> parseFirestoreResponse(matjson::Value const& json) {
        std::vector<IconPack> packs;

        // Firestore returns {} (no "documents" key) when the collection is empty
        if (!json.contains("documents")) return packs;

        auto docsResult = json["documents"].asArray();
        if (!docsResult) return packs;

        for (auto const& doc : *docsResult) {
            if (!doc.contains("name") || !doc.contains("fields")) continue;

            auto fields = doc["fields"];

            IconPack pack;
            auto nameRes = doc["name"].asString();
            pack.id         = extractDocId(nameRes ? *nameRes : "");
            pack.name       = parseStringField(fields, "name");
            pack.author     = parseStringField(fields, "author");
            pack.gamemode   = parseStringField(fields, "gamemode");
            pack.imageUrl   = parseStringField(fields, "imageUrl");
            pack.plistUrl   = parseStringField(fields, "plistUrl");
            pack.graphicsType = parseStringField(fields, "graphicsType");
            pack.uploadedAt = parseTimestampField(fields, "uploadedAt");
            pack.downloads  = parseIntField(fields, "Downloads");
            packs.push_back(std::move(pack));
        }

        return packs;
    }

public:
    static GamemodeViewPopup* create(IconType type, std::string const& name) {
        auto ret = new GamemodeViewPopup();
        if (ret && ret->init(type, name)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
