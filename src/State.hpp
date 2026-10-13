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
	bool opponentQuit = false;
	int myDeaths = 0;        // intentos fallidos tuyos
	int oppDeaths = 0;       // intentos fallidos del rival
	float myBest = 0.f;      // tu mejor porcentaje
	float oppBest = 0.f;     // mejor porcentaje del rival
	bool over = false;
	bool inFlight = false;
};

// Si tras estos intentos fallidos nadie llega a la meta, gana quien tenga mas porcentaje
constexpr int kMaxDeaths = 30;

// Convierte SeedValueRSV o int a int, segun la version del juego
template <class T>
int toInt(T const& v) {
	if constexpr (requires { v.value(); }) return (int)v.value();
	else return (int)v;
}

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
	if (s <= 1) return 0; // sin calificar o Auto: se descarta
	if (s <= 2) return 1;
	if (s == 3) return 2;
	if (s <= 5) return 3;
	if (s <= 7) return 4;
	if (s <= 9) return 5;
	return 6;
}

// Niveles oficiales (orden real del juego) -> cara. 0 = no se usa al filtrar por cara
// 1 Stereo Madness, 2 Back on Track, 3 Polargeist, 4 Dry Out, 5 Base After Base,
// 6 Can't Let Go, 7 Jumper, 8 Time Machine, 9 Cycles, 10 xStep, 11 Clutterfunk,
// 12 Theory of Everything, 13 Electroman Adventures, 14 Clubstep, 15 Electrodynamix,
// 16 Hexagon Force, 17 Blast Processing, 18 Theory of Everything 2,
// 19 Geometrical Dominator, 20 Deadlocked, 21 Fingerdash, 22 Dash
inline int mainFace(int id) {
	static int t[22] = {1,1,2,2,3,3,4,4,4,5,5,5,5,6,0,0,0,6,0,0,0,0};
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

// Envia tu progreso al servidor y guarda lo que responde sobre el rival
inline void sendProgress(float pct, bool done, bool quit) {
	auto& m = match();
	matjson::Value b;
	b["room"] = m.room;
	b["playerId"] = m.playerId;
	b["pct"] = (double)pct;
	b["done"] = done;
	b["quit"] = quit;
	b["deaths"] = m.myDeaths;
	b["best"] = (double)m.myBest;
	m.inFlight = true;
	post("/progress", b, [](bool ok, matjson::Value const& v) {
		auto& m = match();
		m.inFlight = false;
		if (!ok) return;
		m.opponentPct = (float)v["opponentPct"].asDouble().unwrapOr(0.0);
		m.opponentDone = v["opponentDone"].asBool().unwrapOr(false);
		m.opponentQuit = v["opponentQuit"].asBool().unwrapOr(false);
		m.oppDeaths = (int)v["opponentDeaths"].asInt().unwrapOr(0);
		m.oppBest = std::max(m.oppBest, (float)v["opponentBest"].asDouble().unwrapOr(0.0));
	});
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
