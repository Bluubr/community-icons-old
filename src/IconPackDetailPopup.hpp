#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <filesystem>
#include <fstream>
#include "IconPack.hpp"

using namespace geode::prelude;

// Shown when the user taps an icon-pack cell in GamemodeViewPopup.
//
// Displays a full-size preview of the pack's thumbnail together with its name
// and author.  The user can then press "Apply Icon" to download the pack's
// assets and inject them into the running game session:
//
//   • If the pack carries a sprite-sheet plist (plistUrl is non-empty):
//       – both the image and the plist are downloaded,
//       – the plist is written to the mod's save directory,
//       – CCSpriteFrameCache::addSpriteFramesWithFile() is called with the
//         saved plist path and the decoded texture so that every sprite frame
//         defined in the sheet replaces the matching in-memory frame.
//
//   • If the pack has only an image (imageUrl, no plistUrl):
//       – the user selects an icon-slot number via a small text input,
//       – the image is decoded and pushed into CCTextureCache under the
//         sprite-name key for that slot (e.g. "player_001_001.png"),
//       – a new CCSpriteFrame covering the full texture is registered in
//         CCSpriteFrameCache under the same key, overriding the game's default.
//
// In both cases the player must reopen the Icon Kit (or restart) to see the
// change reflected on the icon-selection buttons.
class IconPackDetailPopup : public geode::Popup {
protected:
    IconPack    m_pack;
    IconType    m_iconType;

    CCSprite*      m_previewSprite = nullptr;
    CCLabelBMFont* m_statusLabel   = nullptr;
    TextInput*     m_slotInput     = nullptr;
    CCNode*        m_slotRow       = nullptr;

    std::vector<arc::TaskHandle<void>> m_handles;

    // ── Init ─────────────────────────────────────────────────────────────────

    bool init(IconPack const& pack, IconType iconType) {
        if (!Popup::init(420.f, 340.f)) return false;

        m_pack     = pack;
        m_iconType = iconType;

        this->setTitle(pack.name.empty() ? "Icon Pack" : pack.name);
        m_bgSprite->setColor({35, 35, 50});

        auto winSize = m_mainLayer->getContentSize();

        // ── Author label ─────────────────────────────────────────────────
        if (!pack.author.empty()) {
            auto authorLbl = CCLabelBMFont::create(
                ("by " + pack.author).c_str(), "chatFont.fnt");
            authorLbl->setScale(0.38f);
            authorLbl->setColor({150, 150, 185});
            authorLbl->setPosition({winSize.width / 2.f, winSize.height - 40.f});
            m_mainLayer->addChild(authorLbl);
        }

        // ── Preview area ─────────────────────────────────────────────────
        float previewH = winSize.height * 0.42f;
        float previewY = winSize.height - 56.f - previewH / 2.f;

        auto previewBg = CCScale9Sprite::create("GJ_square02.png");
        previewBg->setContentSize({winSize.width - 40.f, previewH});
        previewBg->setPosition({winSize.width / 2.f, previewY});
        previewBg->setColor({45, 45, 65});
        previewBg->setOpacity(210);
        m_mainLayer->addChild(previewBg);

        m_previewSprite = CCSprite::create();
        m_previewSprite->setPosition({winSize.width / 2.f, previewY});
        m_previewSprite->setVisible(false);
        m_mainLayer->addChild(m_previewSprite);

        // ── Slot selector (shown only when the pack has no plist) ────────
        m_slotRow = CCNode::create();
        m_slotRow->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_slotRow);

        float slotY = previewY - previewH / 2.f - 22.f;

        auto slotLbl = CCLabelBMFont::create("Replace icon #:", "chatFont.fnt");
        slotLbl->setScale(0.42f);
        slotLbl->setColor({200, 200, 225});
        slotLbl->setAnchorPoint({1.f, 0.5f});
        slotLbl->setPosition({winSize.width / 2.f, slotY});
        m_slotRow->addChild(slotLbl);

        m_slotInput = TextInput::create(72.f, "e.g. 1");
        m_slotInput->setPosition({winSize.width / 2.f + 44.f, slotY});
        m_slotInput->setString("1");
        m_slotRow->addChild(m_slotInput);

        // When a plist is available the sprite frames replace icons
        // automatically — no slot selection is necessary.
        m_slotRow->setVisible(pack.plistUrl.empty());

        // ── Status label ─────────────────────────────────────────────────
        m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_statusLabel->setScale(0.33f);
        m_statusLabel->setColor({220, 80, 80});
        m_statusLabel->setPosition({winSize.width / 2.f, 34.f});
        m_mainLayer->addChild(m_statusLabel);

        // ── Apply button ─────────────────────────────────────────────────
        auto btnMenu = CCMenu::create();
        btnMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(btnMenu);

        auto applySpr = ButtonSprite::create("Apply Icon");
        applySpr->setScale(0.75f);
        auto applyBtn = CCMenuItemSpriteExtra::create(
            applySpr, this,
            menu_selector(IconPackDetailPopup::onApply));
        applyBtn->setPosition({winSize.width / 2.f, 14.f});
        btnMenu->addChild(applyBtn);

