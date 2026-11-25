#include "plugin.hpp"

Plugin* pluginInstance;

// --- 0. SETTINGS (global, saved to Rack's user folder) ---
enum HotkeyType {
	HOTKEY_KEY = 0,
	HOTKEY_MOUSE = 1
};

static int hotkeyType = HOTKEY_KEY;
static int hotkeyKey = GLFW_KEY_M;
static int hotkeyButton = GLFW_MOUSE_BUTTON_MIDDLE;
static int hotkeyMods = GLFW_MOD_ALT;
static bool listeningForHotkey = false;
static std::string hotkeyStatusMsg;
static bool darkPanel = true;        // true = dark panel, false = light panel
static bool closeWithHotkey = true;  // pressing the hotkey again closes the map
static bool closeWithEsc = true;     // pressing Esc closes the map

static std::string getSettingsPath() {
	return asset::user("Navigator.json");
}

static void saveHotkeySettings() {
	json_t* rootJ = json_object();
	json_object_set_new(rootJ, "type", json_integer(hotkeyType));
	json_object_set_new(rootJ, "key", json_integer(hotkeyKey));
	json_object_set_new(rootJ, "button", json_integer(hotkeyButton));
	json_object_set_new(rootJ, "mods", json_integer(hotkeyMods));
	json_object_set_new(rootJ, "darkPanel", json_boolean(darkPanel));
	json_object_set_new(rootJ, "closeWithHotkey", json_boolean(closeWithHotkey));
	json_object_set_new(rootJ, "closeWithEsc", json_boolean(closeWithEsc));
	FILE* file = fopen(getSettingsPath().c_str(), "w");
	if (file) {
		json_dumpf(rootJ, file, JSON_INDENT(2));
		fclose(file);
	}
	json_decref(rootJ);
}

static void loadHotkeySettings() {
	FILE* file = fopen(getSettingsPath().c_str(), "r");
	if (!file) return;
	json_error_t error;
	json_t* rootJ = json_loadf(file, 0, &error);
	fclose(file);
	if (!rootJ) return;

	json_t* typeJ = json_object_get(rootJ, "type");
	json_t* keyJ = json_object_get(rootJ, "key");
	json_t* buttonJ = json_object_get(rootJ, "button");
	json_t* modsJ = json_object_get(rootJ, "mods");
	json_t* darkJ = json_object_get(rootJ, "darkPanel");
	json_t* closeHotkeyJ = json_object_get(rootJ, "closeWithHotkey");
	json_t* closeEscJ = json_object_get(rootJ, "closeWithEsc");

	if (typeJ) hotkeyType = json_integer_value(typeJ);
	if (keyJ) hotkeyKey = json_integer_value(keyJ);
	if (buttonJ) hotkeyButton = json_integer_value(buttonJ);
	if (modsJ) hotkeyMods = json_integer_value(modsJ) & RACK_MOD_MASK;
	if (darkJ) darkPanel = json_is_true(darkJ);
	if (closeHotkeyJ) closeWithHotkey = json_is_true(closeHotkeyJ);
	if (closeEscJ) closeWithEsc = json_is_true(closeEscJ);
	json_decref(rootJ);
}

static bool isModifierKey(int key) {
	return key >= GLFW_KEY_LEFT_SHIFT && key <= GLFW_KEY_MENU;
}

static std::string getKeyDisplayName(int key) {
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return std::string(1, (char) key);
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return std::string(1, (char) key);
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
	switch (key) {
		case GLFW_KEY_SPACE: return "Space";
		case GLFW_KEY_TAB: return "Tab";
		case GLFW_KEY_ENTER: return "Enter";
		case GLFW_KEY_BACKSPACE: return "Backspace";
		case GLFW_KEY_DELETE: return "Delete";
		case GLFW_KEY_INSERT: return "Insert";
		case GLFW_KEY_HOME: return "Home";
		case GLFW_KEY_END: return "End";
		case GLFW_KEY_PAGE_UP: return "PageUp";
		case GLFW_KEY_PAGE_DOWN: return "PageDown";
		case GLFW_KEY_UP: return "Up";
		case GLFW_KEY_DOWN: return "Down";
		case GLFW_KEY_LEFT: return "Left";
		case GLFW_KEY_RIGHT: return "Right";
		case GLFW_KEY_COMMA: return ",";
		case GLFW_KEY_PERIOD: return ".";
		case GLFW_KEY_SLASH: return "/";
		case GLFW_KEY_SEMICOLON: return ";";
		case GLFW_KEY_APOSTROPHE: return "'";
		case GLFW_KEY_LEFT_BRACKET: return "[";
		case GLFW_KEY_RIGHT_BRACKET: return "]";
		case GLFW_KEY_BACKSLASH: return "\\";
		case GLFW_KEY_MINUS: return "-";
		case GLFW_KEY_EQUAL: return "=";
		case GLFW_KEY_GRAVE_ACCENT: return "`";
	}
	return "Key" + std::to_string(key);
}

