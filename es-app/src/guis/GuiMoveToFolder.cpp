#include "GuiMoveToFolder.h"

#include "CollectionSystemManager.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "SystemData.h"
#include "Settings.h"
#include "utils/Platform.h"

#include "views/gamelist/IGameListView.h"
#include "views/gamelist/ISimpleGameListView.h"
#include "views/ViewController.h"

#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiTextEditPopup.h"
#include "utils/StringUtil.h"

template<class T>
T base_path(T const & path, T const & delims = "/\\")
{
  return path.substr(0,path.find_last_of(delims));
}

template<class T>
T base_name(T const & path, T const & delims = "/\\")
{
  return path.substr(path.find_last_of(delims) + 1);
}

template<class T>
T remove_extension(T const & filename)
{
  typename T::size_type const p(filename.find_last_of('.'));
  return p > 0 && p != T::npos ? filename.substr(0, p) : filename;
}

// ISimpleGameListView::goBack() - the same in-place, no-full-rebuild
// navigation the BACK button uses to step the browsed folder up to its
// parent - is exactly what's needed after a move empties the folder
// currently being browsed, but it's a protected member, and this is an
// unrelated class. Rather than touch ISimpleGameListView.h (shared,
// non-EmuELEC-specific code) to expose it, this local helper pulls it into
// public scope via a "using" declaration and nothing else - no new members,
// no virtual overrides - so it's layout-identical to ISimpleGameListView.
// An existing BasicGameListView/GridGameListView instance can then be
// reinterpreted through it purely to make this one call; no
// GoBackAccessor is ever actually constructed.
class GoBackAccessor : public ISimpleGameListView
{
public:
	using ISimpleGameListView::goBack;
};

// The window opened by the "MOVE TO FOLDER" action. One row per destination
// folder; choosing a row hands the path to onChoose and closes just this
// popup. Backing out - hardware Back or the BACK button below - closes the
// popup without ever calling onChoose, so nothing is chosen and nothing runs.
class GuiFolderPicker : public GuiComponent
{
public:
	GuiFolderPicker(Window* window, const std::string& title,
		const std::vector<std::pair<std::string, std::string>>& folders, // display name, path
		const std::string& preselectPath,
		const std::function<void(const std::string&)>& onChoose)
		: GuiComponent(window), mMenu(window, title.c_str())
	{
		addChild(&mMenu);

		for (auto& folder : folders)
		{
			mMenu.addEntry(folder.first, false, [this, onChoose, folder]
			{
				onChoose(folder.second);
				delete this;
			}, "", folder.second == preselectPath);
		}

		mMenu.addButton(_("BACK"), "back", [this] { delete this; });

		if (Renderer::ScreenSettings::fullScreenMenus())
			mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
		else
			mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
	}

	bool input(InputConfig* config, Input input) override
	{
		if (GuiComponent::input(config, input))
			return true;

		// Cancel: closes this popup only, no folder is chosen, no command runs.
		if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
		{
			delete this;
			return true;
		}

		return false;
	}

	std::vector<HelpPrompt> getHelpPrompts() override
	{
		auto prompts = mMenu.getHelpPrompts();
		prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
		return prompts;
	}

private:
	MenuComponent mMenu;
};

GuiMoveToFolder::GuiMoveToFolder(Window* window, FileData* file) :
  GuiSettings(window, _("FILE ")+file->getName().c_str())
{
  // Each entry below is just a one-line call into the matching static
  // helper - the same helper a caller that wants only one specific action
  // (and no "FOLDER OPTIONS" screen at all) calls directly instead. See
  // GuiGameOptions.cpp's own "FOLDER OPTIONS" entry for that caller.

  if (file->getType() == GAME)
  {
    // Selecting this opens the folder picker (current subfolders of this
    // game's own folder, plus a "go up a level" entry when applicable),
    // then a confirmation before anything actually moves. onMoved closes
    // this screen once the move actually happens - not on a cancel at
    // either step - since the game this screen is about has just moved
    // somewhere else.
    addEntry(_("MOVE TO FOLDER"), true, [this, window, file]
    {
      moveToFolder(window, file, [this] { close(); });
    });
  }

  // CREATE FOLDER works the same regardless of whether 'file' is a game or
  // a folder - getSourceFileData()->getParent() are plain FileData members
  // that behave identically either way (a FolderData doesn't override
  // either one - only CollectionFileData does, and that's never itself a
  // FolderData) - so a folder selected here gets a new sibling folder
  // created next to IT, same as a game would. No onCreated: this mirrors
  // the original behavior of leaving the screen open afterwards, since the
  // folder created is unrelated to 'file' itself (a sibling, not a
  // replacement for it).
  FolderData* parent = file->getSourceFileData()->getParent();
  if (parent != nullptr)
  {
    addEntry(_("CREATE FOLDER"), false, [this, window, parent]
    {
      createFolder(window, parent);
    });
  }

  if (file->getType() == FOLDER)
  {
    // 'file' IS the folder to remove here - no picker needed, unlike MOVE
    // TO FOLDER above. Same as CREATE FOLDER, this leaves the screen open
    // afterwards (matching the original design) rather than closing it.
    addEntry(_("REMOVE FOLDER"), false, [this, window, file]
    {
      confirmAndRemove(window, (FolderData*)file);
    });
  }
}

