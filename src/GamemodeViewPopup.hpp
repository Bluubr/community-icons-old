#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class GamemodeViewPopup : public geode::Popup<IconType, std::string const&> {
protected:
    IconType m_iconType;
    std::string m_gamemodeName;
    std::string m_searchText;
    ScrollLayer* m_scrollLayer = nullptr;

    bool setup(IconType iconType, std::string const& name) override {
        m_iconType = iconType;
        m_gamemodeName = name;

        this->setTitle(name);

        auto winSize = m_mainLayer->getContentSize();

        // Search bar
        auto searchBar = TextInput::create(200.f, "Search...");
        searchBar->setPosition({winSize.width / 2, winSize.height - 35.f});
        m_mainLayer->addChild(searchBar);

        searchBar->setCallback([this](std::string const& text) {
            m_searchText = text;
            this->refreshIcons();
        });

        // Scrollable grid
        float scrollH = winSize.height - 85.f;
        m_scrollLayer = ScrollLayer::create({winSize.width - 40.f, scrollH});
        m_scrollLayer->setPosition({20.f, 20.f});
        m_mainLayer->addChild(m_scrollLayer);

        this->refreshIcons();
        return true;
    }

    void refreshIcons() {
        auto content = m_scrollLayer->m_contentLayer;
        content->removeAllChildren();

        int totalCount = GameManager::get()->countForType(m_iconType);
        float iconSize = 40.f;
        float padding  = 15.f;
        int   cols     = 5;

        std::vector<int> filtered;
        for (int i = 0; i <= totalCount; i++) {
            if (!m_searchText.empty() && std::to_string(i).find(m_searchText) == std::string::npos)
                continue;
            filtered.push_back(i);
        }

        int rows = (static_cast<int>(filtered.size()) + cols - 1) / cols;
        float totalH = std::max(m_scrollLayer->getContentSize().height, rows * (iconSize + padding) + padding);
        content->setContentSize({m_scrollLayer->getContentSize().width, totalH});

        for (size_t idx = 0; idx < filtered.size(); idx++) {
            int frame = filtered[idx];
            int col = idx % cols;
            int row = idx / cols;

            float x = (col * (iconSize + padding)) + (iconSize / 2) + (padding / 2);
            float y = totalH - (row * (iconSize + padding)) - (iconSize / 2) - (padding / 2);

            auto player = SimplePlayer::create(frame);
            player->updatePlayerFrame(frame, m_iconType);
            player->setScale(0.6f);
            player->setPosition({x, y});
            content->addChild(player);

            auto label = CCLabelBMFont::create(std::to_string(frame).c_str(), "chatFont.fnt");
            label->setScale(0.4f);
            label->setPosition({x, y - 18.f});
            content->addChild(label);
        }
        m_scrollLayer->moveToTop();
    }

public:
    static GamemodeViewPopup* create(IconType type, std::string const& name) {
        auto ret = new GamemodeViewPopup();
        if (ret && ret->init(360.f, 280.f, type, name)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
