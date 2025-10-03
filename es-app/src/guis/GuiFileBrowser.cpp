#include "guis/GuiFileBrowser.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "components/ComponentGrid.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "components/MultiLineMenuEntry.h"
#include "guis/GuiMsgBox.h"
#include <cstring>
#include "SystemConf.h"
#include "Paths.h"
#ifdef _ENABLEEMUELEC
#include "components/ImageComponent.h"
#include "components/BusyComponent.h"
#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include <algorithm>
#endif

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.65f))

#define DRIVE_ICON		_U("\uF0A0 ")
#define FOLDER_ICON		_U("\uF07C ")
#define IMAGE_ICON		_U("\uF03E ")
#define VIDEO_ICON		_U("\uF03D ")
#define DOCUMENT_ICON	_U("\uF02D ")
#define AUDIO_ICON _U("\uF028 ")



GuiFileBrowser::GuiFileBrowser(Window* window, const std::string startPath, const std::string selectedFile, FileTypes types, const std::function<void(const std::string&)>& okCallback, const std::string& title)
    : GuiComponent(window), mMenu(window, title.empty() ? _("FILE BROWSER") : title)
{
	setTag("popup");

	mTypes = types;
	mSelectedFile = Utils::FileSystem::getCanonicalPath(selectedFile);
	mOkCallback = okCallback;

    addChild(&mMenu);
#ifdef _ENABLEEMUELEC
       mPreview = std::make_shared<ImageComponent>(window);
       mPreview->setVisible(false);
       mPreview->setAllowFading(false);
       addChild(mPreview.get());
       mLoadingBg = std::make_shared<ImageComponent>(window);
       mLoadingBg->setVisible(false);
       mLoadingBg->setImage(":/white.png");
       mLoadingBg->setColorShift(0x000000FF);
       addChild(mLoadingBg.get());
       mLoading = std::make_shared<BusyComponent>(window);
       mLoading->setVisible(false);
       mLoading->setBackgroundVisible(false);
       addChild(mLoading.get());
       mCurrentFrame = 0;
       mFrameTime = 0;
       mGeneratingPreview = false;
       mExpectedFrames = 0;
       mLastFrameCount = 0;
       mNoFrameTime = 0;
       mMenu.getList()->setCursorChangedCallback([this](CursorState state)
       {
               if (mMenu.size() == 0)
               {
                       mPreview->setImage("");
                       mPreview->setVisible(false);
                       clearVideoPreview();
                       return;
               }
               std::string path = mMenu.getSelected();
               std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(path));
               if (ext == ".jpg" || ext == ".png" || ext == ".gif" || ext == ".svg")
               {
                       clearVideoPreview();
                       mPreview->setImage(path);
                       mPreview->setVisible(true);
               }
               else if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".webm")
               {
                       generateVideoPreview(path);
               }
               else
               {
                       clearVideoPreview();
                       mPreview->setImage("");
                       mPreview->setVisible(false);
               }
       });
#endif
	if (mOkCallback != nullptr)
	{
		mMenu.addButton(_("RESET"), "back", [&]
		{
			onOk("");			
		});
	}

    mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	if (startPath.empty() || !Utils::FileSystem::isDirectory(startPath))
	{
		mCurrentPath = Settings::getInstance()->getString("LastFileBrowserFolder");
		if (mCurrentPath.empty() || !Utils::FileSystem::isDirectory(mCurrentPath))
			mCurrentPath = Paths::getScreenShotPath();

		navigateTo(mCurrentPath);
	}
	else
        navigateTo(startPath);
