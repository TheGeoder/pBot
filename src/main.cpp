#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <fstream>
#include <vector>
#include <algorithm>

using namespace geode::prelude;

namespace pbot {

struct Input {
    uint32_t frame;
    uint8_t button;
    bool pressed;
    bool player2;
};

static std::vector<Input> macro;
static uint32_t frame = 0;

static bool recording = false;
static bool playing = false;
static bool injecting = false;

static size_t playbackIndex = 0;

static std::filesystem::path macroPath() {
    auto path = Mod::get()->getConfigDir() / "pbot_macro.pbot";
    std::filesystem::create_directories(path.parent_path());
    return path;
}

static void startRecording() {
    macro.clear();
    frame = 0;
    recording = true;
    playing = false;

    Notification::create("pBot: Recording started")->show();
}

static void stopRecording() {
    recording = false;

    Notification::create(
        fmt::format("pBot: Recorded {} inputs", macro.size())
    )->show();
}

static void saveMacro() {
    if (macro.empty()) {
        Notification::create("pBot: Nothing to save")->show();
        return;
    }

    std::ofstream file(macroPath(), std::ios::binary);

    if (!file) {
        Notification::create("pBot: Save failed")->show();
        return;
    }

    const char magic[] = "PBOT1";
    file.write(magic, sizeof(magic) - 1);

    uint32_t count = static_cast<uint32_t>(macro.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& input : macro) {
        file.write(
            reinterpret_cast<const char*>(&input),
            sizeof(Input)
        );
    }

    Notification::create("pBot: Macro saved")->show();
}

static void loadMacro() {
    std::ifstream file(macroPath(), std::ios::binary);

    if (!file) {
        Notification::create("pBot: Macro not found")->show();
        return;
    }

    char magic[5]{};
    file.read(magic, 5);

    if (std::string(magic, 5) != "PBOT1") {
        Notification::create("pBot: Invalid macro file")->show();
        return;
    }

    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (!file || count > 1000000) {
        Notification::create("pBot: Invalid macro data")->show();
        return;
    }

    std::vector<Input> loaded(count);

    file.read(
        reinterpret_cast<char*>(loaded.data()),
        static_cast<std::streamsize>(count * sizeof(Input))
    );

    if (!file) {
        Notification::create("pBot: Corrupted macro")->show();
        return;
    }

    macro = std::move(loaded);

    Notification::create(
        fmt::format("pBot: Loaded {} inputs", macro.size())
    )->show();
}

static void startPlayback() {
    if (macro.empty()) {
        Notification::create("pBot: Load or record a macro first")->show();
        return;
    }

    if (!PlayLayer::get()) {
        Notification::create("pBot: Enter a level first")->show();
        return;
    }

    recording = false;
    playing = true;
    playbackIndex = 0;
    frame = 0;

    Notification::create("pBot: Playback started")->show();
}

static void stopPlayback() {
    playing = false;
    Notification::create("pBot: Playback stopped")->show();
}

}

// ------------------------------------------
// PLAYER INPUT RECORDING
// ------------------------------------------

class $modify(PBotPlayer, PlayerObject) {

    bool pushButton(PlayerButton button) {
        bool result = PlayerObject::pushButton(button);

        if (!pbot::injecting && pbot::recording) {
            auto* layer = PlayLayer::get();

            if (layer) {
                bool p2 = this == layer->m_player2;
                bool p1 = this == layer->m_player1;

                if (p1 || p2) {
                    pbot::macro.push_back({
                        pbot::frame,
                        static_cast<uint8_t>(button),
                        true,
                        p2
                    });
                }
            }
        }

        return result;
    }

    bool releaseButton(PlayerButton button) {
        bool result = PlayerObject::releaseButton(button);

        if (!pbot::injecting && pbot::recording) {
            auto* layer = PlayLayer::get();

            if (layer) {
                bool p2 = this == layer->m_player2;
                bool p1 = this == layer->m_player1;

                if (p1 || p2) {
                    pbot::macro.push_back({
                        pbot::frame,
                        static_cast<uint8_t>(button),
                        false,
                        p2
                    });
                }
            }
        }

        return result;
    }
};