static std::string getButtonDisplayName(int button) {
	if (button == GLFW_MOUSE_BUTTON_MIDDLE) return "Middle Mouse";
	return "Mouse " + std::to_string(button + 1);
}

static std::string getHotkeyName() {
	std::string s;
	if (hotkeyMods & GLFW_MOD_CONTROL) s += "Ctrl+";
#if defined ARCH_MAC
	if (hotkeyMods & GLFW_MOD_ALT) s += "Option+";
#else
	if (hotkeyMods & GLFW_MOD_ALT) s += "Alt+";
#endif
	if (hotkeyMods & GLFW_MOD_SHIFT) s += "Shift+";
#if defined ARCH_MAC
	if (hotkeyMods & GLFW_MOD_SUPER) s += "Cmd+";
#else
	if (hotkeyMods & GLFW_MOD_SUPER) s += "Win+";
#endif
	if (hotkeyType == HOTKEY_MOUSE) return s + getButtonDisplayName(hotkeyButton);
	return s + getKeyDisplayName(hotkeyKey);
}

static void startHotkeyCapture() {
	listeningForHotkey = true;
	hotkeyStatusMsg = "Press a key or middle/side mouse button. Esc to cancel.";
}

static void setHotkeyKey(int key, int mods) {
	hotkeyType = HOTKEY_KEY;
	hotkeyKey = key;
	hotkeyMods = mods;
	saveHotkeySettings();
}

static void setHotkeyMouse(int button, int mods) {
	hotkeyType = HOTKEY_MOUSE;
	hotkeyButton = button;
	hotkeyMods = mods;
	saveHotkeySettings();
}

static void setDarkPanel(bool dark) {
	darkPanel = dark;
	saveHotkeySettings();
}

static void setCloseWithHotkey(bool v) {
	closeWithHotkey = v;
	saveHotkeySettings();
}

static void setCloseWithEsc(bool v) {
	closeWithEsc = v;
	saveHotkeySettings();
}

// Don't fire a plain-key hotkey while the user is typing in a text box
// (module browser search, rename fields, etc.)
static bool isTypingInTextField() {
	if (!APP || !APP->event) return false;
	return dynamic_cast<ui::TextField*>(APP->event->selectedWidget) != nullptr;
}

// --- 1. THE POP-OUT MINIMAP WIDGET ---
struct NavigatorOverlay : widget::OpaqueWidget {
	math::Vec worldMin;
	math::Vec worldMax;
	float scale = 0.05f;
	float padding = 10.f;

	NavigatorOverlay() {
		box.size = math::Vec(320.f, 200.f);
		// Start hidden. New widgets are visible by default, which made the
		// first hotkey press "toggle" the map closed instead of open.
		visible = false;
	}

	void updateBounds() {
		if (!APP->scene || !APP->scene->rack) return;

		math::Rect bounds = APP->scene->rack->getModuleContainer()->getChildrenBoundingBox();
		if (!bounds.size.isFinite() || bounds.size.x <= 0.f || bounds.size.y <= 0.f) {
			bounds = math::Rect(math::Vec(0.f, 0.f), math::Vec(2000.f, 1000.f));
		}

		worldMin = bounds.pos.minus(math::Vec(300.f, 300.f));
		worldMax = bounds.getBottomRight().plus(math::Vec(300.f, 300.f));
		math::Vec worldSize = worldMax.minus(worldMin);

		float availW = box.size.x - padding * 2.f;
		float availH = box.size.y - padding * 2.f;
		scale = std::min(availW / worldSize.x, availH / worldSize.y);
	}

