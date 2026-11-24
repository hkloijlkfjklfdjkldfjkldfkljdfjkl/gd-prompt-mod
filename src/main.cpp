#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/utils/web.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class PromptPopup : public geode::Popup<> {
protected:
    TextInput* m_input = nullptr;
    CCMenuItemSpriteExtra* m_genBtn = nullptr;
    EventListener<web::WebTask> m_listener;

    bool setup() override {
        this->setTitle("AI Генератор Уровня");
        auto winSize = m_mainLayer->getContentSize();

        m_input = TextInput::create(300.f, "Опиши участок (напр. 3 прыжка через шипы)...");
        m_input->setPosition({winSize.width / 2, winSize.height / 2 + 15.f});
        m_mainLayer->addChild(m_input);

        auto btnSpr = ButtonSprite::create("Создать", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        m_genBtn = CCMenuItemSpriteExtra::create(
            btnSpr, this, menu_selector(PromptPopup::onGenerate)
        );
        m_genBtn->setPosition({winSize.width / 2, winSize.height / 2 - 40.f});

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        menu->addChild(m_genBtn);
        m_mainLayer->addChild(menu);

        return true;
    }

    void onGenerate(CCObject*) {
        std::string prompt = m_input->getString();
        if (prompt.empty()) {
            FLAlertLayer::create("Ошибка", "Введи текстовый запрос!", "ОК")->show();
            return;
        }

        std::string apiKey = Mod::get()->getSettingValue<std::string>("api-key");
        if (apiKey.empty()) {
            FLAlertLayer::create("Нет ключа", "Укажи API Key в настройках мода Geode!", "ОК")->show();
            return;
        }

        m_genBtn->setEnabled(false);

        std::string sysPrompt = 
            "You are a Geometry Dash level generator. Output ONLY raw Geometry Dash object strings separated by semicolons. "
            "Format: 1,<obj_id>,2,<x>,3,<y>; "
            "Common IDs: 1 = standard block, 8 = spike, 36 = yellow orb, 140 = pink pad. "
            "1 grid unit = 30 pixels. Ground Y is 15. Start X at 300. "
            "Do NOT write markdown, explanations, or code blocks. Only the raw string.";

        matjson::Value body = matjson::makeObject({
            {"model", Mod::get()->getSettingValue<std::string>("model")},
            {"messages", matjson::makeArray({
                matjson::makeObject({{"role", "system"}, {"content", sysPrompt}}),
                matjson::makeObject({{"role", "user"}, {"content", prompt}})
            })},
            {"temperature", 0.3}
        });

        web::WebRequest req;
        req.header("Authorization", fmt::format("Bearer {}", apiKey));
        req.header("Content-Type", "application/json");
        req.bodyJSON(body);

        m_listener.bind([this](web::WebTask::Event* e) {
            if (web::WebResponse* res = e->getValue()) {
                m_genBtn->setEnabled(true);
                if (!res->ok()) {
                    FLAlertLayer::create("Ошибка сети", fmt::format("Код: {}", res->code()), "ОК")->show();
                    return;
                }

                auto jsonRes = res->json();
                if (!jsonRes) return;

                auto data = jsonRes.unwrap();
                std::string levelStr = data["choices"]["message"]["content"].asString().unwrapOr("");

                if (!levelStr.empty()) {
                    if (auto editor = LevelEditorLayer::get()) {
                        editor->createObjectsFromString(levelStr.c_str(), true, true);
                        editor->updateEditorMode();
                        Notification::create("Объекты добавлены!", NotificationIcon::Success)->show();
                        this->onClose(nullptr);
                    }
                }
            }
        });

        m_listener.setFilter(req.post(Mod::get()->getSettingValue<std::string>("api-url")));
    }

public:
    static PromptPopup* create() {
        auto ret = new PromptPopup();
        if (ret && ret->initAnchored(360.f, 180.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(MyEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto spr = ButtonSprite::create("AI", "bigFont.fnt", "GJ_button_04.png", 0.6f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(MyEditorUI::onOpenAIPopup)
        );

        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({45.f, CCDirector::sharedDirector()->getWinSize().height - 85.f});
        this->addChild(menu);

        return true;
    }

    void onOpenAIPopup(CCObject*) {
        PromptPopup::create()->show();
    }
};
