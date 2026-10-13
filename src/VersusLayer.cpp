#include "VersusLayer.hpp"
#include <random>

static std::mt19937& rng() { static std::mt19937 r{std::random_device{}()}; return r; }
static int randInt(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng()); }

static ButtonSprite* mkBtn(char const* t, char const* tex = "GJ_button_01.png", int w = 100, float sc = .7f) {
	return ButtonSprite::create(t, w, true, "bigFont.fnt", tex, 30.f, sc);
}

static CCLabelBMFont* mkLabel(CCNode* parent, char const* t, char const* font, float sc, CCPoint pos) {
	auto l = CCLabelBMFont::create(t, font);
	l->setScale(sc);
	l->setPosition(pos);
	parent->addChild(l);
	return l;
}

static void paint(SimplePlayer* p, vs::Cosmetic const& c) {
	auto g = GameManager::get();
	p->updatePlayerFrame(c.icon, IconType::Cube);
	p->setColor(g->colorForIdx(c.c1));
	p->setSecondColor(g->colorForIdx(c.c2));
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
	float H = win.height;

	// --- Fondo estilo Geometry Dash ---
	auto bg = CCSprite::create("GJ_gradientBG.png");
	bg->setAnchorPoint({0, 0});
	bg->setScaleX(win.width / bg->getContentWidth());
	bg->setScaleY(win.height / bg->getContentHeight());
	bg->setColor({0, 102, 255});
	this->addChild(bg, -3);

	auto sl = CCSprite::createWithSpriteFrameName("GJ_sideArt_001.png");
	sl->setAnchorPoint({0, 0});
	this->addChild(sl, -2);
	auto sr = CCSprite::createWithSpriteFrameName("GJ_sideArt_001.png");
	sr->setAnchorPoint({1, 0});
	sr->setFlipX(true);
	sr->setPosition({win.width, 0});
	this->addChild(sr, -2);

	mkLabel(this, "VERSUS", "goldFont.fnt", .9f, {win.width / 2 + 40, H - 22});

	// --- Panel izquierdo: TU icono arriba, RIVAL abajo ---
	float lx = 95.f;
	auto panel = CCLayerColor::create({0, 0, 0, 90}, 150.f, H - 30.f);
	panel->setPosition({lx - 75.f, 12.f});
	this->addChild(panel, -1);

	auto g = GameManager::get();
	mkLabel(this, "TU", "bigFont.fnt", .6f, {lx, H - 40});
	auto me = SimplePlayer::create(g->getPlayerFrame());
	paint(me, vs::mine());
	me->setScale(1.5f);
	me->setPosition({lx, H - 80});
	this->addChild(me);

	mkLabel(this, "VS", "goldFont.fnt", .8f, {lx, H - 125});

	mkLabel(this, "RIVAL", "bigFont.fnt", .6f, {lx, H - 160});
	m_bot = SimplePlayer::create(1);
	m_bot->setScale(1.5f);
	m_bot->setPosition({lx, H - 200});
	this->addChild(m_bot);

	auto menu = CCMenu::create();
	menu->setPosition({0, 0});
	this->addChild(menu);

	// Elegir icono del bot
	for (int d : {-1, 1}) {
		auto arrow = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
		arrow->setScale(.5f);
		arrow->setFlipX(d < 0);
		auto b = CCMenuItemSpriteExtra::create(arrow, this, menu_selector(VersusLayer::onIcon));
		b->setTag(d);
		b->setPosition({lx + d * 52.f, H - 244});
		menu->addChild(b);
	}
	m_iconLabel = mkLabel(this, "", "bigFont.fnt", .45f, {lx, H - 244});

	// Colores del bot
	auto c1 = CCMenuItemSpriteExtra::create(mkBtn("Color 1", "GJ_button_02.png", 64, .5f), this, menu_selector(VersusLayer::onColor1));
	c1->setPosition({lx - 36, H - 275});
	menu->addChild(c1);
	auto c2 = CCMenuItemSpriteExtra::create(mkBtn("Color 2", "GJ_button_02.png", 64, .5f), this, menu_selector(VersusLayer::onColor2));
	c2->setPosition({lx + 36, H - 275});
	menu->addChild(c2);

	// --- Atras ---
	auto back = CCMenuItemSpriteExtra::create(
		CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png"), this, menu_selector(VersusLayer::onBack));
	back->setPosition({win.width - 28, H - 24});
	menu->addChild(back);

	// --- Panel derecho: dificultad, fuente y modos ---
	float rx = 385.f;
	for (int i = 0; i <= 10; i++) {
		auto spr = CCSprite::createWithSpriteFrameName(vs::faceFrame(i).c_str());
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(VersusLayer::onFace));
		btn->setTag(i);
		if (i <= 5) btn->setPosition({rx + (i - 2.5f) * 50.f, H - 62});
		else btn->setPosition({rx + (i - 6 - 2.f) * 50.f, H - 112});
		menu->addChild(btn);
		m_faceBtns.push_back(btn);
	}
	m_faceLabel = mkLabel(this, "", "bigFont.fnt", .5f, {rx, H - 150});

	auto tog = CCMenuItemToggler::create(
		mkBtn("Del juego", "GJ_button_02.png"), mkBtn("Online", "GJ_button_01.png"),
		this, menu_selector(VersusLayer::onSource));
	tog->toggle(true);
	tog->setPosition({rx, H - 185});
	menu->addChild(tog);

	auto start = CCMenuItemSpriteExtra::create(mkBtn("Comenzar vs Bot", "GJ_button_01.png", 190, .8f), this, menu_selector(VersusLayer::onBot));
	start->setPosition({rx, H - 225});
	menu->addChild(start);

	auto create = CCMenuItemSpriteExtra::create(mkBtn("Crear sala", "GJ_button_03.png", 100), this, menu_selector(VersusLayer::onCreate));
	create->setPosition({rx - 70, H - 262});
	menu->addChild(create);
	auto join = CCMenuItemSpriteExtra::create(mkBtn("Unirse", "GJ_button_03.png", 100), this, menu_selector(VersusLayer::onJoin));
	join->setPosition({rx + 70, H - 262});
	menu->addChild(join);

	m_code = TextInput::create(100.f, "Codigo", "bigFont.fnt");
	m_code->setFilter("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	m_code->setMaxCharCount(4);
	m_code->setPosition({rx + 70, 22});
	this->addChild(m_code);

	m_status = mkLabel(this, "", "bigFont.fnt", .35f, {rx - 50, 22});

	this->refreshFaces();
	this->refreshBot();
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

// --- Icono y colores del bot ---
void VersusLayer::onIcon(CCObject* s) {
	auto& c = vs::botCosmetic();
	c.icon += static_cast<CCNode*>(s)->getTag();
	if (c.icon < 1) c.icon = 484;
	if (c.icon > 484) c.icon = 1;
	this->refreshBot();
}

void VersusLayer::onColor1(CCObject*) {
	auto& c = vs::botCosmetic();
	c.c1 = (c.c1 + 1) % 42;
	this->refreshBot();
}

void VersusLayer::onColor2(CCObject*) {
	auto& c = vs::botCosmetic();
	c.c2 = (c.c2 + 1) % 42;
	this->refreshBot();
}

void VersusLayer::refreshBot() {
	auto& c = vs::botCosmetic();
	paint(m_bot, c);
	m_iconLabel->setString(fmt::format("{}", c.icon).c_str());
	vs::saveBotCosmetic();
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
	if (m == vs::Mode::Bot) vs::match().opp = vs::botCosmetic();
	this->setStatus("Buscando nivel...");
	this->pickLevel();
}

void VersusLayer::pickLevel() {
	auto& m = vs::match();
	if (!m_online) {
		std::vector<int> ids;
		for (int i = 1; i <= 22; i++) if (vs::faceMatches(m_face, vs::mainFace(i))) ids.push_back(i);
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
		if (vs::faceMatches(m_face, f)) ok.push_back(lvl);
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
	vs::putCosmetic(b, vs::mine());
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
			vs::match().opp = vs::parseCosmetic(v["opp"]);
			s->unschedule(schedule_selector(VersusLayer::pollRoom));
			s->startLevel();
		}
	});
}

void VersusLayer::onJoin(CCObject*) {
	if (m_busy) return;
	std::string code = m_code->getString();
	if (code.size() != 4) return this->setStatus("Escribe el codigo de 4 letras");
	std::transform(code.begin(), code.end(), code.begin(), ::toupper);
	m_busy = true;
	vs::match() = vs::Match{};
	vs::match().mode = vs::Mode::Online;
	vs::match().room = code;
	this->setStatus("Uniendose...");
	matjson::Value b;
	b["room"] = code;
	vs::putCosmetic(b, vs::mine());
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
		m.opp = vs::parseCosmetic(v["opp"]);
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
	glm->downloadLevel(m.levelID, false, 0);
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