// static
void GuiMoveToFolder::moveToFolder(Window* window, FileData* file, std::function<void()> onMoved)
{
  if (file == nullptr || file->getType() != GAME)
    return;

  std::string lastFolder = SystemConf::getInstance()->get("folder_option");

  // getChildFolders() shells out to "find <path> -type d", so it needs the
  // DIRECTORY the game lives in - not the game file's own path
  // (file->getPath() is the ROM file itself, e.g. ".../nes/foo.zip").
  // Passing the file path here is why the list came back empty.
  std::string gameDir = base_path<std::string>(file->getPath());
  std::vector<std::string> folders = ApiSystem::getInstance()->getChildFolders(gameDir);

  std::vector<std::pair<std::string, std::string>> options;

  if (file->getParent()->getParent() != nullptr) {
    std::string upPath = base_path<std::string>(gameDir);
    options.push_back(std::make_pair(".. (" + base_name<std::string>(upPath) + ")", upPath));
  }

  for (auto folder : folders)
    options.push_back(std::make_pair(base_name<std::string>(folder), folder));

  if (options.empty())
  {
    window->pushGui(new GuiMsgBox(window, _("NO FOLDERS FOUND"), _("OK")));
    return;
  }

  window->pushGui(new GuiFolderPicker(window, _("CHOOSE FOLDER"), options, lastFolder,
    [window, file, onMoved](const std::string& path)
    {
      // Same YES/NO confirmation this used to show from the old instance
      // method confirmAndMove() - just with no GuiMoveToFolder screen left
      // to close afterwards. onMoved() stands in for that close() call: it
      // only runs once the move actually happens, same as before, so
      // declining this confirmation (or backing out of the picker above)
      // leaves 'file' untouched and runs nothing.
      std::string folderName = base_name<std::string>(path);

      window->pushGui(new GuiMsgBox(window,
        Utils::String::format(_("MOVE '%s' TO '%s' ?").c_str(), file->getName().c_str(), folderName.c_str()),
        _("YES"), [file, path, onMoved]
        {
          SystemConf::getInstance()->set("folder_option", path);
          SystemConf::getInstance()->saveSystemConf();

          // moveToFolderGame() patches the in-memory FolderData tree itself
          // and, via view->remove(sourceFile), already triggers a full
          // ViewController::reloadGameListView() to refresh the current
          // gamelist - the same call GuiGameOptions::deleteGame() relies on
          // for exactly the same reason, so nothing else needs to refresh
          // anything after this.
          moveToFolderGame(file, path);

          if (onMoved)
            onMoved();
        },
        _("NO"), nullptr));
    }));
}

