#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

namespace vs {

enum class Mode { None, Bot, Online };

struct Match {
	Mode mode = Mode::None;
	int face = 0;            // 0 = N/A (aleatoria), 1 Facil ... 6 Demon
	bool mainLevel = false;  // true = nivel oficial del juego
	int levelID = 0;
	std::string room, playerId;
	float opponentPct = 0.f;
	bool opponentDone = false;
	bool over = false;
	bool inFlight = false;
};

inline Match& match() { static Match m; return m; }

// Caras por defecto del juego (GJ_GameSheet)
inline std::string faceFrame(int f) { return fmt::format("diffIcon_{:02}_btn_001.png", f); }

inline char const* faceName(int f) {
	static char const* n[] = {"Aleatoria", "Facil", "Normal", "Dificil", "Mas dificil", "Insano", "Demon"};
	return n[std::clamp(f, 0, 6)];
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

}
