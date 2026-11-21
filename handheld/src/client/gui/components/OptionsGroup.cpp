#include "OptionsGroup.h"
#include "../../Minecraft.h"
#include "Button.h"
#include "ImageButton.h"
#include "OptionsItem.h"
#include "../screens/OptionsScreen.h"
#include "Slider.h"
#include "TextBox.h"
#include "../../../locale/I18n.h"
#include "../../../platform/input/Mouse.h"
#include "../../sound/SoundEngine.h"
#if defined(MCPE_UWP)
#include "../../../platform/uwp/UwpOriginalClientBridge.h"
#endif

namespace {
class OptionsDescription : public GuiElement {
public:
	OptionsDescription(const std::string& text) : GuiElement(false, true, 0, 0, 24, 18), text(text) {}
	void render(Minecraft* minecraft, int, int) {
		int tx = x + (width - minecraft->font->width(text)) / 2;
		minecraft->font->draw(text, (float)tx, (float)y + 4, 0xb0b0b0, false);
	}
private:
	std::string text;
};

class ClearSkinCacheButton : public Touch::TButton {
public:
	ClearSkinCacheButton() : Touch::TButton(0, I18n::get("options.clear")) {}
	virtual void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {
		if (buttonNum == MouseAction::ACTION_LEFT && clicked(minecraft, x, y)) {
			minecraft->options.clearImportedSkin();
			minecraft->soundEngine->playUI("random.click", 1, 1);
		}
	}
};

class ImportSkinButton : public Touch::TButton {
public:
	ImportSkinButton() : Touch::TButton(0, I18n::get("options.import")) {}
	virtual void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {
		if (buttonNum == MouseAction::ACTION_LEFT && clicked(minecraft, x, y)) {
			minecraft->options.importSkinFromFile();
			minecraft->soundEngine->playUI("random.click", 1, 1);
		}
	}
};

class KeyMappingButton : public Touch::TButton {
public:
	KeyMappingButton() : Touch::TButton(0, I18n::get("options.configure")) {}
	virtual void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {
		if (buttonNum == MouseAction::ACTION_LEFT && clicked(minecraft, x, y)) {
#if defined(MCPE_UWP)
			UwpOriginalClientRequestKeyMapping();
#endif
			if (minecraft != nullptr && minecraft->soundEngine != nullptr)
				minecraft->soundEngine->playUI("random.click", 1, 1);
		}
	}
};

class RestoreDefaultsButton : public Touch::TButton {
public:
	RestoreDefaultsButton() : Touch::TButton(0, I18n::get("options.apply")) {}
	virtual void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {
		if (buttonNum == MouseAction::ACTION_LEFT && clicked(minecraft, x, y)) {
			if (minecraft != nullptr)
				minecraft->requestOptionsReset();
			if (minecraft != nullptr && minecraft->soundEngine != nullptr)
				minecraft->soundEngine->playUI("random.click", 1, 1);
		}
	}
};

#if defined(MCPE_UWP)
class LanguageModeButton : public Button {
public:
	LanguageModeButton(OptionsScreen* screen, const std::string& mode,
		const std::string& label)
		: Button(0, label), screen(screen), mode(mode), label(label) {}

	void render(Minecraft* minecraft, int mouseX, int mouseY) override
	{
		if (!visible)
			return;

		const bool selected = screen->getLanguageMode() == mode;
		const bool hovered = active && isInside(mouseX, mouseY);
		if (selected)
		{
			// Match the gray outer / black inner selection frame used by
			// RolledSelectionListH, and span the whole option row.
			fill(x, y, x + width, y + height, 0xff808080);
			if (width > 2 && height > 2)
				fill(x + 1, y + 1, x + width - 1, y + height - 1, 0xff000000);
		}

		const int textHeight = minecraft->font->lineHeightFor(label);
		const int textColor = (selected || hovered) ? 0xffffa0 : 0xe0e0e0;
		drawCenteredString(minecraft->font, label, x + width / 2,
			y + (height - textHeight) / 2, textColor);
	}