        // ── Load preview thumbnail ────────────────────────────────────────
        if (!pack.imageUrl.empty()) {
            float maxW = winSize.width - 60.f;
            float maxH = previewH - 8.f;
            loadPreview(pack.imageUrl, maxW, maxH);
        }

        return true;
    }

    // ── Thumbnail ─────────────────────────────────────────────────────────────

    void loadPreview(std::string const& url, float maxW, float maxH) {
        std::string cacheKey = "ci_detail_" + m_pack.id;
        Ref<IconPackDetailPopup> selfRef(this);

        m_handles.push_back(geode::async::spawn(
            web::WebRequest().get(url),
            [selfRef, cacheKey, maxW, maxH](web::WebResponse response) mutable {
                if (!selfRef || !response.ok()) return;

                auto const& bytes = response.data();
                if (bytes.empty()) return;

                auto* tex =
                    CCTextureCache::sharedTextureCache()->textureForKey(cacheKey.c_str());
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

                if (!tex || !selfRef->m_previewSprite) return;
                auto sz = tex->getContentSize();
                if (sz.width <= 0.f || sz.height <= 0.f) return;

                selfRef->m_previewSprite->setTexture(tex);
                selfRef->m_previewSprite->setTextureRect({0.f, 0.f, sz.width, sz.height});
                float scale = std::min(maxW / sz.width, maxH / sz.height);
                selfRef->m_previewSprite->setScale(std::max(scale, 0.01f));
                selfRef->m_previewSprite->setVisible(true);
            }));
    }

    // ── Sprite-name helpers ───────────────────────────────────────────────────

    // Returns the cocos2d sprite-frame name used by Geometry Dash for the
    // given icon type at the given 1-based slot index.
    static std::string iconSpriteName(IconType type, int slot) {
        char buf[64];
        switch (type) {
            case IconType::Cube:
                snprintf(buf, sizeof(buf), "player_%03d_001.png", slot);
                break;
            case IconType::Ship:
                snprintf(buf, sizeof(buf), "ship_%02d_001.png", slot);
                break;
            case IconType::Ball:
                snprintf(buf, sizeof(buf), "player_ball_%02d_001.png", slot);
                break;
            case IconType::Ufo:
                snprintf(buf, sizeof(buf), "bird_%02d_001.png", slot);
                break;
            case IconType::Wave:
                snprintf(buf, sizeof(buf), "dart_%02d_001.png", slot);
                break;
            case IconType::Robot:
                snprintf(buf, sizeof(buf), "robot_%02d_001.png", slot);
                break;
            case IconType::Spider:
                snprintf(buf, sizeof(buf), "spider_%02d_001.png", slot);
                break;
            case IconType::Swing:
                snprintf(buf, sizeof(buf), "swing_%02d_001.png", slot);
                break;
            case IconType::Jetpack:
                snprintf(buf, sizeof(buf), "jetpack_%02d_001.png", slot);
                break;
            default:
                snprintf(buf, sizeof(buf), "player_%03d_001.png", slot);
                break;
        }
        return buf;
    }

    // ── Status helper ─────────────────────────────────────────────────────────

    void setStatus(const char* msg, bool error) {
        if (!m_statusLabel) return;
        m_statusLabel->setString(msg);
        m_statusLabel->setColor(
            error ? ccColor3B{220, 80, 80} : ccColor3B{80, 200, 100});
    }

    // ── Apply button handler ──────────────────────────────────────────────────

    void onApply(CCObject*) {
        if (m_pack.imageUrl.empty() && m_pack.plistUrl.empty()) {
            setStatus("No downloadable assets for this pack.", true);
            return;
        }

        // Plist-based path: download image + plist, let CCSpriteFrameCache
        // apply all defined frames at once (no slot selection required).
        if (!m_pack.plistUrl.empty()) {
            applyWithPlist();
            return;
        }

        // Image-only path: replace a single user-chosen icon slot.
        int slot = 1;
        try {
            slot = std::stoi(m_slotInput ? m_slotInput->getString() : "1");
        } catch (...) {}
        if (slot < 1) slot = 1;

        applyImageToSlot(slot);
    }

    // ── Plist-based apply ─────────────────────────────────────────────────────

    // Downloads the pack image and plist in sequence, writes the plist to the
    // mod's save directory, then calls addSpriteFramesWithFile() so every frame
    // defined by the community pack overrides the matching in-memory frame.
    void applyWithPlist() {
        setStatus("Downloading...", false);

        std::string imageUrl = m_pack.imageUrl;
        std::string plistUrl = m_pack.plistUrl;
        std::string packId   = m_pack.id;
        Ref<IconPackDetailPopup> selfRef(this);

        if (imageUrl.empty()) {
            setStatus("No image URL for this pack.", true);
            return;
        }

        // Step 1: download the sprite-sheet image.
        m_handles.push_back(geode::async::spawn(
            web::WebRequest().get(imageUrl),
            [selfRef, plistUrl, packId](web::WebResponse imageResp) mutable {
                if (!selfRef) return;
                if (!imageResp.ok()) {
                    selfRef->setStatus("Image download failed.", true);
                    return;
                }

                auto const& imgBytes = imageResp.data();
                if (imgBytes.empty()) {
                    selfRef->setStatus("Empty image response.", true);
                    return;
                }

                // Decode the image into a CCTexture2D and cache it.
                std::string texKey = "ci_apply_" + packId;
                auto* imgObj = new CCImage();
                CCTexture2D* tex = nullptr;
                if (imgObj->initWithImageData(
                        const_cast<unsigned char*>(imgBytes.data()),
                        static_cast<int>(imgBytes.size()))) {
                    tex = CCTextureCache::sharedTextureCache()->addUIImage(
                        imgObj, texKey.c_str());
                }
                imgObj->release();

                if (!tex) {
                    selfRef->setStatus("Failed to decode image.", true);
                    return;
                }

                // Step 2: download the plist.
                selfRef->m_handles.push_back(geode::async::spawn(
                    web::WebRequest().get(plistUrl),
                    [selfRef, tex, packId](web::WebResponse plistResp) mutable {
                        if (!selfRef) return;
                        if (!plistResp.ok()) {
                            selfRef->setStatus("Plist download failed.", true);
                            return;
                        }

                        auto const& plistBytes = plistResp.data();
                        if (plistBytes.empty()) {
                            selfRef->setStatus("Empty plist response.", true);
                            return;
                        }

                        // Write the plist to disk — CCSpriteFrameCache requires
                        // a file path rather than in-memory data.
                        auto saveDir =
                            Mod::get()->getSaveDir() / "packs" / packId;
                        std::error_code ec;
                        std::filesystem::create_directories(saveDir, ec);
                        if (ec) {
                            selfRef->setStatus(
                                "Failed to create save directory.", true);
                            return;
                        }

                        auto plistPath = saveDir / "pack.plist";
                        {
                            std::ofstream ofs(
                                plistPath, std::ios::binary | std::ios::trunc);
                            if (!ofs) {
                                selfRef->setStatus(
                                    "Failed to write plist to disk.", true);
                                return;
                            }
                            ofs.write(
                                reinterpret_cast<const char*>(plistBytes.data()),
                                static_cast<std::streamsize>(plistBytes.size()));
                        }

                        // Load all sprite frames from the plist using the
                        // already-decoded texture so no additional I/O is needed.
                        // Store the path string to avoid a dangling c_str().
                        std::string plistPathStr = plistPath.string();
                        CCSpriteFrameCache::sharedSpriteFrameCache()
                            ->addSpriteFramesWithFile(
                                plistPathStr.c_str(), tex);

                        selfRef->setStatus(
                            "Applied! Reopen the Icon Kit to see changes.",
                            false);
                    }));
            }));
    }

    // ── Image-only slot apply ─────────────────────────────────────────────────

    // Downloads the pack's preview image, decodes it, and registers it in the
    // sprite-frame cache under the name that Geometry Dash uses for the chosen
    // icon slot.  Reopening the Icon Kit (or restarting the game) is required
    // for the change to appear on the icon buttons.
    void applyImageToSlot(int slot) {
        setStatus("Downloading...", false);

        std::string imageUrl = m_pack.imageUrl;
        IconType    iconType = m_iconType;
        Ref<IconPackDetailPopup> selfRef(this);

        m_handles.push_back(geode::async::spawn(
            web::WebRequest().get(imageUrl),
            [selfRef, slot, iconType](web::WebResponse response) mutable {
                if (!selfRef) return;
                if (!response.ok()) {
                    selfRef->setStatus("Download failed.", true);
                    return;
                }

                auto const& bytes = response.data();
                if (bytes.empty()) {
                    selfRef->setStatus("Empty response.", true);
                    return;
                }

                std::string spriteName = iconSpriteName(iconType, slot);

                auto* img = new CCImage();
                bool decoded = img->initWithImageData(
                    const_cast<unsigned char*>(bytes.data()),
                    static_cast<int>(bytes.size()));
                if (!decoded) {
                    img->release();
                    selfRef->setStatus("Failed to decode image.", true);
                    return;
                }

                // Add (or overwrite) the texture in the cache using the
                // sprite name as the key so subsequent spriteFrameByName()
                // calls find the new texture.
                CCTexture2D* tex = CCTextureCache::sharedTextureCache()->addUIImage(
                    img, spriteName.c_str());
                img->release();

                if (!tex) {
                    selfRef->setStatus("Failed to load texture.", true);
                    return;
                }

                // Register a new CCSpriteFrame that covers the full texture
                // rect under the sprite name, replacing the existing frame.
                auto sz = tex->getContentSize();
                auto* frame = CCSpriteFrame::createWithTexture(
                    tex, {0.f, 0.f, sz.width, sz.height});
                CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFrame(
                    frame, spriteName.c_str());

                selfRef->setStatus(
                    ("Applied to icon #" + std::to_string(slot) +
                     "! Reopen Icon Kit.").c_str(),
                    false);
            }));
    }

public:
    static IconPackDetailPopup* create(IconPack const& pack, IconType iconType) {
        auto ret = new IconPackDetailPopup();
        if (ret && ret->init(pack, iconType)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
