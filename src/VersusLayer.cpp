#include "VersusLayer.hpp"
#include <random>

static std::mt19937& rng() { static std::mt19937 r{std::random_device{}()}; return r; }
static int randInt(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng()); }

static ButtonSprite* mkBtn(char const* t, char const* tex = "GJ_button_01.png") {
	return ButtonSprite::create(t, 100, true, "bigFont.fnt", tex, 30.f, .7f);
}

VersusLayer* VersusLayer::create() {
	auto r = new VersusLayer();
	if (r && r->init()) { r->autorelease(); return r; }
	CC_SAFE_DELETE(r);
	return nullptr;
}

CCScene* VersusLayer::scene() {
	auto s = CCScene::create();
	s->addChild(VersusLayer::create());
	return s;
}

VersusLayer::~VersusLayer() {
	auto glm = GameLevelManager::get();
	if (glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
	if (glm->m_levelDownloadDelegate == this) glm->m_levelDownloadDelegate = nullptr;
}

bool VersusLayer::init() {
	if (!CCLayer::init()) return false;
	auto win = CCDirector::get()->getWinSize();

	// --- Fondo estilo Geometry Dash ---
	auto bg = CCSprite::create("GJ_gradientBG.png");
	bg->setAnchorPoint({0, 0});
	bg->setScaleX(win.width / bg->getContentWidth());
	bg->setScaleY(win.height / bg->getContentHeight());
	bg->setColor({0, 102, 255});
	this->addChild(bg, -2);

	auto sl = CCSprite::createWithSpriteFrameName("GJ_sideArt_001.png");
	sl->setAnchorPoint({0, 0});
	this->addChild(sl, -1);
	auto sr = CCSprite::createWithSpriteFrameName("GJ_sideArt_001.png");
	sr->setAnchorPoint({1, 0});
	sr->setFlipX(true);
	sr->setPosition({win.width, 0});
	this->addChild(sr, -1);

	auto title = CCLabelBMFont::create("VERSUS", "goldFont.fnt");
	title->setPosition({win.width / 2, win.height - 28});
	this->addChild(title);

	auto menu = CCMenu::create();
	menu->setPosition({0, 0});
	this->addChild(menu);

	// --- Atras ---
	auto back = CCMenuItemSpriteExtra::create(
		CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png"), this, menu_selector(VersusLayer::onBack));
	back->setPosition({25, win.height - 25});
	menu->addChild(back);

	// --- Caras de dificultad (0 = aleatoria, 1..6) ---
	for (int i = 0; i <= 6; i++) {
		auto spr = CCSprite::createWithSpriteFrameName(vs::faceFrame(i).c_str());
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(VersusLayer::onFace));
		btn->setTag(i);
		btn->setPosition({win.width / 2 + (i - 3) * 58.f, win.height - 80});
		menu->addChild(btn);
		m_faceBtns.push_back(btn);
	}
	m_faceLabel = CCLabelBMFont::create("", "bigFont.fnt");
	m_faceLabel->setScale(.55f);
	m_faceLabel->setPosition({win.width / 2, win.height - 118});
	this->addChild(m_faceLabel);

	// --- Fuente de niveles ---
	auto tog = CCMenuItemToggler::create(
		mkBtn("Del juego", "GJ_button_02.png"), mkBtn("Online", "GJ_button_01.png"),
		this, menu_selector(VersusLayer::onSource));
	tog->toggle(true);
	tog->setPosition({win.width / 2, win.height - 160});
	menu->addChild(tog);

	// --- Modos ---
	float cx = win.width / 2;
	auto bot = CCMenuItemSpriteExtra::create(mkBtn("Vs Bot", "GJ_button_01.png"), this, menu_selector(VersusLayer::onBot));
	bot->setPosition({cx - 120, 95});
	menu->addChild(bot);
	auto create = CCMenuItemSpriteExtra::create(mkBtn("Crear sala", "GJ_button_03.png"), this, menu_selector(VersusLayer::onCreate));
	create->setPosition({cx, 95});
	menu->addChild(create);
	auto join = CCMenuItemSpriteExtra::create(mkBtn("Unirse", "GJ_button_03.png"), this, menu_selector(VersusLayer::onJoin));
	join->setPosition({cx + 120, 95});
	menu->addChild(join);

	m_code = TextInput::create(100.f, "Codigo", "bigFont.fnt");
	m_code->setFilter("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	m_code->setMaxCharCount(4);
	m_code->setPosition({cx + 120, 55});
	this->addChild(m_code);

	m_status = CCLabelBMFont::create("", "bigFont.fnt");
	m_status->setScale(.4f);
	m_status->setPosition({cx, 22});
	this->addChild(m_status);

	this->refreshFaces();
	this->setKeypadEnabled(true);
	return true;
}

void VersusLayer::onBack(CCObject*) {
	CCDirector::get()->popSceneWithTransition(.5f, PopTransition::kPopTransitionFade);
}

void VersusLayer::onFace(CCObject* s) {
	m_face = static_cast<CCNode*>(s)->getTag();
	this->refreshFaces();
}

void VersusLayer::onSource(CCObject* s) {
	// el estado se invierte despues del callback
	m_online = !static_cast<CCMenuItemToggler*>(s)->isToggled();
}

void VersusLayer::refreshFaces() {
	for (auto b : m_faceBtns) {
		static_cast<CCSprite*>(b->getNormalImage())->setOpacity(b->getTag() == m_face ? 255 : 90);
	}
	m_faceLabel->setString(fmt::format("Dificultad: {}", vs::faceName(m_face)).c_str());
}

void VersusLayer::setStatus(std::string const& s) { m_status->setString(s.c_str()); }

void VersusLayer::fail(std::string const& msg) {
	m_busy = false;
	this->unschedule(schedule_selector(VersusLayer::pollRoom));
	this->setStatus(msg);
}

// ---------- Modos ----------
void VersusLayer::onBot(CCObject*) { this->begin(vs::Mode::Bot); }
void VersusLayer::onCreate(CCObject*) { this->begin(vs::Mode::Online); }

void VersusLayer::begin(vs::Mode m) {
	if (m_busy) return;
	m_busy = true;
	m_mode = m;
	m_attempts = 0;
	vs::match() = vs::Match{};
	vs::match().mode = m;
	this->setStatus("Buscando nivel...");
	this->pickLevel();
}

void VersusLayer::pickLevel() {
	auto& m = vs::match();
	if (!m_online) {
		std::vector<int> ids;
		for (int i = 1; i <= 22; i++) if (m_face == 0 || vs::mainFace(i) == m_face) ids.push_back(i);
		if (ids.empty()) return this->fail("No hay niveles del juego con esa cara");
		m.mainLevel = true;
		m.levelID = ids[randInt(0, (int)ids.size() - 1)];
		m.face = vs::mainFace(m.levelID);
		return this->afterPick();
	}
	this->searchOnline();
}

void VersusLayer::searchOnline() {
	auto so = GJSearchObject::create(SearchType::Featured);
	so->m_page = randInt(0, 25);
	auto glm = GameLevelManager::get();
	glm->m_levelManagerDelegate = this;
	glm->getOnlineLevels(so);
}

void VersusLayer::onLevels(CCArray* levels) {
	auto glm = GameLevelManager::get();
	if (glm->m_levelManagerDelegate == this) glm->m_levelManagerDelegate = nullptr;
	std::vector<GJGameLevel*> ok;
	for (auto lvl : CCArrayExt<GJGameLevel*>(levels)) {
		int f = vs::faceOfStars(lvl->m_stars.value());
		if (f == 0) continue;
		if (m_face == 0 || f == m_face) ok.push_back(lvl);
	}
	if (ok.empty()) {
		if (++m_attempts > 8) return this->fail("No se encontro un nivel con esa cara");
		return this->searchOnline();
	}
	auto lvl = ok[randInt(0, (int)ok.size() - 1)];
	auto& m = vs::match();
	m.mainLevel = false;
	m.levelID = lvl->m_levelID.value();
	m.face = vs::faceOfStars(lvl->m_stars.value());
	this->afterPick();
}

void VersusLayer::afterPick() {
	auto& m = vs::match();
	if (m.mode == vs::Mode::Bot) return this->startLevel();

	matjson::Value b;
	b["levelID"] = m.levelID;
	b["mainLevel"] = m.mainLevel;
	b["face"] = m.face;
	WeakRef<VersusLayer> self = this;
	vs::post("/create", b, [self](bool ok, matjson::Value const& v) {
		auto s = self.lock();
		if (!s) return;
		if (!ok) return s->fail("No se pudo conectar al servidor");
		auto& m = vs::match();
		m.room = v["room"].asString().unwrapOr("");
		m.playerId = v["playerId"].asString().unwrapOr("p1");
		s->setStatus(fmt::format("Sala {} - esperando rival...", m.room));
		s->m_polling = false;
		s->schedule(schedule_selector(VersusLayer::pollRoom), 1.f);
	});
}

void VersusLayer::pollRoom(float) {
	if (m_polling) return;
	m_polling = true;
	matjson::Value b;
	b["room"] = vs::match().room;
	WeakRef<VersusLayer> self = this;
	vs::post("/status", b, [self](bool ok, matjson::Value const& v) {
		auto s = self.lock();
		if (!s) return;
		s->m_polling = false;
		if (ok && v["ready"].asBool().unwrapOr(false)) {
			s->unschedule(schedule_selector(VersusLayer::pollRoom));
			s->startLevel();
		}
	});
}

void VersusLayer::onJoin(CCObject*) {
	if (m_busy) return;
	auto code = m_code->getString();
	if (code.size() != 4) return this->setStatus("Escribe el codigo de 4 letras");
	std::transform(code.begin(), code.end(), code.begin(), ::toupper);
	m_busy = true;
	vs::match() = vs::Match{};
	vs::match().mode = vs::Mode::Online;
	vs::match().room = code;
	this->setStatus("Uniendose...");
	matjson::Value b;
	b["room"] = code;
	WeakRef<VersusLayer> self = this;
	vs::post("/join", b, [self](bool ok, matjson::Value const& v) {
		auto s = self.lock();
		if (!s) return;
		if (!ok) return s->fail("No se pudo unir (revisa codigo/servidor)");
		auto& m = vs::match();
		m.playerId = v["playerId"].asString().unwrapOr("p2");
		m.levelID = (int)v["levelID"].asInt().unwrapOr(0);
		m.mainLevel = v["mainLevel"].asBool().unwrapOr(false);
		m.face = (int)v["face"].asInt().unwrapOr(0);
		s->startLevel();
	});
}

// ---------- Cargar y jugar ----------
void VersusLayer::startLevel() {
	auto& m = vs::match();
	auto glm = GameLevelManager::get();
	this->setStatus("Cargando nivel...");
	if (m.mainLevel) return this->play(glm->getMainLevel(m.levelID, false));

	auto saved = glm->getSavedLevel(m.levelID);
	if (saved && !saved->m_levelString.empty()) return this->play(saved);
	glm->m_levelDownloadDelegate = this;
	glm->downloadLevel(m.levelID, false);
}

void VersusLayer::play(GJGameLevel* lvl) {
	if (!lvl) return this->fail("Nivel no disponible");
	auto glm = GameLevelManager::get();
	if (glm->m_levelDownloadDelegate == this) glm->m_levelDownloadDelegate = nullptr;
	auto& m = vs::match();
	m.over = false;
	m.opponentPct = 0.f;
	m.opponentDone = false;
	CCDirector::get()->replaceScene(CCTransitionFade::create(.5f, PlayLayer::scene(lvl, false, false)));
}
