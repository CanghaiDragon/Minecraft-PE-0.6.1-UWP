#ifndef APPPLATFORM_UWP_H__
#define APPPLATFORM_UWP_H__

#include "AppPlatform.h"

#include <map>
#include <vector>

// UWP implementation of the original platform contract used by the first
// native-client bootstrap stages.  It owns platform policy only; it does not
// own Minecraft, the renderer, or any world state.
class AppPlatform_uwp : public AppPlatform
{
public:
	AppPlatform_uwp(int screenWidth, int screenHeight);

	BinaryBlob readAssetFile(const std::string& filename) override;
	TextureData loadTexture(const std::string& filename, bool textureFolder) override;
	int getScreenWidth() override;
	int getScreenHeight() override;
	float getPixelsPerMillimeter() override;
	bool supportsTouchscreen() override;
	std::string getPlatformStringVar(int stringId) override;
	std::string getDateString(int seconds) override;
	void openUrl(const std::string& url) override;
	void showKeyboard() override;
	void hideKeyboard() override;

	void setScreenSize(int screenWidth, int screenHeight);
	void cacheTexturePixels(const std::string& filename, bool textureFolder,
		unsigned int width, unsigned int height, const unsigned char* pixels,
		unsigned int byteCount);
	void setTexturePreloadFailure(const std::string& filename, bool textureFolder,
		const std::string& reason);
	void clearTexturePreload(const std::string& filename, bool textureFolder);
	const std::string& lastTextureLoadError() const;

private:
	struct CachedTexture
	{
		unsigned int width = 0;
		unsigned int height = 0;
		bool transparent = true;
		std::vector<unsigned char> pixels;
	};

	int m_screenWidth;
	int m_screenHeight;
	std::map<std::string, CachedTexture> m_textureCache;
	std::map<std::string, std::string> m_texturePreloadFailures;
	std::string m_lastTextureLoadError;
};

#endif // APPPLATFORM_UWP_H__
