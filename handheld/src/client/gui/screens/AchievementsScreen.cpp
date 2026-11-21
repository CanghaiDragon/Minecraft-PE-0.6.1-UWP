#include "AchievementsScreen.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "../../gui/Font.h"
#include "../Gui.h"
#include "../../renderer/Tesselator.h"
#include "../../renderer/Textures.h"
#include "../../renderer/entity/ItemRenderer.h"
#include "../../../world/item/ItemInstance.h"
#include "../../../world/item/Item.h"
#include "../../../world/level/tile/Tile.h"
#include "../../renderer/gles.h"
#include "../../../world/level/Level.h"
#include "../../../world/level/storage/LevelStorage.h"
#include "../../../world/level/storage/AchievementIds.h"
#include "../../../platform/input/Mouse.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../locale/I18n.h"
#include <string>

namespace {
	const int kNodeX[AchievementIds::Count] = { 0, 1, 2, 3, 4, 6, 2, 1, 9, 8, 1, 7, 2, 3, 5 };
	const int kNodeY[AchievementIds::Count] = { 0, 0, 0, 0, 0, 0, 4, 2, 0, 0, 4, 0, 2, 2, 0 };
	const int kParent[AchievementIds::Count] = { -1, 0, 1, 2, 3, 14, 10, 1, 9, 11, 1, 5, 7, 12, 4 };
	const char* kNameKeys[AchievementIds::Count] = {
		"achievement.pe.0.title", "achievement.pe.1.title", "achievement.pe.2.title",
		"achievement.pe.3.title", "achievement.pe.4.title", "achievement.pe.5.title",
		"achievement.pe.6.title", "achievement.pe.7.title", "achievement.pe.8.title",
		"achievement.pe.9.title", "achievement.pe.10.title", "achievement.pe.11.title",
		"achievement.pe.12.title", "achievement.pe.13.title", "achievement.pe.14.title"
	};
	const char* kDescriptionKeys[AchievementIds::Count] = {
		"achievement.pe.0.desc", "achievement.pe.1.desc", "achievement.pe.2.desc",
		"achievement.pe.3.desc", "achievement.pe.4.desc", "achievement.pe.5.desc",
		"achievement.pe.6.desc", "achievement.pe.7.desc", "achievement.pe.8.desc",
		"achievement.pe.9.desc", "achievement.pe.10.desc", "achievement.pe.11.desc",
		"achievement.pe.12.desc", "achievement.pe.13.desc", "achievement.pe.14.desc"
	};
	const int kGrid = 50;
	int achievementIconId(int i)
	{
		switch (i) {
		case 0: return Tile::chest->id;
		case 1: return Tile::workBench->id;
		case 2: return Item::pickAxe_wood->id;
		case 3: return Tile::furnace->id;
		case 4: return Item::pickAxe_stone->id;
		case 5: return Item::ironIngot->id;
		case 6: return Item::bread->id;
		case 7: return Item::sword_iron->id;
		case 8: return Item::chestplate_diamond->id;
		case 9: return Tile::netherReactor->id;
		case 10: return Item::seeds_wheat->id;
		case 11: return Item::emerald->id;
		case 12: return Item::sword_emerald->id;
		case 13: return Item::sword_gold->id;
		case 14: return Item::bed->id;
		default: return 0;
		}
	}
	const int kCanvasWidth = 514;
	const int kCanvasHeight = 262;
}

AchievementsScreen::AchievementsScreen()
	: doneButton(NULL), panX(0), panY(0), dragX(0), dragY(0), dragStartX(0), dragStartY(0), dragging(false), dragMoved(false), selectedAchievement(-1), ownsPause(false), previousPause(false)
{
}

AchievementsScreen::~AchievementsScreen()
{
	if (ownsPause && minecraft) minecraft->pause = previousPause;
	delete doneButton;
}

void AchievementsScreen::init()
{
	if (!ownsPause) {
		previousPause = minecraft->pause;
		ownsPause = true;
	}
	minecraft->pause = true;
	delete doneButton;
	buttons.clear();
	tabButtons.clear();
	if (!minecraft->options.javaUi) {
		doneButton = new Touch::TButton(1, I18n::get("achievements.back"));
		doneButton->width = 84;
		doneButton->height = 28;
	} else {
		doneButton = new Button(1, 0, 0, 100, 20, I18n::get("gui.done"));
	}
	buttons.push_back(doneButton);
	tabButtons.push_back(doneButton);
	setupPositions();
	clampPan();
}

void AchievementsScreen::setupPositions()
{
	if (doneButton == NULL) return;
	if (!minecraft->options.javaUi) {
		doneButton->x = width - doneButton->width - 12;
		doneButton->y = height - doneButton->height - 4;
	} else {
		doneButton->x = 0;
		doneButton->y = 0;
	}
	clampPan();
}

