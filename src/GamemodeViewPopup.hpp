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
    CCLabelBMFont* m_emptyLabel = nullptr;

    static constexpr int GRID_COLS = 8;
    static constexpr int GRID_ROWS = 6;
    static constexpr int ICONS_PER_PAGE = GRID_COLS * GRID_ROWS;

    bool init(IconType iconType, std::string const& name) {
        if (!Popup::init(520.f, 420.f)) return false;

        m_iconType = iconType;
        m_gamemodeName = name;
        this->setTitle(name);

        // Dark mode background
        m_bgSprite->setColor({35, 35, 50});

        auto winSize = m_mainLayer->getContentSize();

        // Search bar
        auto searchBar = TextInput::create(260.f, "Search...");
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

        // Empty-state label shown when the community DB has no icons
        m_emptyLabel = CCLabelBMFont::create("No community icons yet", "bigFont.fnt");
        m_emptyLabel->setScale(0.45f);
        m_emptyLabel->setColor({170, 170, 200});
        m_emptyLabel->setPosition({winSize.width / 2.f, winSize.height / 2.f - 10.f});
        m_mainLayer->addChild(m_emptyLabel);

        // Container rebuilt on each refresh
        m_iconGrid = CCNode::create();
        m_iconGrid->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_iconGrid);

        this->refreshIcons();
        return true;
    }

    // Returns community icon IDs from the database.
    // The database does not exist yet, so this always returns an empty list.
    std::vector<int> getCommunityIcons() {
        // TODO: fetch from community icon database
        // Filter by m_searchText when implemented
        return {};
    }

    int calcTotalPages(int n) {
        return std::max(1, (n + ICONS_PER_PAGE - 1) / ICONS_PER_PAGE);
    }

    void refreshIcons() {
        m_iconGrid->removeAllChildren();

        auto winSize = m_mainLayer->getContentSize();
        auto icons = getCommunityIcons();

        bool isEmpty = icons.empty();
        m_emptyLabel->setVisible(isEmpty);
        m_pageLabel->setVisible(!isEmpty);

        if (isEmpty) return;

        int total = static_cast<int>(icons.size());
        int totalPages = calcTotalPages(total);
        if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;
        if (m_currentPage < 0) m_currentPage = 0;

        std::string pageStr =
            std::to_string(m_currentPage + 1) + " / " + std::to_string(totalPages);
        m_pageLabel->setString(pageStr.c_str());

        // Grid layout: 8 cols × 6 rows, with side margins for arrow buttons
        float cellW = (winSize.width - 60.f) / GRID_COLS;
        float cellH = (winSize.height - 80.f) / GRID_ROWS;
        float startX = 30.f + cellW / 2.f;
        float startY = winSize.height - 65.f;

        int startIdx = m_currentPage * ICONS_PER_PAGE;
        int endIdx = std::min(startIdx + ICONS_PER_PAGE, total);

        for (int i = startIdx; i < endIdx; i++) {
            int iconId = icons[i];
            int localIdx = i - startIdx;
            int col = localIdx % GRID_COLS;
            int row = localIdx / GRID_COLS;

            float x = startX + col * cellW;
            float y = startY - row * cellH;

            // Dark placeholder cell
            auto cell = CCScale9Sprite::create("GJ_square02.png");
            cell->setContentSize({cellW - 4.f, cellH - 4.f});
            cell->setPosition({x, y});
            cell->setColor({55, 55, 75});
            cell->setOpacity(210);
            m_iconGrid->addChild(cell);

            auto label = CCLabelBMFont::create(
                std::to_string(iconId).c_str(), "chatFont.fnt");
            label->setScale(0.4f);
            label->setColor({190, 190, 215});
            label->setPosition({x, y});
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
        auto icons = getCommunityIcons();
        if (m_currentPage < calcTotalPages(static_cast<int>(icons.size())) - 1) {
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
