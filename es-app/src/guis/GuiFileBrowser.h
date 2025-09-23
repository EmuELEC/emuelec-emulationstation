#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;
#ifdef _ENABLEEMUELEC
#include <memory>
class ImageComponent;
class TextureResource;
class BusyComponent;
#include <vector>
#endif
class GuiFileBrowser : public GuiComponent
{
public:
	enum FileTypes
	{
		IMAGES = 1,
		MANUALS = 2,
		VIDEO = 4,
		DIRECTORY = 8,
		FILES = 16,
#ifdef _ENABLEEMUELEC
		AUDIO = 32,
#endif		
		ALL = 255
	};

	GuiFileBrowser(Window* window, const std::string startPath, const std::string selectedFile, FileTypes types = FileTypes::IMAGES, const std::function<void(const std::string&)>& okCallback = nullptr, const std::string& title = "");

#ifdef _ENABLEEMUELEC        
		~GuiFileBrowser() override;
#endif
	bool input(InputConfig* config, Input input) override;
#ifdef _ENABLEEMUELEC
        void update(int deltaTime) override;
#endif		
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
        void onOk(const std::string& path);
        void navigateTo(const std::string path);
        void centerWindow();
#ifdef _ENABLEEMUELEC
        void generateVideoPreview(const std::string& path);
        void clearVideoPreview();
#endif
        MenuComponent mMenu;
        std::string mCurrentPath;
        std::string mSelectedFile;
       FileTypes   mTypes;
#ifdef _ENABLEEMUELEC
       std::shared_ptr<ImageComponent> mPreview;
       std::shared_ptr<BusyComponent> mLoading;
       std::shared_ptr<ImageComponent> mLoadingBg;
       std::vector<std::string> mVideoFrames;
       std::vector<std::shared_ptr<TextureResource>> mFrameTextures;
        int mCurrentFrame;
        int mFrameTime;
        std::string mTempPreviewDir;
       bool mGeneratingPreview;
       int mExpectedFrames;
       int mLastFrameCount;
       int mNoFrameTime;
#endif
        std::function<void(const std::string&)> mOkCallback;
};
