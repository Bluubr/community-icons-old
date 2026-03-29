#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include "GamemodeViewPopup.hpp"

using namespace geode::prelude;

struct GamemodeEntry {
    std::string name;
    IconType    type;
};

class DefaultIconsPopup : public geode::Popup {
protected:
    static std::vector<GamemodeEntry> const& getGamemodes() {
        static const std::vector<GamemodeEntry> s_gamemodes = {
            {"Cube",    IconType::Cube},
            {"Ship",    IconType::Ship},
            {"Ball",    IconType::Ball},
            {"UFO",     IconType::Ufo},
            {"Wave",    IconType::Wave},
            {"Robot",   IconType::Robot},
            {"Spider",  IconType::Spider},
            {"Swing",   IconType::Swing},
            {"Jetpack", IconType::Jetpack},
        };
        return s_gamemodes;
    }

    bool init() {
        if (!Popup::init(380.f, 310.f)) return false;

        this->setTitle("Default Icons");

        auto winSize = m_mainLayer->getContentSize();
        auto scrollLayer = ScrollLayer::create({winSize.width - 20.f, winSize.height - 50.f});
        scrollLayer->setPosition({10.f, 10.f});
        m_mainLayer->addChild(scrollLayer);

        auto const& gamemodes = getGamemodes();
        float entryH = 88.f;
        float padding = 6.f;
        float totalH = static_cast<float>(gamemodes.size()) * (entryH + padding);
        scrollLayer->m_contentLayer->setContentSize({winSize.width - 20.f, totalH});

        for (size_t i = 0; i < gamemodes.size(); i++) {
            auto const& gm = gamemodes[i];
            float y = totalH - (i * (entryH + padding)) - entryH / 2.f;

            auto bg = CCScale9Sprite::create("GJ_square02.png");
            bg->setContentSize({winSize.width - 28.f, entryH - 6.f});
            bg->setPosition({(winSize.width - 20.f) / 2.f, y});
            bg->setOpacity(100);
            scrollLayer->m_contentLayer->addChild(bg);

            auto player = SimplePlayer::create(1);
            player->updatePlayerFrame(1, gm.type);
            player->setScale(0.75f);
            player->setPosition({38.f, y});
            scrollLayer->m_contentLayer->addChild(player);

            auto nameLabel = CCLabelBMFont::create(gm.name.c_str(), "goldFont.fnt");
            nameLabel->setScale(0.65f);
            nameLabel->setAnchorPoint({0.f, 0.5f});
            nameLabel->setPosition({72.f, y});
            scrollLayer->m_contentLayer->addChild(nameLabel);

            auto viewBtnSpr = ButtonSprite::create("View");
            viewBtnSpr->setScale(0.75f);
            auto menu = CCMenu::create();
            auto viewBtn = CCMenuItemSpriteExtra::create(viewBtnSpr, this, menu_selector(DefaultIconsPopup::onViewGamemode));
            viewBtn->setTag(static_cast<int>(i));
            menu->addChild(viewBtn);
            menu->setPosition({winSize.width - 65.f, y});
            scrollLayer->m_contentLayer->addChild(menu);
        }
        scrollLayer->moveToTop();
        return true;
    }

    void onViewGamemode(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag();
        auto const& gm = getGamemodes().at(idx);
        GamemodeViewPopup::create(gm.type, gm.name)->show();
    }

public:
    static DefaultIconsPopup* create() {
        auto ret = new DefaultIconsPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