	void draw(const DrawArgs& args) override {
		updateBounds();

		// Background & border
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 6.f);
		nvgFillColor(args.vg, nvgRGBA(24, 24, 28, 240));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(200, 200, 210, 180));
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStroke(args.vg);

		if (!APP->scene || !APP->scene->rack) return;

		// Modules
		for (widget::Widget* w : APP->scene->rack->getModuleContainer()->children) {
			math::Vec relPos = w->box.pos.minus(worldMin).mult(scale);
			math::Vec relSize = w->box.size.mult(scale);

			nvgBeginPath(args.vg);
			nvgRect(args.vg, padding + relPos.x, padding + relPos.y, std::max(2.f, relSize.x), std::max(2.f, relSize.y));
			nvgFillColor(args.vg, nvgRGBA(90, 170, 255, 200));
			nvgFill(args.vg);
		}

		// Current camera viewport box
		app::RackScrollWidget* scroll = APP->scene->rackScroll;
		if (scroll) {
			float zoom = scroll->getZoom();
			math::Vec viewPos = scroll->offset.div(zoom);
			math::Vec viewSize = scroll->box.size.div(zoom);

			math::Vec relViewPos = viewPos.minus(worldMin).mult(scale);
			math::Vec relViewSize = viewSize.mult(scale);

			nvgBeginPath(args.vg);
			nvgRect(args.vg, padding + relViewPos.x, padding + relViewPos.y, relViewSize.x, relViewSize.y);
			nvgStrokeColor(args.vg, nvgRGBA(255, 220, 80, 255));
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStroke(args.vg);
		}
	}

	void onHoverKey(const HoverKeyEvent& e) override {
		// While the map is open and the mouse is over it, handle the close keys here
		if (e.action == GLFW_PRESS) {
			if (closeWithEsc && e.key == GLFW_KEY_ESCAPE) {
				hide();
				e.consume(this);
				return;
			}
			if (closeWithHotkey
				&& hotkeyType == HOTKEY_KEY
				&& e.key == hotkeyKey
				&& (e.mods & RACK_MOD_MASK) == hotkeyMods) {
				hide();
				e.consume(this);
				return;
			}
		}
		OpaqueWidget::onHoverKey(e);
	}

	void onButton(const ButtonEvent& e) override {
		// Mouse-button hotkey pressed again while over the map: close it
		if (closeWithHotkey
			&& hotkeyType == HOTKEY_MOUSE
			&& e.action == GLFW_PRESS
			&& e.button == hotkeyButton
			&& (e.mods & RACK_MOD_MASK) == hotkeyMods) {
			hide();
			e.consume(this);
			return;
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			math::Vec clickRel = e.pos.minus(math::Vec(padding, padding)).div(scale);
			math::Vec targetWorld = worldMin.plus(clickRel);

			app::RackScrollWidget* scroll = APP->scene->rackScroll;
			if (scroll) {
				float zoom = scroll->getZoom();
				math::Vec viewSize = scroll->box.size.div(zoom);
				scroll->offset = targetWorld.minus(viewSize.div(2.f)).mult(zoom);
			}

			hide();
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
};

// --- 2. FULL-SCREEN HOTKEY LISTENER ---
struct GlobalHotkeyListener : widget::Widget {
	NavigatorOverlay* overlay = nullptr;

	// Used so a middle-mouse DRAG (panning the rack) doesn't also open the map
	bool mousePending = false;
	math::Vec mousePressPos;

	void step() override {
		// Keep listener covering the entire Rack window so events always arrive
		if (parent) {
			box.pos = math::Vec(0.f, 0.f);
			box.size = parent->box.size;
		}
		Widget::step();
	}

	void toggleOverlay() {
		// If "close with hotkey" is off, the hotkey only opens the map
		if (overlay && overlay->isVisible() && !closeWithHotkey) return;

		if (!overlay) {
			overlay = new NavigatorOverlay();   // created hidden
			APP->scene->addChild(overlay);
		}

		if (overlay->isVisible()) {
			overlay->hide();
		} else {
			math::Vec mouse = APP->scene->mousePos;
			math::Vec pos = mouse.minus(overlay->box.size.div(2.f));
			math::Rect safeBounds(math::Vec(10.f, 10.f), APP->scene->box.size.minus(overlay->box.size).minus(math::Vec(20.f, 20.f)));
			overlay->box.pos = pos.clamp(safeBounds);
			overlay->show();
		}
	}

	void draw(const DrawArgs& args) override {
		// Show a banner while waiting for the user to press a new hotkey
		if (listeningForHotkey) {
			float w = 440.f;
			float h = 36.f;
			float x = (box.size.x - w) / 2.f;
			float y = 50.f;

			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, x, y, w, h, 6.f);
			nvgFillColor(args.vg, nvgRGBA(20, 20, 24, 235));
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, nvgRGBA(255, 220, 80, 255));
			nvgStrokeWidth(args.vg, 1.5f);
			nvgStroke(args.vg);

			nvgFontSize(args.vg, 14.f);
			nvgFontFaceId(args.vg, APP->window->uiFont->handle);
			nvgFillColor(args.vg, nvgRGB(255, 255, 255));
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, x + w / 2.f, y + h / 2.f, hotkeyStatusMsg.c_str(), NULL);
		}
		Widget::draw(args);
	}

	void onHoverKey(const HoverKeyEvent& e) override {
		// --- Capture mode: the next key becomes the new hotkey ---
		if (listeningForHotkey) {
			if (e.action == GLFW_PRESS) {
				if (isModifierKey(e.key)) {
					// Ignore pure modifier presses, wait for the real key
				}
				else if (e.key == GLFW_KEY_ESCAPE) {
					listeningForHotkey = false;
				}
				else {
					setHotkeyKey(e.key, e.mods & RACK_MOD_MASK);
					listeningForHotkey = false;
				}
			}
			e.consume(this);
			return;
		}

		// --- Esc closes the map when it is open ---
		if (closeWithEsc
			&& e.action == GLFW_PRESS
			&& e.key == GLFW_KEY_ESCAPE
			&& overlay && overlay->isVisible()) {
			overlay->hide();
			e.consume(this);
			return;
		}

		// --- Normal mode: trigger on the configured key ---
		if (hotkeyType == HOTKEY_KEY
			&& e.action == GLFW_PRESS
			&& e.key == hotkeyKey
			&& (e.mods & RACK_MOD_MASK) == hotkeyMods
			&& !isTypingInTextField()) {
			toggleOverlay();
			e.consume(this);
			return;
		}
		Widget::onHoverKey(e);
	}

	void onButton(const ButtonEvent& e) override {
		// --- Capture mode: a middle/side mouse button becomes the new hotkey ---
		if (listeningForHotkey) {
			if (e.action == GLFW_PRESS && e.button >= GLFW_MOUSE_BUTTON_MIDDLE) {
				setHotkeyMouse(e.button, e.mods & RACK_MOD_MASK);
				listeningForHotkey = false;
				e.consume(this);
				return;
			}
			Widget::onButton(e);
			return;
		}

		// --- Click outside the open map closes it ---
		// Clicks inside the map are handled (and consumed) by the map itself,
		// so only clicks outside it reach this point.
		if (overlay && overlay->isVisible()
			&& e.action == GLFW_PRESS
			&& !overlay->box.contains(e.pos)) {
			overlay->hide();
			mousePending = false;   // don't let a mouse hotkey reopen it on release
			e.consume(this);
			return;
		}

		// --- Normal mode: trigger on the configured mouse button ---
		if (hotkeyType == HOTKEY_MOUSE && e.button == hotkeyButton) {
			if (e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == hotkeyMods) {
				// Remember where the press happened; decide on release.
				// Not consumed, so Rack's normal middle-drag panning still works.
				mousePending = true;
				mousePressPos = e.pos;
			}
			else if (e.action == GLFW_RELEASE && mousePending) {
				mousePending = false;
				// Only count as a click if the mouse barely moved (not a pan-drag)
				if (e.pos.minus(mousePressPos).norm() < 4.f) {
					toggleOverlay();
				}
			}
		}
		Widget::onButton(e);
	}
};

