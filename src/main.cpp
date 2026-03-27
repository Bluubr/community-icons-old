#include <Geode/Geode.hpp>
#include <Geode/modify/CharacterColorPage.hpp>
#include "DefaultIconsPopup.hpp"

using namespace geode::prelude;

class $modify(MyIconKit, CharacterColorPage) {
    bool init() {
        if (!CharacterColorPage::init()) return false;

        auto menu = this->getChildByID("buttons-menu");
        if (!menu) return true;

        // Try to load custom icon, fallback to search icon
        auto btnSpr = CCSprite::create("workshop-icon.png"_spr);
        if (!btnSpr) {
            btnSpr = CCSprite::createWithSpriteFrameName("GJ_searchBtn_001.png");
        }

        if (btnSpr) {
            btnSpr->setScale(0.8f);
        }

        auto searchBtn = CCMenuItemSpriteExtra::create(
            btnSpr,
            this,
            menu_selector(MyIconKit::onOpenDefaultIcons) // Selector fixed
        );

        menu->addChild(searchBtn);
        searchBtn->setID("workshop-search-button"_spr);

        menu->updateLayout();

        return true;
    }

    // Function must exist in this class for the selector above to work
    void onOpenDefaultIcons(CCObject* sender) {
        DefaultIconsPopup::create()->show();
    }
};
