#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <random>
#include "VersusLayer.hpp"

// ---------- Boton en el menu principal (cara de dificultad) ----------
class $modify(VsMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		auto spr = CircleButtonSprite::createWithSpriteFrameName(
			"diffIcon_05_btn_001.png", 1.f, CircleBaseColor::Green, CircleBaseSize::MediumAlt);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(VsMenu::onVersus));
		btn->setID("versus-button"_spr);
		if (auto menu = this->getChildByID("bottom-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		}
		return true;
	}
	void onVersus(CCObject*) {
		CCDirector::get()->pushScene(CCTransitionFade::create(.5f, VersusLayer::scene()));
	}
};

// ---------- Carrera dentro del nivel ----------
class $modify(VsPlayLayer, PlayLayer) {
	struct Fields {
		CCSprite* face = nullptr;
		float botTotal = 0.f;
		float sendT = 0.f;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		auto& m = vs::match();
		if (m.mode == vs::Mode::None) return true;

		auto win = CCDirector::get()->getWinSize();
		auto bar = CCLayerColor::create({0, 0, 0, 130}, 200.f, 6.f);
		bar->setPosition({win.width / 2 - 100.f, win.height - 38.f});
		m_uiLayer->addChild(bar, 50);

		auto face = CCSprite::createWithSpriteFrameName(vs::faceFrame(m.face).c_str());
		face->setScale(.4f);
		face->setPosition({0.f, 3.f});
		bar->addChild(face);
		m_fields->face = face;
		return true;
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);
		auto& m = vs::match();
		if (m.mode == vs::Mode::None || m.over || m_hasCompletedLevel) return;
		auto f = m_fields.self();

		if (m.mode == vs::Mode::Bot) {
			static std::mt19937 rng{std::random_device{}()};
			if (f->botTotal <= 0.f) {
				float len = m_levelLength > 0.f ? m_levelLength : 18000.f;
				f->botTotal = std::max(8.f, len / 311.58f); // segundos a velocidad 1x
			}
			float skill = (float)Mod::get()->getSettingValue<double>("bot-skill");
			m.opponentPct += dt / (f->botTotal / skill) * 100.f;
			// el bot "muere" mas a menudo en caras mas dificiles
			int fc = m.face == 0 ? 3 : m.face;
			float p = 0.004f * fc * fc / skill;
			if (std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p * dt) m.opponentPct = 0.f;
		} else {
			f->sendT += dt;
			if (f->sendT >= .25f && !m.inFlight) {
				f->sendT = 0.f;
				m.inFlight = true;
				matjson::Value b;
				b["room"] = m.room;
				b["playerId"] = m.playerId;
				b["pct"] = (double)this->getCurrentPercent();
				b["done"] = false;
				vs::post("/progress", b, [](bool ok, matjson::Value const& v) {
					auto& m = vs::match();
					m.inFlight = false;
					if (!ok) return;
					m.opponentPct = (float)v["opponentPct"].asDouble().unwrapOr(0.0);
					m.opponentDone = v["opponentDone"].asBool().unwrapOr(false);
				});
			}
		}

		if (f->face) f->face->setPositionX(std::min(m.opponentPct, 100.f) * 2.f);

		if (m.opponentPct >= 100.f || m.opponentDone) {
			m.over = true;
			FLAlertLayer::create("Perdiste", "El rival llego primero a la meta.", "OK")->show();
			this->pauseGame(false);
		}
	}

	void levelComplete() {
		PlayLayer::levelComplete();
		auto& m = vs::match();
		if (m.mode == vs::Mode::None || m.over) return;
		m.over = true;
		if (m.mode == vs::Mode::Online) {
			matjson::Value b;
			b["room"] = m.room;
			b["playerId"] = m.playerId;
			b["pct"] = 100.0;
			b["done"] = true;
			vs::post("/progress", b, [](bool, matjson::Value const&) {});
		}
		FLAlertLayer::create("Victoria", "Llegaste primero a la meta.", "OK")->show();
	}

	void onQuit() {
		vs::match() = vs::Match{};
		PlayLayer::onQuit();
	}
};