static bool listenerInstalled = false;

static void ensureListenerInstalled() {
	if (!listenerInstalled && APP && APP->scene) {
		APP->scene->addChild(new GlobalHotkeyListener());
		listenerInstalled = true;
	}
}

// --- 3. LIGHTWEIGHT MODULE & PANEL ---
struct NavigatorModule : Module {
	NavigatorModule() {
		config(0, 0, 0, 0);
	}
};

struct NavigatorWidget : ModuleWidget {
	// Panel images (15.24 x 15.24 mm, i.e. the full width of the 3HP panel)
	widget::SvgWidget* logoDark = nullptr;   // white artwork, shown on the dark panel
	widget::SvgWidget* logoLight = nullptr;  // black artwork, shown on the light panel

	// Gap between the bottom edge of the panel and the image, in pixels.
	// 0 = image sits flush against the bottom. Increase to lift it up.
	static constexpr float logoBottomMargin = 0.f;

	NavigatorWidget(NavigatorModule* module) {
		setModule(module);
		// Standard 3HP width (15px * 3 = 45px) x 380px height
		box.size = math::Vec(RACK_GRID_WIDTH * 3, RACK_GRID_HEIGHT);
		ensureListenerInstalled();

		logoDark = new widget::SvgWidget;
		logoDark->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/logo-dark.svg")));
		logoDark->box.pos = math::Vec(
			(box.size.x - logoDark->box.size.x) / 2.f,
			box.size.y - logoDark->box.size.y - logoBottomMargin);
		addChild(logoDark);

		logoLight = new widget::SvgWidget;
		logoLight->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/logo-light.svg")));
		logoLight->box.pos = math::Vec(
			(box.size.x - logoLight->box.size.x) / 2.f,
			box.size.y - logoLight->box.size.y - logoBottomMargin);
		addChild(logoLight);
	}