	void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) override
	{
		if (buttonNum == MouseAction::ACTION_LEFT && clicked(minecraft, x, y))
		{
			if (minecraft->soundEngine != nullptr)
				minecraft->soundEngine->playUI("random.click", 1, 1);
			screen->requestLanguageChange(mode);
		}
	}

private:
	OptionsScreen* screen;
	std::string mode;
	std::string label;
};
#endif
}

OptionsGroup::OptionsGroup( std::string labelID )  {
	label = I18n::get(labelID);
}

void OptionsGroup::setupPositions() {
	// Leave 18px at top for the group header label
	int curY = y + 18;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->width = width - 2;
		(*it)->y = curY;
		(*it)->x = x + 1;
		(*it)->setupPositions();
		curY += (*it)->height + 1;
	}
	height = curY - y + 4;
}

void OptionsGroup::render( Minecraft* minecraft, int xm, int ym ) {
	// Group header background
	fill(x, y, x + width, y + 16, 0x99223344);
	// Header separator line
	fill(x, y + 16, x + width, y + 17, 0x884488aa);
	// Header label
	minecraft->font->draw(label, (float)x + 5, (float)y + 4, 0xffffff, false);
	super::render(minecraft, xm, ym);
}

OptionsGroup& OptionsGroup::addOptionItem( const Options::Option* option, Minecraft* minecraft ) {
	if(option->isBoolean())
		createToggle(option, minecraft);
	else if(option->isProgress())
		createProgressSlider(option, minecraft);
	else if(option->isInt())
		createStepSlider(option, minecraft);
	return *this;
}

TextBox* OptionsGroup::addTextInput(const std::string& itemLabel, const std::string& hint, const std::string& value) {
	TextBox* element = new TextBox(0, hint);
	element->width = 132;
	element->height = 22;
	element->text = value;
	addChild(new OptionsItem(itemLabel, element));
	setupPositions();
	return element;
}

OptionsGroup& OptionsGroup::addDisabledItem(const std::string& itemLabel, const std::string& value) {
	Button* element = new Button(0, value);
	element->active = false;
	element->width = 96;
	element->height = 20;
	addChild(new OptionsItem(itemLabel, element));
	setupPositions();
	return *this;
}

OptionsGroup& OptionsGroup::addKeyMappingItem(Minecraft* minecraft) {
#if !defined(MCPE_UWP)
	(void)minecraft;
	return addDisabledItem(I18n::get("options.keyMapping"), I18n::get("options.unavailable"));
#else
	KeyMappingButton* element = new KeyMappingButton();
	element->width = 96;
	element->height = 20;
	addChild(new OptionsItem(I18n::get("options.keyMapping"), element));
	setupPositions();
	return *this;
#endif
}

OptionsGroup& OptionsGroup::addDescription(const std::string& text) {
	addChild(new OptionsDescription(text));
	setupPositions();
	return *this;
}

OptionsGroup& OptionsGroup::addRestoreDefaultsItem(Minecraft* minecraft) {
	(void)minecraft;
	RestoreDefaultsButton* element = new RestoreDefaultsButton();
	element->width = 70;
	element->height = 20;
	addChild(new OptionsItem(I18n::get("options.restoreDefaults"), element));
	setupPositions();
	return *this;
}

OptionsGroup& OptionsGroup::addClearSkinCacheItem(Minecraft* minecraft) {
	ClearSkinCacheButton* element = new ClearSkinCacheButton();
	element->width = 70;
	element->height = 20;
	addChild(new OptionsItem(I18n::get("options.clearSkinCache"), element));
	setupPositions();
	return *this;
}

OptionsGroup& OptionsGroup::addImportSkinItem(Minecraft* minecraft) {
	ImportSkinButton* element = new ImportSkinButton();
	element->width = 70;
	element->height = 20;
	addChild(new OptionsItem(I18n::get("options.importSkin"), element));
	setupPositions();
	return *this;
}