void AchievementsScreen::clampPan()
{
	const int viewW = width - 14;
	const int viewH = height - 78;
	const float minX = (float)viewW - kCanvasWidth;
	const float minY = (float)viewH - kCanvasHeight;
	if (panX < minX) panX = minX;
	if (panX > 0) panX = 0;
	if (panY < minY) panY = minY;
	if (panY > 0) panY = 0;
}

void AchievementsScreen::render(int xm, int ym, float a)
{
	(void)a;
	renderBackground();
	const int left = 7;
	const int top = 38;
	const int right = width - 7;
	const int bottom = height - 40;
	// Use the existing tiled dirt texture for the achievement panel.
	glEnable2(GL_TEXTURE_2D);
	minecraft->textures->loadAndBindTexture("gui/background.png");
	glColor4f2(1, 1, 1, 1);
	Tesselator& t = Tesselator::instance;
	const float textureScale = 32.0f;
	t.begin();
	t.color(0x404040);
	t.vertexUV((float)left, (float)bottom, 0, 0, bottom / textureScale);
	t.vertexUV((float)right, (float)bottom, 0, right / textureScale, bottom / textureScale);
	t.vertexUV((float)right, (float)top, 0, right / textureScale, top / textureScale);
	t.vertexUV((float)left, (float)top, 0, 0, top / textureScale);
	t.draw();
	int hx = -1;
	int hy = -1;
	int nodeX[AchievementIds::Count];
	int nodeY[AchievementIds::Count];
	for (int i = 0; i < AchievementIds::Count; ++i) {
		nodeX[i] = left + 14 + (int)panX + kNodeX[i] * kGrid;
		nodeY[i] = top + 16 + (int)panY + kNodeY[i] * kGrid;
	}
	// Clip the tree so nodes and connectors remain inside the framed viewport.
	const float guiScale = Gui::GuiScale;
	glEnable2(GL_SCISSOR_TEST);
	glScissor((GLint)(guiScale * left), minecraft->height - (GLint)(guiScale * bottom),
		(GLsizei)(guiScale * (right - left)), (GLsizei)(guiScale * (bottom - top)));
	// The main row runs left to right; both side branches start at Benchmarking.
	for (int i = 0; i < AchievementIds::Count; ++i) {
		if (kParent[i] < 0) continue;
		const int p = kParent[i];
		const bool unlocked = minecraft->level->getLevelStorage()->hasAchievement(i);
		const bool parentUnlocked = minecraft->level->getLevelStorage()->hasAchievement(kParent[i]);
		const int color = unlocked ? 0xff66cc66 : (parentUnlocked ? 0xff55bb55 : 0xff555555);
		const int x0 = nodeX[p] + 9, y0 = nodeY[p] + 9;
		const int x1 = nodeX[i] + 9, y1 = nodeY[i] + 9;
		const int steps = (x1 > x0 ? x1 - x0 : x0 - x1) > (y1 > y0 ? y1 - y0 : y0 - y1)
			? (x1 > x0 ? x1 - x0 : x0 - x1) : (y1 > y0 ? y1 - y0 : y0 - y1);
		for (int s = 0; s <= steps; ++s) {
			const int px = x0 + (steps ? (x1 - x0) * s / steps : 0);
			const int py = y0 + (steps ? (y1 - y0) * s / steps : 0);
			fill(px, py, px + 1, py + 1, color);
		}
	}
	const int hoverId = hoveredAchievement(xm, ym);
	const int detailId = dragging ? -1 : (hoverId >= 0 ? hoverId : selectedAchievement);
	for (int i = 0; i < AchievementIds::Count; ++i) {
		const int x = nodeX[i], y = nodeY[i];
		if (x + 22 < left || x - 2 > right || y + 22 < top || y - 2 > bottom) continue;
		const bool unlocked = minecraft->level->getLevelStorage()->hasAchievement(i);
		if (detailId == i) { hx = x; hy = y; }
		if (!unlocked)
			fill((float)x, (float)y, (float)x + 24.0f, (float)y + 24.0f, 0x99000000);
		Tesselator& slotTess = Tesselator::instance;
		slotTess.beginOverride();
		slotTess.colorABGR(0xffffffff);
		blit((float)x - 2, (float)y - 2, 200, 46, 24.0f, 24.0f, 16, 16);
		minecraft->textures->loadAndBindTexture("gui/inventory_slot.png");
		slotTess.endOverrideAndDraw();
		ItemInstance icon(achievementIconId(i), 1, 0);
		ItemRenderer::renderGuiItem(NULL, minecraft->textures, &icon, (float)x + 2, (float)y + 2, 16, 16, false);
		if (!unlocked)
			fill((float)x - 2, (float)y - 2, (float)x + 22.0f, (float)y + 22.0f, 0x88000000);
	}
	glDisable2(GL_SCISSOR_TEST);
	if (detailId >= 0 && hx >= 0) {
		const int id = detailId;
		const bool unlocked = minecraft->level->getLevelStorage()->hasAchievement(id);
		const bool hasProgress = id == AchievementIds::Overkill;
		const int tipHeight = hasProgress ? 74 : 58;
		int tipX = (hx + 24 + 210 > right) ? hx - 214 : hx + 24;
		int tipY = hy + 26 + tipHeight > bottom ? hy - tipHeight - 6 : hy + 24;
		if (tipY < top) tipY = top;
		if (tipY + tipHeight > bottom) tipY = bottom - tipHeight;
		if (tipX < left) tipX = left;
		if (tipX + 210 > right) tipX = right - 210;
		fill(tipX, tipY, tipX + 210, tipY + tipHeight, 0xee080808);
		fill(tipX, tipY, tipX + 210, tipY + 1, unlocked ? 0xffd6bd52 : 0xff888888);
		drawString(font, I18n::get(kNameKeys[id]), tipX + 7, tipY + 6, unlocked ? 0xffffff80 : 0xffaaaaaa);
		drawString(font, I18n::get(kDescriptionKeys[id]), tipX + 7, tipY + 22, 0xffdddddd);
		if (hasProgress) {
			const int progress = minecraft->level->getLevelStorage()->getOverkillMobCount();
			std::string progressText = I18n::get("achievements.progress");
			const size_t progressToken = progressText.find("%1$s");
			if (progressToken != std::string::npos)
				progressText.replace(progressToken, 4, std::to_string(progress));
			drawString(font, progressText, tipX + 7, tipY + 39, 0xffdddddd);
		}
		drawString(font, I18n::get(unlocked ? "achievements.unlocked" : "achievements.locked"), tipX + 7, tipY + (hasProgress ? 55 : 39),
			unlocked ? 0xff80ff80 : 0xff999999);
	}
	// Keep the frame border thickness fixed while fitting the current panel bounds.
	Tesselator::instance.colorABGR(0xffffffff);
	minecraft->textures->loadAndBindTexture("gui/itemframe.png");
	glEnable2(GL_BLEND);
	glColor4f2(1, 1, 1, 1);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Same source rectangle and blit path as the PE inventory frame.
	blit(0, top - 8, 0, 0, width, bottom - top + 16, 215, 256);
	glDisable2(GL_BLEND);
	drawCenteredString(font, I18n::get("achievements.title"), width / 2, 14, 0xffffffff);
	drawString(font, I18n::get("achievements.drag"),
		left + 4, bottom + 12, 0xffcccccc);
	super::render(xm, ym, a);
}

