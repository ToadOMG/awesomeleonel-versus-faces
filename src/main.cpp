#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <random>
#include "VersusLayer.hpp"

// ---------- Boton en el menu principal (cara de dificultad) ----------
class $modify(VsMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		auto spr = CircleButtonSprite::createWithSpriteFrameName(
			"diffIcon_06_btn_001.png", 1.f, CircleBaseColor::Green, CircleBaseSize::MediumAlt);
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
		SimplePlayer* icon = nullptr;
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

		// icono del rival sobre la barra
		auto g = GameManager::get();
		auto icon = SimplePlayer::create(m.opp.icon);
		icon->setColor(g->colorForIdx(m.opp.c1));
		icon->setSecondColor(g->colorForIdx(m.opp.c2));
		icon->setScale(.4f);
		icon->setPosition({0.f, 9.f});
		bar->addChild(icon);
		m_fields->icon = icon;
		return true;
	}

	// Cartel de resultado y salida del nivel
	void endRace(bool won) {
		auto win = CCDirector::get()->getWinSize();
		auto lbl = CCLabelBMFont::create(won ? "VICTORIA" : "DERROTA", "goldFont.fnt");
		lbl->setPosition({win.width / 2, win.height / 2 + 40.f});
		lbl->setScale(1.2f);
		if (auto sc = CCDirector::get()->getRunningScene()) sc->addChild(lbl, 9999);
		this->runAction(CCSequence::create(
			CCDelayTime::create(2.2f),
			CCCallFunc::create(this, callfunc_selector(VsPlayLayer::exitRace)),
			nullptr));
	}

	void exitRace() { this->onQuit(); }

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
			int fc = std::clamp(m.face, 1, 6);
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

		if (f->icon) f->icon->setPositionX(std::min(m.opponentPct, 100.f) * 2.f);

		// Llego el rival: pierdes y sales del nivel
		if (m.opponentPct >= 100.f || m.opponentDone) {
			m.over = true;
			this->endRace(false);
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
		// Llegaste tu: ganas y sales del nivel
		this->endRace(true);
	}

	void onQuit() {
		vs::match() = vs::Match{};
		PlayLayer::onQuit();
	}
};
