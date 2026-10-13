#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

namespace vs {

enum class Mode { None, Bot, Online };

// Icono (cubo) y colores de un jugador
struct Cosmetic {
	int icon = 1;
	int c1 = 0;
	int c2 = 3;
};

struct Match {
	Mode mode = Mode::None;
	int face = 0;            // cara de dificultad del nivel (1..6)
	bool mainLevel = false;  // true = nivel oficial del juego
	int levelID = 0;
	std::string room, playerId;
	Cosmetic opp;            // icono y colores del rival
	float opponentPct = 0.f;
	bool opponentDone = false;
	bool over = false;
	bool inFlight = false;
};

inline Match& match() { static Match m; return m; }

// Icono y colores del bot (se guardan entre sesiones)
inline Cosmetic& botCosmetic() {
	static Cosmetic c;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		auto m = Mod::get();
		c.icon = m->getSavedValue<int>("bot-icon", 1);
		c.c1 = m->getSavedValue<int>("bot-c1", 0);
		c.c2 = m->getSavedValue<int>("bot-c2", 3);
	}
	return c;
}

inline void saveBotCosmetic() {
	auto& c = botCosmetic();
	auto m = Mod::get();
	m->setSavedValue<int>("bot-icon", c.icon);
	m->setSavedValue<int>("bot-c1", c.c1);
	m->setSavedValue<int>("bot-c2", c.c2);
}

// Tu icono y colores actuales del juego
inline Cosmetic mine() {
	auto g = GameManager::get();
	return {g->getPlayerFrame(), g->getPlayerColor(), g->getPlayerColor2()};
}

// Caras por defecto del juego: 0 N/A (aleatoria), 1..5 normales,
// 6 Demon, 7 Demon facil, 8 Demon medio, 9 Demon insano, 10 Demon extremo
inline std::string faceFrame(int f) {
	auto n = fmt::format("diffIcon_{:02}_btn_001.png", f);
	if (!CCSpriteFrameCache::get()->spriteFrameByName(n.c_str())) n = "diffIcon_06_btn_001.png";
	return n;
}

inline char const* faceName(int f) {
	static char const* n[] = {"Aleatoria", "Facil", "Normal", "Dificil", "Mas dificil", "Insano",
		"Demon", "Demon facil", "Demon medio", "Demon insano", "Demon extremo"};
	return n[std::clamp(f, 0, 10)];
}

// Estrellas -> cara (niveles online calificados)
inline int faceOfStars(int s) {
	if (s <= 0) return 0;
	if (s <= 2) return 1;
	if (s == 3) return 2;
	if (s <= 5) return 3;
	if (s <= 7) return 4;
	if (s <= 9) return 5;
	return 6;
}

// Niveles oficiales 1..22 -> cara (aproximado, ajustalo a tu gusto)
inline int mainFace(int id) {
	static int t[22] = {1,2,2,3,3,4,4,4,5,5,5,5,6,5,5,5,5,5,5,5,5,5};
	return t[std::clamp(id, 1, 22) - 1];
}

// La cara elegida (0..10) coincide con la cara del nivel (0..6)
inline bool faceMatches(int selected, int levelFace) {
	if (selected == 0) return true;
	if (selected >= 6) return levelFace == 6;
	return selected == levelFace;
}

// POST JSON al servidor de salas. cb(ok, json) se ejecuta en el hilo principal.
template <class F>
void post(std::string const& path, matjson::Value body, F cb) {
	auto url = Mod::get()->getSettingValue<std::string>("server-url") + path;
	async::spawn(
		web::WebRequest().bodyJSON(body).post(url),
		[cb = std::move(cb)](web::WebResponse res) mutable {
			if (!res.ok()) return cb(false, matjson::Value());
			auto j = res.json();
			if (!j) return cb(false, matjson::Value());
			cb(true, j.unwrap());
		}
	);
}

inline Cosmetic parseCosmetic(matjson::Value const& v) {
	Cosmetic c;
	c.icon = (int)v["icon"].asInt().unwrapOr(1);
	c.c1 = (int)v["c1"].asInt().unwrapOr(0);
	c.c2 = (int)v["c2"].asInt().unwrapOr(3);
	return c;
}

inline void putCosmetic(matjson::Value& b, Cosmetic const& c) {
	b["icon"] = c.icon;
	b["c1"] = c.c1;
	b["c2"] = c.c2;
}

}