// ------------------------------------------
// PLAYBACK ENGINE
// ------------------------------------------

class $modify(PBotPlayLayer, PlayLayer) {

    void update(float dt) {
        PlayLayer::update(dt);

        if (pbot::recording) {
            pbot::frame++;
        }

        if (!pbot::playing)
            return;

        pbot::frame++;

        while (
            pbot::playbackIndex < pbot::macro.size() &&
            pbot::macro[pbot::playbackIndex].frame <= pbot::frame
        ) {
            const auto& input =
                pbot::macro[pbot::playbackIndex];

            pbot::injecting = true;

            this->handleButton(
                input.pressed,
                input.button,
                !input.player2
            );

            pbot::injecting = false;

            pbot::playbackIndex++;
        }

        if (pbot::playbackIndex >= pbot::macro.size()) {
            pbot::stopPlayback();
        }
    }
};

// ------------------------------------------
// PBOT MENU
// ------------------------------------------

class $modify(PBotMenuLayer, MenuLayer) {

    bool init() {
        if (!MenuLayer::init())
            return false;

        auto sprite = CCSprite::create("Menu_btn.png");

        if (!sprite) {
            log::error("pBot: Menu_btn.png missing!");
            return true;
        }

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(PBotMenuLayer::openPBot)
        );

        auto menu = CCMenu::create();
        menu->addChild(button);

        auto size = CCDirector::sharedDirector()->getWinSize();

        menu->setPosition(
            ccp(size.width - 45, size.height - 45)
        );

        addChild(menu, 100);

        return true;
    }

    void openPBot(CCObject*) {
        auto size = CCDirector::sharedDirector()->getWinSize();

        auto popup = CCLayerColor::create(
            ccc4(20, 10, 40, 245),
            330,
            240
        );

        popup->setPosition(ccp(
            (size.width - 330) / 2,
            (size.height - 240) / 2
        ));

        this->addChild(popup, 200);

        auto title = CCLabelTTF::create(
            "pBot",
            "Arial",
            32
        );

        title->setPosition(ccp(165, 205));
        popup->addChild(title);

        auto makeButton = [&](const char* text,
                              SEL_MenuHandler callback,
                              float x,
                              float y) {

            auto label = CCLabelTTF::create(
                text,
                "Arial",
                19
            );

            auto item = CCMenuItemLabel::create(
                label,
                this,
                callback
            );

            item->setPosition(ccp(x, y));

            return item;
        };

        auto buttons = CCMenu::create();

        buttons->addChild(makeButton(
            "Record",
            menu_selector(PBotMenuLayer::onRecord),
            80, 155
        ));

        buttons->addChild(makeButton(
            "Stop",
            menu_selector(PBotMenuLayer::onStop),
            250, 155
        ));

        buttons->addChild(makeButton(
            "Save",
            menu_selector(PBotMenuLayer::onSave),
            80, 105
        ));

        buttons->addChild(makeButton(
            "Load",
            menu_selector(PBotMenuLayer::onLoad),
            250, 105
        ));

        buttons->addChild(makeButton(
            "Play",
            menu_selector(PBotMenuLayer::onPlay),
            165, 55
        ));

        popup->addChild(buttons);

        auto close = CCLabelTTF::create(
            "X",
            "Arial",
            22
        );

        auto closeButton = CCMenuItemLabel::create(
            close,
            popup,
            menu_selector(CCNode::removeFromParent)
        );

        auto closeMenu = CCMenu::create();
        closeMenu->addChild(closeButton);
        closeMenu->setPosition(ccp(310, 220));

        popup->addChild(closeMenu);
    }

    void onRecord(CCObject*) {
        pbot::startRecording();
    }

    void onStop(CCObject*) {
        if (pbot::recording)
            pbot::stopRecording();

        if (pbot::playing)
            pbot::stopPlayback();
    }

    void onSave(CCObject*) {
        pbot::saveMacro();
    }

    void onLoad(CCObject*) {
        pbot::loadMacro();
    }

    void onPlay(CCObject*) {
        pbot::startPlayback();
    }
};