// static
void GuiMoveToFolder::moveToFolderGame(FileData* file, const std::string& path)
{
	if (file->getType() != GAME)
		return;

	auto sourceFile = file->getSourceFileData();

	auto sys = sourceFile->getSystem();
	if (sys->isGroupChildSystem())
		sys = sys->getParentGroupSystem();

	CollectionSystemManager::get()->deleteCollectionFiles(sourceFile);

	auto view = ViewController::get()->getGameListView(sys, false);

	char cmdMvFile[1024];
  snprintf(cmdMvFile, sizeof(cmdMvFile), "mv \"%s\" \"%s\" 2>&1 | tee /emuelec/logs/mtf.log",
    sourceFile->getFullPath().c_str(), path.c_str());
  std::string strMvFile = cmdMvFile;
	system(strMvFile.c_str());

  // Resolve the destination FolderData node. The folder picker only ever
  // offers two kinds of destination: one level UP (the game's own
  // grandparent folder) or a folder DOWN, directly inside the game's current
  // folder. The previous version tried to find the destination with a single
  // name lookup scoped to the game's current parent - which is the right
  // scope for a "down" move, but wrong for an "up" move (the grandparent
  // isn't a child of the parent, it's the parent's own parent), so "up"
  // moves more than one level deep would silently fail to find it. Handling
  // the two cases explicitly, instead of one lookup for both, is the fix.
  FolderData* parentDir = file->getParent();
  FolderData* grandParentDir = parentDir->getParent();

  bool movingUp = (grandParentDir != nullptr && path == grandParentDir->getPath());

  FolderData* fd = nullptr;
  if (movingUp)
    fd = grandParentDir;
  else
  {
    fd = getFolderData(parentDir, base_name<std::string>(path));
    if (fd == nullptr)
    {
      // The destination folder exists on disk (getChildFolders() found it)
      // but has no node in the tree yet - e.g. it's empty, or was just
      // created and has never held a game. Add one so the move can still be
      // reflected without a full rescan. ownsChildrens defaults to true,
      // same as every other FolderData that represents a real directory
      // (see SystemData::populateFolder) - it should own and clean up
      // whatever games end up inside it.
      fd = new FolderData(path.c_str(), sys);
      parentDir->addChild(fd);
    }
  }

  std::string newPath = path+"/"+base_name<std::string>(file->getPath());
  FileData* newFile = new FileData(GAME, newPath, sys);
  newFile->setMetadata(file->getMetadata());

  fd->addChild(newFile);

  // Moving UP is the only way the game's OLD folder can end up with nothing
  // left in it - moving DOWN always leaves the destination folder itself
  // behind as a child of parentDir, so parentDir can never empty out that way.
  bool sourceFolderWillBeEmpty = movingUp && parentDir->getChildren().size() == 1;

  // Is the screen actually browsing that same folder right now (a normal
  // per-folder view sitting on parentDir), or is folder navigation not
  // really "in play" here at all - a flat "never show folders" list, the
  // system root, a grid/grouped quirk, or no view open yet? Only in the
  // first case does "go back up a level" mean anything concrete.
  ISimpleGameListView* simpleView = (view != nullptr) ? dynamic_cast<ISimpleGameListView*>(view.get()) : nullptr;
  bool browsingSourceFolder = simpleView != nullptr && simpleView->getCurrentFolder() == parentDir;

  if (browsingSourceFolder && sourceFolderWillBeEmpty)
  {
    // Nothing will be left to show in the folder we're leaving - instead of
    // refreshing it onto an empty list, detach the game directly (mirroring
    // the "no view" fallback below) and step the view back up to the parent
    // in place, the same lightweight cursor-stack navigation the BACK button
    // itself uses. The parent already contains the moved game at this point,
    // so it shows up immediately - no full view rebuild needed.
    sys->getRootFolder()->removeFromVirtualFolders(sourceFile);
    delete sourceFile;
    static_cast<GoBackAccessor*>(simpleView)->goBack();
  }
  else if (view != nullptr) {
    // Either the folder we're leaving still has other things in it, or
    // folder navigation isn't in play right now (e.g. a flat listing) - in
    // both cases the established, safe pattern (see
    // GuiGameOptions::deleteGame) is to hand the removal to the view
    // itself, which detaches/deletes sourceFile and refreshes on its own,
    // recomputing whatever's currently displayed - a flat list included -
    // from the now up-to-date tree.
    view.get()->remove(sourceFile);
  }
  else {
    sys->getRootFolder()->removeFromVirtualFolders(sourceFile);
    delete sourceFile;
  }
}

// static
FolderData* GuiMoveToFolder::getFolderData(FolderData* folder, const std::string& name)
{
  std::vector<FileData*> children = folder->getChildren();
  std::string name2;
  for (auto it = children.begin(); it != children.end(); it++) {
    name2 = base_name<std::string>((*it)->getPath());
    if ((*it)->getType() == FOLDER && name2 == name)
      return dynamic_cast<FolderData*>(*it);
  }
  return nullptr;
}