#ifdef _ENABLEEMUELEC
}
GuiFileBrowser::~GuiFileBrowser()
{
       clearVideoPreview();
}
void GuiFileBrowser::update(int deltaTime)
{
       GuiComponent::update(deltaTime);
       if (mGeneratingPreview)
       {
               auto files = Utils::FileSystem::getDirectoryFiles(mTempPreviewDir);
               files.sort([](const Utils::FileSystem::FileInfo& a, const Utils::FileSystem::FileInfo& b) { return a.path < b.path; });
               for (auto file : files)
               {
                       if (file.directory)
                               continue;
                       if (Utils::String::toLower(Utils::FileSystem::getExtension(file.path)) != ".png")
                               continue;
                       if (std::find(mVideoFrames.begin(), mVideoFrames.end(), file.path) == mVideoFrames.end())
                       {
                               mVideoFrames.push_back(file.path);
                               // Force-load textures so the preview doesn't flicker on first playthrough
                               mFrameTextures.push_back(TextureResource::get(file.path, false, true, true));
                       }
               }
               if ((int)mVideoFrames.size() >= mExpectedFrames)
                       mGeneratingPreview = false;
               else if ((int)mVideoFrames.size() == mLastFrameCount)
               {
                       mNoFrameTime += deltaTime;
                       if (mNoFrameTime > 1000)
                               mGeneratingPreview = false;
               }
               else
               {
                       mLastFrameCount = (int)mVideoFrames.size();
                       mNoFrameTime = 0;
               }
       }
       if (!mGeneratingPreview && !mFrameTextures.empty() && !mPreview->isVisible())
       {
               mPreview->setImage(mFrameTextures[0]);
               mPreview->setVisible(true);
               mLoading->setVisible(false);
               mLoadingBg->setVisible(false);
       }
       if (mPreview->isVisible() && !mFrameTextures.empty())
       {
               mFrameTime += deltaTime;
               if (mFrameTime > 100)
               {
                       mFrameTime = 0;
                       mCurrentFrame = (mCurrentFrame + 1) % mFrameTextures.size();
                       mPreview->setImage(mFrameTextures[mCurrentFrame]);
               }
       }
}
#endif
void GuiFileBrowser::navigateTo(const std::string path)
{
	mCurrentPath = path;

	auto theme = ThemeData::getMenuTheme();

	mMenu.clear();
	mMenu.setSubTitle(mCurrentPath);

	auto files = Utils::FileSystem::getDirectoryFiles(mCurrentPath);

	if (mCurrentPath != "\\" && mCurrentPath != "/" && !mCurrentPath.empty())
	{
		mMenu.addEntry(FOLDER_ICON + std::string(".."), false, [this]()
		{
			navigateTo(Utils::FileSystem::getParent(mCurrentPath));
		});
	}
	files.sort([](const Utils::FileSystem::FileInfo& file1, const Utils::FileSystem::FileInfo& file2) {
	    auto name1 = Utils::FileSystem::getFileName(file1.path);
	    auto name2 = Utils::FileSystem::getFileName(file2.path);
	    return Utils::String::compareIgnoreCase(name1, name2) < 0;
	});
	for (auto file : files)
	{
		if (!file.directory || file.hidden)
			continue;

		std::string icon = FOLDER_ICON;
		if (Utils::String::endsWith(file.path, ":"))
			icon = DRIVE_ICON;

		bool isSelected = false;

		if (mTypes == FileTypes::DIRECTORY)
			isSelected = (mSelectedFile == file.path);

		mMenu.addEntry(icon + Utils::FileSystem::getFileName(file.path), false, [this, file]()
		{
			navigateTo(Utils::FileSystem::combine(mCurrentPath, Utils::FileSystem::getFileName(file.path)));
		}, "", isSelected, false, file.path, false);
	}

	if (mTypes != FileTypes::DIRECTORY)
	{
		for (auto file : files)
		{
			if (file.directory || file.hidden)
				continue;

			std::string ext = Utils::FileSystem::getExtension(file.path);

			std::string icon;
			
			

			if ((mTypes & FileTypes::IMAGES) == FileTypes::IMAGES)
				if (ext == ".jpg" || ext == ".png" || ext == ".gif" || ext == ".svg")
					icon = IMAGE_ICON;

			if ((mTypes & FileTypes::MANUALS) == FileTypes::MANUALS)
				if (ext == ".pdf" || ext == ".cbz")
					icon = DOCUMENT_ICON;

			if ((mTypes & FileTypes::VIDEO) == FileTypes::VIDEO)
				if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".webm")
					icon = VIDEO_ICON;
				
			if ((mTypes & FileTypes::AUDIO) == FileTypes::AUDIO)
				if (ext == ".ogg" || ext == ".mp3" || ext == ".wav")
					icon = AUDIO_ICON;

			if ((mTypes & FileTypes::FILES) == FileTypes::FILES)
					icon = DOCUMENT_ICON;

			if (icon.empty())
				continue;

			bool isSelected = (mSelectedFile == file.path);

			mMenu.addEntry(icon + Utils::FileSystem::getFileName(file.path), false, 
				[this, file]() { onOk(file.path); }, 
				"", isSelected, false, file.path, false);
		}
	}

    centerWindow();
#ifdef _ENABLEEMUELEC
        if (mMenu.size() > 0 && mMenu.getList()->getCursorChangedCallback())
                mMenu.getList()->getCursorChangedCallback()(CURSOR_STOPPED);
#endif
}