OptionsGroup& OptionsGroup::addLanguageModes(Minecraft* minecraft,
	OptionsScreen* screen) {
#if defined(MCPE_UWP)
	(void)minecraft;
	static const char* modeKeys[][2] = {
		{ "origin", "options.language.mode.origin" },
		{ "system", "options.language.mode.system" },
		{ "en_US", "options.language.mode.english" },
		{ "zh_CN", "options.language.mode.simplifiedChinese" }
	};
	for (size_t i = 0; i < sizeof(modeKeys) / sizeof(modeKeys[0]); ++i)
	{
		LanguageModeButton* element = new LanguageModeButton(
			screen, modeKeys[i][0], I18n::get(modeKeys[i][1]));
		element->width = 160;
		element->height = 22;
		addChild(element);
	}
	setupPositions();
#else
	(void)minecraft;
	(void)screen;
#endif
	return *this;
}

void OptionsGroup::createToggle( const Options::Option* option, Minecraft* minecraft ) {
	ImageDef def;
	// The option/toggle icon row in touchguinew.png is at the bottom.
	def.setSrc(IntRectangle(160, 235, 39, 20));
	def.name = "gui/touchgui.png";
	def.width = 39 * 0.7f;
	def.height = 20 * 0.7f;
	OptionButton* element = new OptionButton(option);
	element->setImageDef(def, true);
	element->updateImage(&minecraft->options);
	std::string itemLabel = I18n::get(option->getCaptionId());
	OptionsItem* item = new OptionsItem(itemLabel, element);
	addChild(item);
	setupPositions();
}

void OptionsGroup::createProgressSlider( const Options::Option* option, Minecraft* minecraft ) {
	Slider* element = new Slider(minecraft,
								option,
								minecraft->options.getProgrssMin(option),
								minecraft->options.getProgrssMax(option));
	// The two Input Mode labels are deliberately descriptive; give this
	// selector enough room without shrinking its slider to an unusable size.
	element->width = (option == &Options::Option::INPUT_MODE) ? 160 : 120;
	element->height = 24;
	std::string itemLabel = I18n::get(option->getCaptionId());
	OptionsItem* item = new OptionsItem(itemLabel, element);
	addChild(item);
	setupPositions();
}

void OptionsGroup::createStepSlider( const Options::Option* option, Minecraft* minecraft ) {
	std::vector<int> steps;
	if (option == &Options::Option::DIFFICULTY) {
		steps.push_back(0);
		steps.push_back(2);
	} else if (option == &Options::Option::RENDER_DISTANCE) {
		steps.push_back(0); // Far
		steps.push_back(1); // Normal
		steps.push_back(2); // Short
		steps.push_back(3); // Tiny
	} else if (option == &Options::Option::GUI_SCALE) {
		steps.push_back(0);
		steps.push_back(1);
		steps.push_back(2);
		steps.push_back(3);
		steps.push_back(4);
	} else if (option == &Options::Option::DPAD_SIZE) {
		steps.push_back(0);
		steps.push_back(1);
		steps.push_back(2);
	} else if (option == &Options::Option::THIRD_PERSON) {
		steps.push_back(Options::THIRD_PERSON_FIRST);
		steps.push_back(Options::THIRD_PERSON_BACK);
		steps.push_back(Options::THIRD_PERSON_FRONT);
	} else if (option == &Options::Option::INPUT_MODE) {
		steps.push_back(0);
		steps.push_back(1);
	} else if (option == &Options::Option::SKIN_ARM_TYPE) {
		steps.push_back(0);
		steps.push_back(1);
	} else if (option == &Options::Option::GRAPHICS) {
		steps.push_back(0);
		steps.push_back(1);
	} else if (option == &Options::Option::DYNAMIC_TILE) {
		steps.push_back(Options::DYNAMIC_TILE_OFF);
		steps.push_back(Options::DYNAMIC_TILE_SIMPLE);
		steps.push_back(Options::DYNAMIC_TILE_FULL);
	}

	if (steps.size() < 2) {
		return;
	}

	Slider* element = new Slider(minecraft, option, steps);
	// Camera has three longer value labels, so it needs a wider control than
	// the compact two/three-step sliders. Otherwise its value area would be
	// wider than the control itself, leaving no draggable track.
	element->width = (option == &Options::Option::THIRD_PERSON) ? 240
		: (option == &Options::Option::DIFFICULTY ? 128 : 120);
	element->height = 24;
	std::string itemLabel = I18n::get(option->getCaptionId());
	OptionsItem* item = new OptionsItem(itemLabel, element);
	addChild(item);
	setupPositions();
}