int AchievementsScreen::hoveredAchievement(int mouseX, int mouseY) const
{
	const int left = 7, top = 38, right = width - 7, bottom = height - 40;
	if (mouseX < left || mouseX >= right || mouseY < top || mouseY >= bottom) return -1;
	for (int i = 0; i < AchievementIds::Count; ++i) {
		const int x = left + 14 + (int)panX + kNodeX[i] * kGrid;
		const int y = top + 16 + (int)panY + kNodeY[i] * kGrid;
		if (mouseX >= x - 2 && mouseX < x + 22 && mouseY >= y - 2 && mouseY < y + 22) return i;
	}
	return -1;
}

void AchievementsScreen::mouseClicked(int x, int y, int buttonNum)
{
	const int left = 7, top = 38, right = width - 7, bottom = height - 40;
	if (buttonNum == MouseAction::ACTION_LEFT && x >= left && x < right && y >= top && y < bottom) {
		dragging = true; dragMoved = false;
		dragX = dragStartX = x;
		dragY = dragStartY = y;
		selectedAchievement = hoveredAchievement(x, y);
		return;
	}
	super::mouseClicked(x, y, buttonNum);
}

void AchievementsScreen::mouseReleased(int x, int y, int buttonNum)
{
	if (dragging) { dragging = false; dragMoved = false; return; }
	super::mouseReleased(x, y, buttonNum);
}

void AchievementsScreen::mouseMoved(int x, int y)
{
	if (dragging) {
		panX += x - dragX;
		panY += y - dragY;
		dragX = x;
		dragY = y;
		const int totalX = x - dragStartX;
		const int totalY = y - dragStartY;
		if (!dragMoved && totalX * totalX + totalY * totalY > 16) {
			dragMoved = true;
			selectedAchievement = -1;
		}
		clampPan();
		return;
	}
	selectedAchievement = hoveredAchievement(x, y);
}

void AchievementsScreen::mouseScrolled(int x, int y, int delta)
{
	(void)x; (void)y;
	panX += delta * 18.0f;
	clampPan();
}

bool AchievementsScreen::renderGameBehind() { return false; }

bool AchievementsScreen::handleBackEvent(bool isDown)
{
	if (isDown) minecraft->setScreen(NULL);
	return true;
}

void AchievementsScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE) { minecraft->setScreen(NULL); return; }
	super::keyPressed(eventKey);
}

void AchievementsScreen::buttonClicked(Button* button)
{
	if (button == doneButton) minecraft->setScreen(NULL);
}
