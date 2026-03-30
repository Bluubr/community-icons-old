#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class GamemodeViewPopup : public geode::Popup {
protected:
    IconType m_iconType;
    std::string m_gamemodeName;
    std::string m_searchText;
    int m_currentPage = 0;
    CCNode* m_iconGrid = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;

    static constexpr int COLS = 4;
    static constexpr int ROWS = 6;
    static constexpr int PAGE_SIZE = COLS * ROWS;

    bool init(IconType iconType, std::string const& name) {
        if (!Popup::init(380.f, 400.f)) return false;

        m_iconType = iconType;
        m_gamemodeName = name;
        this->setTitle(name);

        auto winSize = m_mainLayer->getContentSize();

        // Search bar
        auto searchBar = TextInput::create(220.f, "Search by number...");
        searchBar->setPosition({winSize.width / 2.f, winSize.height - 35.f});
        m_mainLayer->addChild(searchBar);
        searchBar->setCallback([this](std::string const& text) {
            m_searchText = text;
            m_currentPage = 0;
            this->refreshIcons();
        });

        // Prev / Next navigation menu
        auto navMenu = CCMenu::create();
        navMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(navMenu);

        auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        prevSpr->setScale(0.65f);
        prevSpr->setFlipX(true);
        auto prevBtn = CCMenuItemSpriteExtra::create(
            prevSpr, this, menu_selector(GamemodeViewPopup::onPrevPage));
        prevBtn->setPosition({20.f, winSize.height / 2.f - 10.f});
        navMenu->addChild(prevBtn);

        auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        nextSpr->setScale(0.65f);
        auto nextBtn = CCMenuItemSpriteExtra::create(
            nextSpr, this, menu_selector(GamemodeViewPopup::onNextPage));
        nextBtn->setPosition({winSize.width - 20.f, winSize.height / 2.f - 10.f});
        navMenu->addChild(nextBtn);

        // Page indicator
        m_pageLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_pageLabel->setScale(0.55f);
        m_pageLabel->setPosition({winSize.width / 2.f, 16.f});
        m_mainLayer->addChild(m_pageLabel);

        // Container rebuilt on each refresh
        m_iconGrid = CCNode::create();
        m_iconGrid->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_iconGrid);

        this->refreshIcons();
        return true;
    }

    std::vector<int> getFilteredIcons() {
        int total = GameManager::sharedState()->countForType(m_iconType);
        std::vector<int> result;
        // GD icon IDs are 1-based
        for (int i = 1; i <= total; i++) {
            if (!m_searchText.empty() &&
                std::to_string(i).find(m_searchText) == std::string::npos)
                continue;
            result.push_back(i);
        }
        return result;
    }

    int calcTotalPages() {
        int n = static_cast<int>(getFilteredIcons().size());
        return std::max(1, (n + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    void refreshIcons() {
        m_iconGrid->removeAllChildren();

        auto winSize = m_mainLayer->getContentSize();
        auto filtered = getFilteredIcons();

        int totalPages = std::max(1, static_cast<int>(filtered.size() + PAGE_SIZE - 1) / PAGE_SIZE);
        if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;
        if (m_currentPage < 0) m_currentPage = 0;

        // Page label
        std::string pageStr =
            std::to_string(m_currentPage + 1) + " / " + std::to_string(totalPages);
        m_pageLabel->setString(pageStr.c_str());

        // Grid layout: 4 columns, 6 rows — margins leave room for arrow buttons
        float iconSpacingX = (winSize.width - 80.f) / COLS;
        float iconSpacingY = 42.f;
        float startX = 40.f + iconSpacingX / 2.f;
        float startY = winSize.height - 70.f;

        int startIdx = m_currentPage * PAGE_SIZE;
        int endIdx = std::min(startIdx + PAGE_SIZE, static_cast<int>(filtered.size()));

        for (int i = startIdx; i < endIdx; i++) {
            int frame = filtered[i];
            int localIdx = i - startIdx;
            int col = localIdx % COLS;
            int row = localIdx / COLS;

            float x = startX + col * iconSpacingX;
            float y = startY - row * iconSpacingY;

            auto player = SimplePlayer::create(0);
            player->updatePlayerFrame(frame, m_iconType);
            player->setScale(0.75f);
            player->setPosition({x, y});
            m_iconGrid->addChild(player);

            auto label = CCLabelBMFont::create(
                std::to_string(frame).c_str(), "chatFont.fnt");
            label->setScale(0.38f);
            label->setPosition({x, y - 22.f});
            m_iconGrid->addChild(label);
        }
    }

    void onPrevPage(CCObject*) {
        if (m_currentPage > 0) {
            m_currentPage--;
            refreshIcons();
        }
    }

    void onNextPage(CCObject*) {
        if (m_currentPage < calcTotalPages() - 1) {
            m_currentPage++;
            refreshIcons();
        }
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