// static
void GuiMoveToFolder::createFolder(Window* window, FolderData* parentFolder, std::function<void()> onCreated)
{
  if (parentFolder == nullptr)
    return;

  // Same prompt-for-a-name-then-create flow this used to duplicate inline
  // in GuiGameOptions.cpp's "CREATE FOLDER" entry (and, before that, in
  // GuiMoveToFolder's own now-removed screen) - one copy of it here instead.
  auto updateFN = [window, parentFolder, onCreated](const std::string& newVal)
  {
    if (newVal.empty())
      return;

    std::string path = parentFolder->getPath() + "/" + newVal;
    if (Utils::FileSystem::exists(path.c_str()))
    {
      window->pushGui(new GuiMsgBox(window, _("FOLDER EXISTS"), _("OK"), nullptr));
      return;
    }

    createFolder(parentFolder, path);

    if (onCreated)
      onCreated();
  };

  if (Settings::getInstance()->getBool("UseOSK"))
    window->pushGui(new GuiTextEditPopupKeyboard(window, _("FOLDER NAME"), "", updateFN, false));
  else
    window->pushGui(new GuiTextEditPopup(window, _("FOLDER NAME"), "", updateFN, false));
}

// static
void GuiMoveToFolder::createFolder(FolderData* parentFolder, const std::string& path)
{
  // parentFolder is always a resolved, real tree node by the time it gets
  // here - a FolderData is never a CollectionFileData proxy (that class
  // wraps a FileData, not a FolderData), so its getSystem() is already the
  // real system. Callers are responsible for resolving any raw FileData -
  // which might be a collection entry - down to its real parent FolderData
  // before calling this (see GuiGameOptions.cpp's "CREATE FOLDER" entry),
  // so no getSourceFileData() unwrapping is needed in here anymore.
  auto sys = parentFolder->getSystem();
	if (sys->isGroupChildSystem())
		sys = sys->getParentGroupSystem();
  auto view = ViewController::get()->getGameListView(sys, false);

  if (!Utils::FileSystem::exists(path.c_str())) {
		Utils::FileSystem::createDirectory(path.c_str());

    // Add the new folder to the in-memory tree and the currently displayed
    // gamelist right away, so it shows up immediately instead of waiting
    // for a rescan - mirrors the repopulate()+setCursor() pattern used
    // elsewhere in this codebase (see GuiGameOptions::createMultidisc).
    // ownsChildrens defaults to true here (it did NOT before - that was a
    // latent bug: passing false marks a folder as "virtual storage", which
    // is meant for synthetic/group folders, not a real directory like this
    // one that should own and clean up whatever games get moved into it).
    FolderData* newFolder = new FolderData(path.c_str(), sys);
    parentFolder->addChild(newFolder);

    if (view != nullptr) {
      // repopulate() is a lightweight, in-place refresh of the currently
      // displayed list - unlike reloadGameListView(), it doesn't tear down
      // and rebuild the view object, so it's safe to follow immediately
      // with setCursor(). Calling reloadGameListView() as well (as this used
      // to) would rebuild the view from scratch right after, silently
      // undoing the cursor placement (it can only restore the cursor by
      // matching a GAME's path, and newFolder is a FOLDER) for no benefit -
      // the same redundant-double-refresh pattern that crashes MOVE TO
      // FOLDER when it lands on a subfolder.
      view.get()->repopulate();
      view->setCursor(newFolder);
    }
	}
}

// static
void GuiMoveToFolder::confirmAndRemove(Window* window, FolderData* folder)
{
  if (folder == nullptr)
    return;

  std::string path = folder->getPath();
  std::string folderName = base_name<std::string>(path);

  // The caller already has the exact FolderData in hand (e.g. it's the item
  // currently selected in a gamelist), so there's no name lookup to get
  // wrong here - just confirm, delete from disk, and detach/refresh the
  // model.
  window->pushGui(new GuiMsgBox(window,
    Utils::String::format(_("REMOVE FOLDER '%s' AND ALL ITS CONTENTS ?").c_str(), folderName.c_str()),
    _("YES"), [window, folder, path]
    {
      char cmdRmDir[1024];
      snprintf(cmdRmDir, sizeof(cmdRmDir), "rm -rf \"%s\" 2>&1 | tee /emuelec/logs/mtf.log", path.c_str());
      system(cmdRmDir);

      SystemData* sys = folder->getSystem();
      if (sys->isGroupChildSystem())
        sys = sys->getParentGroupSystem();

      // Same pattern as everywhere else in this file (see
      // GuiGameOptions::deleteGame for the original precedent): hand the
      // removal to the view when one is open - it detaches/deletes and
      // refreshes on its own - and skip the model update entirely when
      // there's nothing currently on screen for this system.
      auto view = ViewController::get()->getGameListView(sys, false);
      if (view != nullptr)
        view.get()->remove(folder);
      else
      {
        sys->getRootFolder()->removeFromVirtualFolders(folder);
        delete folder;
      }
    },
    _("NO"), nullptr));
}
