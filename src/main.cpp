#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/CharacterColorPage.hpp>
#include "DefaultIconsPopup.hpp"

using namespace geode::prelude;

class $modify(MyIconKit, CharacterColorPage) {
    bool init() {
        if (!CharacterColorPage::init()) return false;

        auto menu = this->getChildByID("buttons-menu");
        if (!menu) return true;

        auto btnSpr = CCSprite::create("workshop-icon.png"_spr);
        if (!btnSpr) {
            btnSpr = CCSprite::createWithSpriteFrameName("GJ_searchBtn_001.png");
        }
        btnSpr->setScale(0.8f);

        auto searchBtn = CCMenuItemSpriteExtra::create(
            btnSpr,
            this,
            menu_selector(MyIconKit::onOpenDefaultIcons)
        );

        menu->addChild(searchBtn);
        searchBtn->setID("workshop-search-button"_spr);
        menu->updateLayout();

        return true;
    }

    void onOpenDefaultIcons(CCObject* sender) {
        DefaultIconsPopup::create()->show();
    }
};