void GuiFileBrowser::centerWindow()
{
#ifdef _ENABLEEMUELEC
        float previewWidth = Renderer::getScreenWidth() * 0.3f;
        float menuWidth = Renderer::getScreenWidth() - previewWidth;
        if (Renderer::ScreenSettings::fullScreenMenus())
        {
                mMenu.setSize(menuWidth, Renderer::getScreenHeight());
                mMenu.setPosition(0, 0);
        }
        else
        {
       mMenu.setSize(menuWidth, Renderer::getScreenHeight() * 0.875f);
       mMenu.setPosition((Renderer::getScreenWidth() - (menuWidth + previewWidth)) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
       }
       mPreview->setPosition(mMenu.getPosition().x() + mMenu.getSize().x(), mMenu.getPosition().y());
       mPreview->setMaxSize(previewWidth, mMenu.getSize().y());
       mLoadingBg->setPosition(mPreview->getPosition());
       mLoadingBg->setSize(previewWidth, mMenu.getSize().y());
       mLoading->setPosition(mPreview->getPosition());
       mLoading->setSize(previewWidth, mMenu.getSize().y());
}void GuiFileBrowser::generateVideoPreview(const std::string& path)
{
       clearVideoPreview();
       mTempPreviewDir = Utils::FileSystem::getTempPath() + "/videopreview";
       Utils::FileSystem::createDirectory(mTempPreviewDir);
       std::string command = "ffmpeg -hide_banner -loglevel error -y -i \"" + path +
               "\" -t 5 -vf \"fps=10,scale=720:480:force_original_aspect_ratio=decrease\" \"" +
               mTempPreviewDir + "/frame_%03d.png\"";
       Utils::Platform::ProcessStartInfo psi(command);
       psi.waitForExit = false;
       psi.run();
       mGeneratingPreview = true;
       mExpectedFrames = 50;
       mLastFrameCount = 0;
       mNoFrameTime = 0;
       mCurrentFrame = 0;
       mFrameTime = 0;
       mPreview->setImage("");
       mPreview->setVisible(false);
       mLoadingBg->setVisible(true);
       mLoading->setVisible(true);
}void GuiFileBrowser::clearVideoPreview()
{
       mGeneratingPreview = false;
#ifndef WIN32
       Utils::Platform::ProcessStartInfo kill("killall -q ffmpeg");
       kill.run();
#endif
       for (auto& img : mVideoFrames)
               Utils::FileSystem::removeFile(img);
       if (!mTempPreviewDir.empty())
               Utils::FileSystem::removeDirectory(mTempPreviewDir);
       mVideoFrames.clear();
       mFrameTextures.clear();
       mTempPreviewDir.clear();
       mCurrentFrame = 0;
       mFrameTime = 0;
       mLoading->setVisible(false);
       mLoadingBg->setVisible(false);
       mLastFrameCount = 0;
       mNoFrameTime = 0;
}
#endif
bool GuiFileBrowser::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (config->isMappedTo("y", input) && input.value)
	{
		if (mCurrentPath != "\\" && mCurrentPath != "/" && !mCurrentPath.empty())
			navigateTo(Utils::FileSystem::getParent(mCurrentPath));

		return true;
	}

	if (input.value != 0 && (config->isMappedLike("left", input) || config->isMappedLike("right", input)))
	{
		mMenu.setCursorToButtons();
		return true;
	}

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{		
		if (mMenu.size() && mOkCallback != nullptr)
		{
			auto path = mMenu.getSelected();

			if (mTypes == FileTypes::DIRECTORY && path.empty())
				onOk(mCurrentPath);				
			else if (!path.empty() && (mTypes == FileTypes::DIRECTORY || !Utils::FileSystem::isDirectory(path)))
				onOk(path);
		}

		return true;		
	}

	if (config->isMappedTo("x", input) && input.value && mOkCallback != nullptr)
	{
		onOk("");		
		return true;
	}
	
	if (config->isMappedTo("select", input))
	{
		navigateTo(Paths::getScreenShotPath());
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiFileBrowser::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	
	if (mOkCallback != nullptr)
		prompts.push_back(HelpPrompt("x", _("RESET")));

	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	prompts.push_back(HelpPrompt("y", _("PARENT FOLDER")));
	prompts.push_back(HelpPrompt("select", _("SCREENSHOTS FOLDER")));
	prompts.push_back(HelpPrompt("start", _("SELECT")));

	return prompts;
}

void GuiFileBrowser::onOk(const std::string& path)
{
	if (Utils::FileSystem::isDirectory(mCurrentPath) && Settings::getInstance()->setString("LastFileBrowserFolder", mCurrentPath))
		Settings::getInstance()->saveFile();

	if (mOkCallback)
		mOkCallback(path);

	delete this;
}
