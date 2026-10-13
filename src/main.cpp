#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
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

// ---------- Preguntar si te rindes al salir del nivel ----------
class $modify(VsPause, PauseLayer) {
	void onQuit(CCObject* sender) {
		auto& m = vs::match();
		if (m.mode == vs::Mode::None || m.over) return PauseLayer::onQuit(sender);
		createQuickPopup(
			"Rendirse",
			"Si sales ahora pierdes la partida contra el rival. Te rindes?",
			"Seguir", "Rendirme",
			[this](FLAlertLayer*, bool giveUp) {
				if (!giveUp) return;
				auto& m = vs::match();
				if (m.mode == vs::Mode::Online) vs::sendProgress(0.f, false, true);
				m.over = true;
				PauseLayer::onQuit(nullptr);
			});
	}
};

// ---------- Carrera dentro del nivel ----------
class $modify(VsPlayLayer, PlayLayer) {
	struct Fields {
		SimplePlayer* icon = nullptr;
		CCLabelBMFont* info = nullptr;
		float botTotal = 0.f;
		float sendT = 0.f;
		bool started = false;
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

		// intentos y mejor porcentaje del rival
		auto info = CCLabelBMFont::create("", "bigFont.fnt");
		info->setScale(.3f);
		info->setPosition({100.f, -8.f});
		bar->addChild(info);
		m_fields->info = info;
		return true;
	}

	// Cartel de resultado y salida del nivel. result: 1 gana, 0 pierde, 2 empate
	void endRace(int result) {
		auto win = CCDirector::get()->getWinSize();
		char const* txt = result == 1 ? "VICTORIA" : result == 0 ? "DERROTA" : "EMPATE";
		auto lbl = CCLabelBMFont::create(txt, "goldFont.fnt");
		lbl->setPosition({win.width / 2, win.height / 2 + 40.f});
		lbl->setScale(1.2f);
		if (auto sc = CCDirector::get()->getRunningScene()) sc->addChild(lbl, 9999);
		this->runAction(CCSequence::create(
			CCDelayTime::create(2.2f),
			CCCallFunc::create(this, callfunc_selector(VsPlayLayer::exitRace)),
			nullptr));
	}

	void exitRace() { this->onQuit(); }

	// Se acabaron los intentos: gana quien tenga mas porcentaje
	void finishByPercent() {
		auto& m = vs::match();
		m.over = true;
		if (m.mode == vs::Mode::Online) vs::sendProgress(m.myBest, false, false);
		int r = m.myBest > m.oppBest ? 1 : m.myBest < m.oppBest ? 0 : 2;
		this->endRace(r);
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		auto& m = vs::match();
		if (m.mode == vs::Mode::None || m.over || !m_fields->started) return;
		m.myDeaths++;
		if (m.myDeaths >= vs::kMaxDeaths) this->finishByPercent();
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);
		auto& m = vs::match();
		if (m.mode == vs::Mode::None || m.over || m_hasCompletedLevel) return;

		float cur = this->getCurrentPercent();
		m_fields->started = true;
		m.myBest = std::max(m.myBest, cur);

		if (m.mode == vs::Mode::Bot) {
			static std::mt19937 rng{std::random_device{}()};
			if (m_fields->botTotal <= 0.f) {
				float len = m_levelLength > 0.f ? m_levelLength : 18000.f;
				m_fields->botTotal = std::max(8.f, len / 311.58f); // segundos a velocidad 1x
			}
			float skill = (float)Mod::get()->getSettingValue<double>("bot-skill");
			m.opponentPct += dt / (m_fields->botTotal / skill) * 100.f;
			m.oppBest = std::max(m.oppBest, std::min(m.opponentPct, 100.f));
			// el bot "muere" mas a menudo en caras mas dificiles
			int fc = m.face == 0 ? 3 : std::clamp(m.face, 1, 6);
			float p = 0.004f * fc * fc / skill;
			if (m.opponentPct < 100.f && std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p * dt) {
				m.opponentPct = 0.f;
				m.oppDeaths++;
			}
		} else {
			m_fields->sendT += dt;
			if (m_fields->sendT >= .25f && !m.inFlight) {
				m_fields->sendT = 0.f;
				vs::sendProgress(cur, false, false);
			}
		}

		if (m_fields->icon) m_fields->icon->setPositionX(std::min(m.opponentPct, 100.f) * 2.f);
		if (m_fields->info) {
			m_fields->info->setString(fmt::format("Intento {}/{}   Rival {}%",
				std::min(m.myDeaths + 1, vs::kMaxDeaths), vs::kMaxDeaths, (int)m.oppBest).c_str());
		}

		// El rival se rindio: ganas
		if (m.opponentQuit) {
			m.over = true;
			return this->endRace(1);
		}
		// Llego el rival: pierdes y sales del nivel
		if (m.opponentPct >= 100.f || m.opponentDone) {
			m.over = true;
			return this->endRace(0);
		}
		// El rival agoto sus intentos sin llegar
		if (m.oppDeaths >= vs::kMaxDeaths) this->finishByPercent();
	}

	void levelComplete() {
		PlayLayer::levelComplete();
		auto& m = vs::match();
		if (m.mode == vs::Mode::None || m.over) return;
		m.over = true;
		if (m.mode == vs::Mode::Online) vs::sendProgress(100.f, true, false);
		// Llegaste tu: ganas y sales del nivel
		this->endRace(1);
	}

	void onQuit() {
		vs::match() = vs::Match{};
		PlayLayer::onQuit();
	}
};