	void step() override {
		// Show only the image that matches the current theme
		if (logoDark) logoDark->visible = darkPanel;
		if (logoLight) logoLight->visible = !darkPanel;
		ModuleWidget::step();
	}

	void draw(const DrawArgs& args) override {
		// Panel colors depend on the selected theme
		NVGcolor fillColor   = darkPanel ? nvgRGB(30, 32, 38)    : nvgRGB(232, 234, 238);
		NVGcolor borderColor = darkPanel ? nvgRGB(70, 75, 85)    : nvgRGB(150, 155, 165);
		NVGcolor textColor   = darkPanel ? nvgRGB(255, 255, 255) : nvgRGB(0, 0, 0);

		// Panel background
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, fillColor);
		nvgFill(args.vg);

		// Panel border
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgStrokeColor(args.vg, borderColor);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// "Navigator" written down the panel (rotated 90 degrees, reads top to bottom)
		std::shared_ptr<window::Font> font = APP->window->uiFont;
		if (font) {
			nvgSave(args.vg);
			nvgTranslate(args.vg, box.size.x / 2.f, box.size.y / 2.f);
			nvgRotate(args.vg, M_PI / 2.f);
			nvgFontSize(args.vg, 24.f);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 4.f);
			nvgFillColor(args.vg, textColor);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, 0.f, 0.f, "Navigator", NULL);
			nvgRestore(args.vg);
		}

		// Draws the child widgets, including the panel image
		ModuleWidget::draw(args);
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Panel theme"));
		menu->addChild(createCheckMenuItem("Dark panel", "",
			[]() { return darkPanel; },
			[]() { setDarkPanel(true); }
		));
		menu->addChild(createCheckMenuItem("Light panel", "",
			[]() { return !darkPanel; },
			[]() { setDarkPanel(false); }
		));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Hotkey: " + getHotkeyName()));
		menu->addChild(createMenuItem("Change hotkey...", "", []() {
			startHotkeyCapture();
		}));
		menu->addChild(createMenuItem("Quick set: M key", "", []() {
			setHotkeyKey(GLFW_KEY_M, 0);
		}));
		menu->addChild(createMenuItem("Quick set: Middle mouse button", "", []() {
			setHotkeyMouse(GLFW_MOUSE_BUTTON_MIDDLE, 0);
		}));
		menu->addChild(createMenuItem("Reset hotkey to Alt+M", "", []() {
			setHotkeyKey(GLFW_KEY_M, GLFW_MOD_ALT);
		}));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Closing the map"));
		menu->addChild(createCheckMenuItem("Hotkey again closes map", "",
			[]() { return closeWithHotkey; },
			[]() { setCloseWithHotkey(!closeWithHotkey); }
		));
		menu->addChild(createCheckMenuItem("Esc closes map", "",
			[]() { return closeWithEsc; },
			[]() { setCloseWithEsc(!closeWithEsc); }
		));
	}
};

Model* modelNavigator = createModel<NavigatorModule, NavigatorWidget>("Navigator");

void init(Plugin* p) {
	pluginInstance = p;
	loadHotkeySettings();
	p->addModel(modelNavigator);
}
