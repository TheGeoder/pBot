#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

class $modify(PBotMenuLayer, MenuLayer) {

    bool init() {
        if (!MenuLayer::init())
            return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto label = CCLabelTTF::create(
            "pBot",
            "Arial",
            22
        );

        auto button = CCMenuItemLabel::create(
            label,
            this,
            menu_selector(PBotMenuLayer::openPBot)
        );

        auto menu = CCMenu::create();
        menu->addChild(button);

        menu->setPosition(
            ccp(winSize.width - 55, winSize.height - 45)
        );

        this->addChild(menu, 100);

        return true;
    }

    void openPBot(CCObject*) {
        FLAlertLayer::create(
            "pBot",
            "Welcome to pBot!<cr><cy>Macro tools coming soon.</cy>",
            "OK"
        )->show();
    }
};
