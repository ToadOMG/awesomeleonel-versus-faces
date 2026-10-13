#pragma once
#include "State.hpp"

class VersusLayer : public CCLayer, public LevelManagerDelegate, public LevelDownloadDelegate {
protected:
	int m_face = 0;
	bool m_online = true;     // fuente de niveles: online / del juego
	bool m_busy = false;
	bool m_polling = false;
	int m_attempts = 0;
	vs::Mode m_mode = vs::Mode::None;
	std::vector<CCMenuItemSpriteExtra*> m_faceBtns;
	CCLabelBMFont* m_status = nullptr;
	CCLabelBMFont* m_faceLabel = nullptr;
	CCLabelBMFont* m_iconLabel = nullptr;
	SimplePlayer* m_bot = nullptr;
	TextInput* m_code = nullptr;

	bool init() override;
	~VersusLayer() override;
	void keyBackClicked() override { this->onBack(nullptr); }

	void onBack(CCObject*);
	void onFace(CCObject*);
	void onSource(CCObject*);
	void onBot(CCObject*);
	void onCreate(CCObject*);
	void onJoin(CCObject*);
	void onIcon(CCObject*);
	void onColor1(CCObject*);
	void onColor2(CCObject*);

	void refreshFaces();
	void refreshBot();
	void setStatus(std::string const& s);
	void begin(vs::Mode m);
	void pickLevel();
	void searchOnline();
	void afterPick();
	void pollRoom(float);
	void play(GJGameLevel* lvl);

public:
	static VersusLayer* create();
	static CCScene* scene();
	void fail(std::string const& msg);
	void startLevel();

	// LevelManagerDelegate
	virtual void loadLevelsFinished(CCArray* levels, char const* key) { this->onLevels(levels); }
	virtual void loadLevelsFinished(CCArray* levels, char const* key, int) { this->onLevels(levels); }
	virtual void loadLevelsFailed(char const* key) { this->fail("Error buscando niveles online"); }
	virtual void loadLevelsFailed(char const* key, int) { this->fail("Error buscando niveles online"); }
	virtual void setupPageInfo(gd::string, char const*) {}
	// LevelDownloadDelegate
	virtual void levelDownloadFinished(GJGameLevel* lvl) { this->play(lvl); }
	virtual void levelDownloadFailed(int) { this->fail("No se pudo descargar el nivel"); }

	void onLevels(CCArray* levels);
};
