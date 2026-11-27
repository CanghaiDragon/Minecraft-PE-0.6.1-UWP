#include "ChatScreen.h"
#include "../Gui.h"
#include "../../Minecraft.h"
#include "../../User.h"
#include "../../../locale/I18n.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../network/packet/ChatPacket.h"
#include "../../../network/RakNetInstance.h"

ChatScreen::ChatScreen()
	: bCancel(NULL), bSend(NULL), messageBox(20, I18n::get("gui.chat.input"))
{
}

ChatScreen::~ChatScreen()
{
	delete bCancel;
	delete bSend;
}

void ChatScreen::init() {
	if (minecraft->options.javaUi) {
		bCancel = new Button(21, I18n::get("gui.cancel"));
		bSend = new Button(22, I18n::get("gui.chat.send"));
	} else {
		bCancel = new Touch::TButton(21, I18n::get("gui.cancel"));
		bSend = new Touch::TButton(22, I18n::get("gui.chat.send"));
	}
	buttons.push_back(bCancel);
	buttons.push_back(bSend);
	tabButtons.push_back(bCancel);
	tabButtons.push_back(bSend);
	textBoxes.push_back(&messageBox);
	messageBox.maxLength = ChatPacket::MAX_CHAT_LENGTH;
	messageBox.setFocus(minecraft);
}

void ChatScreen::removed()
{
	messageBox.loseFocus(minecraft);
	if (minecraft && minecraft->platform())
		minecraft->platform()->hideKeyboard();
}

void ChatScreen::setupPositions()
{
	const int buttonWidth = width >= 240 ? 64 : 52;
	const int rowY = height - 28;
	bCancel->x = 4;
	bCancel->y = rowY;
	bCancel->width = buttonWidth;
	bCancel->height = 24;
	bSend->x = width - buttonWidth - 4;
	bSend->y = rowY;
	bSend->width = buttonWidth;
	bSend->height = 24;
	messageBox.x = buttonWidth + 12;
	messageBox.y = rowY;
	messageBox.width = width - 2 * buttonWidth - 24;
	if (messageBox.width < 40) messageBox.width = 40;
	messageBox.height = 24;
}

void ChatScreen::render(int xm, int ym, float a)
{
	minecraft->gui.renderChatMessages(height - 28, 10, true, minecraft->font);
	Screen::render(xm, ym, a);
}

void ChatScreen::submitMessage()
{
	const std::string message = messageBox.text;
	if (message.empty()) {
		minecraft->setScreen(NULL);
		return;
	}

	if (minecraft->isOnline()) {
		if (minecraft->raknetInstance->isServer()) {
			const std::string playerName = minecraft->user
				? minecraft->user->name : minecraft->options.username;
			const std::string formatted = playerName + ": " + message;
			minecraft->gui.addMessage(formatted);
			ChatPacket packet(formatted, false);
			minecraft->raknetInstance->send(packet);
		} else {
			ChatPacket packet(message, false);
			minecraft->raknetInstance->send(packet);
		}
	} else {
		minecraft->gui.addMessage(message);
	}
	minecraft->setScreen(NULL);
}

void ChatScreen::buttonClicked(Button* button)
{
	if (button == bSend)
		submitMessage();
	else if (button == bCancel)
		minecraft->setScreen(NULL);
}

void ChatScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE) {
		minecraft->setScreen(NULL);
		return;
	}
	if (eventKey == Keyboard::KEY_RETURN) {
		submitMessage();
		return;
	}
	Screen::keyPressed(eventKey);
}

bool ChatScreen::isMessageInputFocused() const
{
	return messageBox.focused;
}

const std::string& ChatScreen::messageText() const
{
	return messageBox.text;
}

void ChatScreen::setMessageText(const std::string& text)
{
	// ChatPacket's historical limit is measured in bytes. Keep that wire limit,
	// but never leave a partial UTF-8 sequence at the end of the input.
	size_t end = text.length();
	if (end > messageBox.maxLength)
	{
		end = messageBox.maxLength;
		while (end > 0 && end < text.length() &&
			(static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
			--end;
	}
	messageBox.text.assign(text, 0, end);
}

bool ChatScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
		minecraft->setScreen(NULL);
	return true;
}

bool ChatScreen::isPauseScreen() { return false; }

bool ChatScreen::isInGameScreen() { return false; }

bool ChatScreen::shouldSleepAfterRender() { return false; }
