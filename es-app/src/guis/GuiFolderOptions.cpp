#include "GuiFolderOptions.h"

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
#include "guis/GuiLoading.h"
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

GuiFolderOptions::GuiFolderOptions(Window* window, FileData* file) :
  GuiSettings(window, _("FILE ")+file->getName().c_str())
{
  // Each entry below is just a one-line call into the matching static
  // helper - the same helper a caller that wants only one specific action
  // (and no "FOLDER OPTIONS" screen at all) calls directly instead. See
  // GuiGameOptions.cpp's own "FOLDER OPTIONS" entry for that caller.

  if (file->getType() == GAME || file->getType() == FOLDER)
  {
    // Selecting this opens the folder picker (current subfolders of this
    // game or folder's own directory, plus a "go up" entry for every
    // ancestor above it), then a confirmation before anything actually
    // moves. onMoved closes this screen once the move actually happens -
    // not on a cancel at either step - since whatever this screen is about
    // has just moved somewhere else.
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
  // created next to IT, same as a game would. onCreated closes this screen
  // once the folder actually gets created - not on a cancel or a name
  // collision - same as onMoved above for MOVE TO FOLDER.
  FolderData* parent = file->getSourceFileData()->getParent();
  if (parent != nullptr)
  {
    addEntry(_("CREATE FOLDER"), false, [this, window, parent]
    {
      createFolder(window, parent, [this] { close(); });
    });
  }

  if (file->getType() == FOLDER)
  {
    // Same name prompt as CREATE FOLDER above, but the new folder is created
    // INSIDE the selected folder rather than next to it. 'file' is already a
    // real FolderData here (GuiGameOptions never offers FOLDER OPTIONS for a
    // collection or virtual-storage folder), so it's the parent directly.
    // onCreated closes this screen, same as CREATE FOLDER above.
    addEntry(_("CREATE FOLDER IN FOLDER"), false, [this, window, file]
    {
      createFolder(window, (FolderData*)file, [this] { close(); });
    });

    // 'file' IS the folder to rename here - no picker needed, same as
    // REMOVE FOLDER below. onRenamed closes this screen once the rename
    // actually happens - not on a cancel or a name collision - same as
    // onCreated above.
    addEntry(_("RENAME FOLDER"), false, [this, window, file]
    {
      renameFolder(window, (FolderData*)file, [this] { close(); });
    });

    // 'file' IS the folder to remove here - no picker needed, unlike MOVE
    // TO FOLDER above. onRemoved closes this screen once the removal
    // actually happens - not on a NO or a Back - same as onRenamed above.
    addEntry(_("REMOVE FOLDER"), false, [this, window, file]
    {
      confirmAndRemove(window, (FolderData*)file, [this] { close(); });
    });
  }

  // Unlike every entry above, this doesn't do anything to 'file' itself -
  // it forces 'file's own system's cached folder tree to be rebuilt right
  // now, straight off disk, rather than waiting on that cache's own
  // automatic staleness check (see ApiSystem.h/.cpp) - so it's shown
  // regardless of whether 'file' is a game or a folder, and leaves this
  // screen open afterwards (there's nothing about 'file' itself for the
  // caller to react to, unlike onMoved/onCreated/onRenamed/onRemoved
  // above).
  addEntry(_("RESCAN FOLDERS"), true, [this, window, file]
  {
    rescanFolders(window, file);
  });
}

// static
void GuiFolderOptions::moveToFolder(Window* window, FileData* file, std::function<void()> onMoved)
{
  if (file == nullptr || (file->getType() != GAME && file->getType() != FOLDER))
    return;

  std::string lastFolder = SystemConf::getInstance()->get("folder_option");

  // getChildFolders() shells out to "find <path> -type d", so it needs the
  // DIRECTORY 'file' lives in - not 'file's own path (for a game,
  // file->getPath() is the ROM file itself, e.g. ".../nes/foo.zip"; for a
  // folder, it's that folder's own directory, one level too deep).
  // Passing the file path here is why the list came back empty.
  std::string gameDir = base_path<std::string>(file->getPath());

  // getChildFolders() caches this system's whole folder tree to a
  // folders.xml right in its own ROM root, so it needs THAT root - not
  // 'gameDir' - to find (or, the very first time, build) it; 'gameDir'
  // just says which part of that cached tree to hand back here. It also
  // only filters out reserved media folder names (see its own comment)
  // right under that root - a folder the user created deeper in the tree
  // to organize games could legitimately be named "images" or the like,
  // and shouldn't be hidden from this picker just because it shares a name
  // with one of those.
  auto sourceFile = file->getSourceFileData();
  auto sys = sourceFile->getSystem();
  if (sys->isGroupChildSystem())
    sys = sys->getParentGroupSystem();

  std::string systemRootPath = sys->getStartPath();

  // getChildFolders() reads folders.xml's cache back when it's already
  // fresh, but falls back to a live "find" rebuild whenever that cache is
  // missing, corrupted, or - per isFolderTreeStale() - has fallen out of
  // sync with what's actually on disk (see both of their own comments in
  // ApiSystem.cpp) - and that rebuild is the one case here that can
  // actually take a while (a slow SD card or network share, a large ROM
  // tree). Whether THIS call is going to hit that slow path or the fast one
  // isn't something this function can tell from out here any more: this
  // used to only wrap the call in a background GuiLoading screen when
  // folders.xml didn't exist yet at all, on the assumption that an existing
  // file meant an already-fresh cache and a fast, safe-to-call-inline
  // result - but a PRESENT, STALE cache takes exactly the same slow rebuild
  // path as a missing one, with nothing visible from here to tell the two
  // apart. Calling getChildFolders() directly, unwrapped, on the UI thread
  // in that case is precisely what froze the menu for over a minute with no
  // "BUILDING FOLDER DATA..." message and no explanation. Always run it
  // through this same background wrapper instead, regardless of whether
  // folders.xml exists yet - the common, already-cached-and-fresh case
  // still resolves fast enough that GuiLoading's own busy animation barely
  // has time to fade in, and a genuine rebuild now always gets to show it
  // rather than ever locking up silently.
  window->pushGui(new GuiLoading<std::vector<std::string>>(window,
    Utils::String::format(_("BUILDING FOLDER DATA FOR %s...").c_str(), sys->getFullName().c_str()),
    [gameDir, systemRootPath](auto /*gui*/)
    {
      return ApiSystem::getInstance()->getChildFolders(gameDir, systemRootPath);
    },
    [window, file, onMoved, gameDir, lastFolder](std::vector<std::string> folders)
    {
      showMoveToFolderPicker(window, file, onMoved, gameDir, lastFolder, folders);
    }));
}

// static
void GuiFolderOptions::showMoveToFolderPicker(Window* window, FileData* file, std::function<void()> onMoved, const std::string& gameDir, const std::string& lastFolder, const std::vector<std::string>& folders)
{
  std::vector<std::pair<std::string, std::string>> options;

  // Offer every ancestor folder above the game's own, all the way up to
  // (and including) the system's own ROM root - not just the immediate
  // parent - as a "move up" destination. Walking the in-memory tree here
  // rather than the filesystem path means this naturally stops right at
  // the system root (its own getParent() is nullptr), the same boundary
  // moveToFolder(FileData*, path) below now resolves against. Each one's
  // own ".." is repeated once per level it actually is above 'file's own
  // folder - "..", "../..", "../../.." and so on, the same relative-path
  // convention a shell uses - so the picker itself says how many steps
  // back each destination is instead of every ancestor looking like a
  // single step up.
  std::string upPrefix;
  for (FolderData* up = file->getParent()->getParent(); up != nullptr; up = up->getParent())
  {
    upPrefix = upPrefix.empty() ? ".." : upPrefix + "/..";
    options.push_back(std::make_pair(upPrefix + " (" + base_name<std::string>(up->getPath()) + ")", up->getPath()));
  }

  // getChildFolders() now lists every folder under gameDir, however many
  // levels down - not just its direct children - so a "down" destination
  // can be any of them. When 'file' is itself a folder, that includes
  // 'file's own directory (it's a subfolder of its own parent same as any
  // sibling) and everything still nested under it; skip all of it rather
  // than offering "move this folder into itself or one of its own
  // subfolders" as a destination.
  for (auto folder : folders)
  {
    if (file->getType() == FOLDER &&
      (folder == file->getPath() || Utils::String::startsWith(folder, file->getPath() + "/")))
      continue;

    // A destination's own name alone is no longer enough to tell it apart
    // from another folder named the same thing elsewhere in the tree, now
    // that this list can reach more than one level down - show its path
    // relative to gameDir instead, slashes and all (folder is always
    // gameDir plus one or more "/name" components, since that's exactly
    // what getChildFolders(gameDir, ...) searched under).
    std::string relativeDisplay = folder.substr(gameDir.size() + 1);
    options.push_back(std::make_pair(relativeDisplay, folder));
  }

  if (options.empty())
  {
    window->pushGui(new GuiMsgBox(window, _("NO FOLDERS FOUND"), _("OK")));
    return;
  }

  window->pushGui(new GuiFolderPicker(window, _("CHOOSE FOLDER"), options, lastFolder,
    [window, file, onMoved](const std::string& path)
    {
      // Same "FOLDER EXISTS" collision check createFolder() above runs
      // before creating a new folder, run here before even asking to move
      // one - the destination already has an entry named the same as
      // 'file' (its extension stripped, same as createFolder()'s check),
      // so moving there would either silently overwrite it or leave the
      // tree with two same-named entries. Neither the move nor its
      // confirmation happens in that case. The message names whichever
      // 'file' itself actually is - a folder being moved collides with
      // "FOLDER ALREADY EXISTS", a game with "FILE ALREADY EXISTS" - since
      // the entry already sitting there could be either one regardless.
      if (destinationHasNameCollision(file, path))
      {
        window->pushGui(new GuiMsgBox(window,
          file->getType() == FOLDER ? _("FOLDER ALREADY EXISTS") : _("FILE ALREADY EXISTS"),
          _("OK"), nullptr));
        return;
      }

      // Same YES/NO confirmation this used to show from the old instance
      // method confirmAndMove() - just with no GuiFolderOptions screen left
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

          // moveToFolder(FileData*, path) below patches the in-memory
          // FolderData tree itself and refreshes whichever gamelist is
          // currently open on its own, so nothing else needs to refresh
          // anything after this.
          moveToFolder(file, path);

          if (onMoved)
            onMoved();
        },
        _("NO"), nullptr));
    }));
}

// static
void GuiFolderOptions::moveToFolder(FileData* file, const std::string& path)
{
  if (file->getType() == FOLDER)
  {
    // Unlike a moved GAME below, a moved FOLDER is never replaced with a
    // new node - it (and everything still attached beneath it) is reused
    // as-is under its new parent, the same way renameFolder(FolderData*,
    // newPath) reuses it for an in-place rename. Moving is really a rename
    // to a new parent, so this mostly follows that function's own shape.
    FolderData* movingFolder = (FolderData*)file;

    if (path == movingFolder->getPath())
      return;

    auto sys = movingFolder->getSystem();
    if (sys->isGroupChildSystem())
      sys = sys->getParentGroupSystem();

    // Same collection cleanup the GAME case below runs for the one game
    // being moved, applied here to every game inside the folder's
    // subtree - moving any of them out from under a collection invalidates
    // that collection's reference to it, the same as moving it individually
    // would.
    for (auto game : movingFolder->getFilesRecursive(GAME))
      CollectionSystemManager::get()->deleteCollectionFiles(game);

    auto view = ViewController::get()->getGameListView(sys, false);

    // Resolve the destination FolderData node - identical ancestor-or-
    // descendant resolution the GAME case below uses for the same purpose,
    // scoped to movingFolder's own parent rather than file->getParent()
    // (same thing here, spelled out since parentDir is reused below after
    // movingFolder is detached from it).
    FolderData* parentDir = movingFolder->getParent();

    FolderData* destAncestor = nullptr;
    int upLevels = 0;
    for (FolderData* up = parentDir->getParent(); up != nullptr; up = up->getParent())
    {
      ++upLevels;
      if (up->getPath() == path)
      {
        destAncestor = up;
        break;
      }
    }

    bool movingUp = (destAncestor != nullptr);

    // resolveDescendant() covers a "down" destination several levels under
    // parentDir now, not just a direct child of it - walking (and creating,
    // where a level exists on disk but was never tracked) every level in
    // between, not just the last one.
    FolderData* fd = movingUp ? destAncestor : resolveDescendant(parentDir, path, true);

    std::string oldPath = movingFolder->getPath();
    std::string newPath = path + "/" + base_name<std::string>(oldPath);

    // Move the directory - and everything inside it - on disk in one shot;
    // "mv" already recurses, so there's no need to walk the subtree here
    // the way updateChildPaths() below has to for the in-memory side.
    char cmdMvDir[1024];
    snprintf(cmdMvDir, sizeof(cmdMvDir), "mv \"%s\" \"%s\" 2>&1 | tee /emuelec/logs/mtf.log",
      oldPath.c_str(), newPath.c_str());
    system(cmdMvDir);

    // movingFolder just changed which folder it's listed under - same
    // reason createFolder() above and renameFolder() / confirmAndRemove()
    // below each do this after their own change to the folder tree.
    ApiSystem::getInstance()->moveFolderInCache(sys->getStartPath(), oldPath, newPath);

    // Moving UP is the only way the folder's OLD parent can end up with
    // nothing left in it - same reasoning as the GAME case below. Checked
    // before movingFolder is actually detached from parentDir just below,
    // same as the GAME case checks it before sourceFile is detached.
    bool sourceFolderWillBeEmpty = movingUp && parentDir->getChildren().size() == 1;

    // Reparent movingFolder in the tree: detach it from parentDir, patch
    // its own path and every descendant's to match (exactly what
    // renameFolder(FolderData*, newPath) does for an in-place rename), then
    // attach it under fd.
    parentDir->removeChild(movingFolder);
    movingFolder->setPath(newPath);
    movingFolder->getMetadata().setDirty();
    updateChildPaths(movingFolder, oldPath, newPath);
    fd->addChild(movingFolder);

    ISimpleGameListView* simpleView = (view != nullptr) ? dynamic_cast<ISimpleGameListView*>(view.get()) : nullptr;
    bool browsingSourceFolder = simpleView != nullptr && simpleView->getCurrentFolder() == parentDir;

    if (browsingSourceFolder && sourceFolderWillBeEmpty)
    {
      // Nothing will be left to show in the folder we're leaving - step the
      // view back up in place, once per ancestor hop this move crossed,
      // same as the GAME case below. Unlike that case, there's no old node
      // to detach-and-delete here first: movingFolder is still alive and
      // valid, just reparented above, so goBack() alone is all this needs.
      for (int i = 0; i < upLevels; i++)
        static_cast<GoBackAccessor*>(simpleView)->goBack();
    }
    else if (view != nullptr) {
      // movingFolder is still a live, valid tree node - repopulate() just
      // recomputes the displayed list from the now up-to-date tree, the
      // same lightweight refresh createFolder() and renameFolder() above
      // use, rather than view->remove() (which would detach AND delete it,
      // destroying the very subtree just preserved above).
      view.get()->repopulate();

      // FOLDER OPTIONS can also be opened on the folder currently being
      // browsed (via the ".." entry), in which case movingFolder IS the
      // view's current folder and the cursor stack still points at its old
      // ancestors. setCursor() on a folder that isn't in the displayed
      // list rebuilds that stack from the tree, landing on movingFolder's
      // new parent with it selected - same as renameFolder() above.
      if (simpleView != nullptr && simpleView->getCurrentFolder() == movingFolder)
        view->setCursor(movingFolder);
    }

    return;
  }

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

  // Resolve the destination FolderData node. The folder picker offers two
  // kinds of destination: some folder DOWN, one or more levels inside the
  // game's current folder, or any ancestor UP the tree, as far as the
  // system's own ROM root. resolveDescendant() below covers a "down" move
  // (walking, and creating where a level exists on disk but was never
  // tracked, every level between parentDir and the destination - not just a
  // direct child of it), but that walk is wrong for an "up" move to any
  // ancestor beyond the immediate parent (none of those are children of
  // parentDir - they're parentDir's own ancestors), so resolving "up" means
  // walking that ancestor chain looking for a path match instead. upLevels
  // counts how many hops it took to get there, for the "step the view back
  // up" case below - the picker can now offer a destination more than one
  // level up, so stepping the browse stack back needs to match however far
  // this particular move actually went, not always exactly one pop.
  FolderData* parentDir = file->getParent();

  FolderData* destAncestor = nullptr;
  int upLevels = 0;
  for (FolderData* up = parentDir->getParent(); up != nullptr; up = up->getParent())
  {
    ++upLevels;
    if (up->getPath() == path)
    {
      destAncestor = up;
      break;
    }
  }

  bool movingUp = (destAncestor != nullptr);

  // ownsChildrens defaults to true on any newly-created level here, same as
  // every other FolderData that represents a real directory (see
  // SystemData::populateFolder) - it should own and clean up whatever games
  // end up inside it.
  FolderData* fd = movingUp ? destAncestor : resolveDescendant(parentDir, path, true);

  std::string newPath = path+"/"+base_name<std::string>(file->getPath());
  FileData* newFile = new FileData(GAME, newPath, sys);
  newFile->setMetadata(file->getMetadata());

  fd->addChild(newFile);

  // Unlike the FOLDER case above, moving a plain GAME never adds, removes
  // or renames anything in ApiSystem's own folders.xml cache - the game
  // itself was never a cache entry - so none of addFolderToCache() /
  // moveFolderInCache() / removeFolderFromCache() apply here. But the move
  // still changes both parentDir's and fd's own mtimes on disk (a file
  // just left one and landed in the other), which is exactly what
  // getChildFolders()'s own staleness check looks for on its very next
  // call - refresh both here so this in-app move doesn't look like an
  // outside edit and force a fresh "find" scan of the whole system it
  // doesn't need.
  ApiSystem::getInstance()->refreshFolderMtime(sys->getStartPath(), parentDir->getPath());
  ApiSystem::getInstance()->refreshFolderMtime(sys->getStartPath(), fd->getPath());

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
    // the "no view" fallback below) and step the view back up in place, the
    // same lightweight cursor-stack navigation the BACK button itself uses -
    // once per ancestor hop the move actually crossed, so a move straight to
    // the system's root steps all the way back there instead of stopping one
    // level short of it. destAncestor already contains the moved game at
    // this point, so it shows up immediately once the stack reaches it - no
    // full view rebuild needed.
    sys->getRootFolder()->removeFromVirtualFolders(sourceFile);
    delete sourceFile;

    for (int i = 0; i < upLevels; i++)
      static_cast<GoBackAccessor*>(simpleView)->goBack();
  }
  else {
    // Either the folder we're leaving still has other things in it, or
    // folder navigation isn't in play right now (e.g. a flat listing) -
    // either way, detach sourceFile directly (same as the "no view"
    // fallback right below, and the emptied-source branch above) and
    // repopulate() whichever folder is actually being browsed right now,
    // rather than handing this to view->remove(). That call used to route
    // through ViewController::reloadGameListView(), which discards this
    // view and builds a brand new one starting back at the system root,
    // then tries to re-descend to roughly where the cursor was by
    // searching for a nearby surviving item by path - if that search ever
    // comes up empty (the cursor landed on a placeholder, or on an item
    // that itself got flattened away by the very move that just
    // happened), the screen silently resets to the system root instead of
    // staying put. repopulate() gives the same end result far more
    // directly: it re-derives the folder actually on top of mCursorStack
    // and recomputes getChildrenListToDisplay() for it fresh, which is
    // also what makes a sibling destination folder's own "having multiple
    // games" flatten decision catch up immediately - not just this game's
    // own removal.
    sys->getRootFolder()->removeFromVirtualFolders(sourceFile);
    delete sourceFile;

    if (view != nullptr)
      view.get()->repopulate();
  }
}

// static
FolderData* GuiFolderOptions::getFolderData(FolderData* folder, const std::string& name)
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
FolderData* GuiFolderOptions::resolveDescendant(FolderData* ancestor, const std::string& path, bool createIfMissing)
{
  if (path == ancestor->getPath())
    return ancestor;

  // 'path' is some descendant of 'ancestor', one or more directory levels
  // down - moveToFolder()'s own "down" picker can now offer a destination
  // buried several levels under the current folder, not just a direct
  // child of it, so this walks it one path component at a time instead of
  // a single getFolderData() lookup (which only ever looks at DIRECT
  // children). 'path' is always ancestor's own path plus one or more
  // "/name" components - every "down" destination is built that way, from
  // gameDir on down - so the substr below always lands past a "/".
  std::string relative = path.substr(ancestor->getPath().size() + 1);
  FolderData* current = ancestor;
  std::string currentPath = ancestor->getPath();

  size_t start = 0;
  while (start < relative.size() && current != nullptr)
  {
    size_t slash = relative.find('/', start);
    std::string segment = (slash == std::string::npos)
      ? relative.substr(start)
      : relative.substr(start, slash - start);

    currentPath += "/" + segment;

    FolderData* child = getFolderData(current, segment);
    if (child == nullptr && createIfMissing)
    {
      // This level exists on disk - every path this is ever called with
      // came from a real directory getChildFolders() found - but has no
      // tree node of its own yet, e.g. it's empty, or everything below it
      // is (see SystemData::populateFolder()). Add one so the rest of the
      // walk, and whatever ends up moved into it, has somewhere real in
      // the tree to attach to.
      child = new FolderData(currentPath.c_str(), current->getSystem());
      current->addChild(child);
    }

    current = child;
    start = (slash == std::string::npos) ? relative.size() : slash + 1;
  }

  return current;
}

// static
bool GuiFolderOptions::destinationHasNameCollision(FileData* file, const std::string& path)
{
  // Same ancestor-or-descendant resolution moveToFolder(FileData*, path)
  // itself uses to find (or create) 'fd' - here only to look at what a real
  // destination node already has, never to create one (resolveDescendant()
  // with createIfMissing=false just returns nullptr the moment any level of
  // the walk down to 'path' turns out untracked). A destination with no
  // tree node yet is skipped entirely rather than treated as "found nothing
  // to collide with" by accident - it's guaranteed empty anyway (see this
  // function's own header comment), so there's nothing to check.
  FolderData* parentDir = file->getParent();

  FolderData* destFolder = nullptr;
  for (FolderData* up = parentDir->getParent(); up != nullptr; up = up->getParent())
  {
    if (up->getPath() == path)
    {
      destFolder = up;
      break;
    }
  }

  if (destFolder == nullptr)
    destFolder = resolveDescendant(parentDir, path, false);

  if (destFolder == nullptr)
    return false;

  // Same extension-stripped comparison createFolder()'s own collision check
  // makes - a game's own name in the gamelist never includes one either.
  std::string movedName = remove_extension<std::string>(base_name<std::string>(file->getPath()));

  for (auto child : destFolder->getChildren())
    if (remove_extension<std::string>(base_name<std::string>(child->getPath())) == movedName)
      return true;

  return false;
}

// static
void GuiFolderOptions::createFolder(Window* window, FolderData* parentFolder, std::function<void()> onCreated)
{
  if (parentFolder == nullptr)
    return;

  // Same prompt-for-a-name-then-create flow this used to duplicate inline
  // in GuiGameOptions.cpp's "CREATE FOLDER" entry (and, before that, in
  // GuiFolderOptions's own now-removed screen) - one copy of it here instead.
  auto updateFN = [window, parentFolder, onCreated](const std::string& newVal)
  {
    if (newVal.empty())
      return;

    // Block a name that's already taken by anything the current gamelist
    // is showing for this folder - not just another folder already on
    // disk (checked below), but any sibling entry already in the tree,
    // folder or game alike. A game's own name is compared with its
    // extension stripped, since that's how it actually reads in the
    // gamelist - "Foo.zip" shows as "Foo" - so a new folder named "Foo"
    // would land right alongside it as a same-named entry, even though its
    // own path ("parentFolder/Foo") never collides with the game's
    // ("parentFolder/Foo.zip") on disk.
    bool nameTaken = false;
    for (auto child : parentFolder->getChildren())
    {
      if (remove_extension<std::string>(base_name<std::string>(child->getPath())) == newVal)
      {
        nameTaken = true;
        break;
      }
    }

    std::string path = parentFolder->getPath() + "/" + newVal;
    if (nameTaken || Utils::FileSystem::exists(path.c_str()))
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
void GuiFolderOptions::createFolder(FolderData* parentFolder, const std::string& path)
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

    // The folder tree ApiSystem::getChildFolders() caches for MOVE TO
    // FOLDER's picker just gained a new entry it doesn't know about yet.
    ApiSystem::getInstance()->addFolderToCache(sys->getStartPath(), path);

    // Only reflect the new folder in the in-memory tree (and so in the
    // current game playlist) when the user actually has folders showing at
    // all - "SHOW FOLDERS" set to "never" means no view will ever display
    // it, so there's nothing to gain by adding a tree node for it, only a
    // stray entry to keep in sync later. A mode like "having multiple
    // games" still counts as "enabled" here - whether this particular
    // (currently empty) folder is actually displayed under that mode is
    // handled separately, at display time, by whatever's building the
    // children list to show.
    if (sys->getFolderViewMode() != "never")
    {
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
}

// static
void GuiFolderOptions::renameFolder(Window* window, FolderData* folder, std::function<void()> onRenamed)
{
  if (folder == nullptr)
    return;

  std::string oldPath = folder->getPath();
  std::string parentPath = base_path<std::string>(oldPath);
  std::string oldName = base_name<std::string>(oldPath);

  // Same prompt-for-a-name-then-rename flow createFolder() above uses for
  // prompt-for-a-name-then-create - one copy of that shape here instead,
  // pre-filled with the current name (createFolder() has nothing to
  // pre-fill with, since the folder it's naming doesn't exist yet).
  auto updateFN = [window, folder, parentPath, oldName, onRenamed](const std::string& newVal)
  {
    if (newVal.empty() || newVal == oldName)
      return;

    // Same "FOLDER EXISTS" collision check createFolder() above runs
    // before creating a new folder, run here before renaming this one -
    // 'folder's own new siblings-to-be are its current parent's other
    // children, so check those (skipping 'folder' itself - pointless
    // comparing it against its own about-to-change name) the same way,
    // extension stripped so a same-named game blocks the rename too.
    bool nameTaken = false;
    if (folder->getParent() != nullptr)
    {
      for (auto sibling : folder->getParent()->getChildren())
      {
        if (sibling != folder &&
          remove_extension<std::string>(base_name<std::string>(sibling->getPath())) == newVal)
        {
          nameTaken = true;
          break;
        }
      }
    }

    std::string path = parentPath + "/" + newVal;
    if (nameTaken || Utils::FileSystem::exists(path.c_str()))
    {
      window->pushGui(new GuiMsgBox(window, _("FOLDER EXISTS"), _("OK"), nullptr));
      return;
    }

    renameFolder(folder, path);

    if (onRenamed)
      onRenamed();
  };

  if (Settings::getInstance()->getBool("UseOSK"))
    window->pushGui(new GuiTextEditPopupKeyboard(window, _("FOLDER NAME"), oldName, updateFN, false));
  else
    window->pushGui(new GuiTextEditPopup(window, _("FOLDER NAME"), oldName, updateFN, false));
}

// static
void GuiFolderOptions::renameFolder(FolderData* folder, const std::string& newPath)
{
  if (folder == nullptr || newPath == folder->getPath())
    return;

  // Same as createFolder(FolderData*, path) above: folder is always a
  // resolved, real tree node by the time it gets here, so its getSystem()
  // is already the real system - no getSourceFileData() unwrapping needed.
  auto sys = folder->getSystem();
	if (sys->isGroupChildSystem())
		sys = sys->getParentGroupSystem();
  auto view = ViewController::get()->getGameListView(sys, false);

  std::string oldPath = folder->getPath();

  // Rename the directory on disk first, same "mv" + log pattern
  // moveToFolder(FileData*, path) and confirmAndRemove() above already use
  // for their own filesystem changes.
  char cmdMvDir[1024];
  snprintf(cmdMvDir, sizeof(cmdMvDir), "mv \"%s\" \"%s\" 2>&1 | tee /emuelec/logs/mtf.log",
    oldPath.c_str(), newPath.c_str());
  system(cmdMvDir);

  // folder's own name just changed - same reason createFolder() /
  // moveToFolder(FileData*, path) above and confirmAndRemove() below each
  // do this after their own change to the folder tree.
  ApiSystem::getInstance()->moveFolderInCache(sys->getStartPath(), oldPath, newPath);

  // Patch the in-memory tree to match: folder's own path moves first, then
  // every descendant underneath it (each FileData node - game or folder -
  // keeps its own absolute path rather than one computed from its parent,
  // so renaming folder doesn't automatically fix up what's inside it).
  // Also update folder's own "Name" metadata to match - that's the actual
  // string the gamelist shows and writes out for it (see FileData::getName()),
  // and unlike mPath it's never derived from the path automatically, so
  // without this it would keep showing (and saving) the old name in the
  // current game playlist even though the folder itself moved. Marking
  // metadata dirty on every touched node is what gets both of those written
  // out the next time this system's gamelist is saved - the same flag
  // loadGamelistFile() sets on a freshly-parsed entry.
  folder->setPath(newPath);
  folder->setMetadata(MetaDataId::Name, base_name<std::string>(newPath));
  folder->getMetadata().setDirty();
  updateChildPaths(folder, oldPath, newPath);

  if (view != nullptr) {
    // Same lightweight in-place refresh createFolder() uses above - the
    // folder itself is still the same tree node, just under a new path, so
    // there's no cursor-matching concern like the one that rules out
    // reloadGameListView() elsewhere in this file.
    view.get()->repopulate();
    view->setCursor(folder);
  }
}

// static
void GuiFolderOptions::updateChildPaths(FolderData* folder, const std::string& oldPath, const std::string& newPath)
{
  for (auto child : folder->getChildren())
  {
    std::string childPath = child->getPath();
    if (childPath.compare(0, oldPath.size(), oldPath) == 0)
    {
      child->setPath(newPath + childPath.substr(oldPath.size()));
      child->getMetadata().setDirty();
    }

    if (child->getType() == FOLDER)
      updateChildPaths((FolderData*)child, oldPath, newPath);
  }
}

// static
void GuiFolderOptions::confirmAndRemove(Window* window, FolderData* folder, std::function<void()> onRemoved)
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
    _("YES"), [window, folder, path, onRemoved]
    {
      SystemData* sys = folder->getSystem();
      if (sys->isGroupChildSystem())
        sys = sys->getParentGroupSystem();

      auto view = ViewController::get()->getGameListView(sys, false);

      // 'folder's own parent is whatever gamelist folder is currently being
      // browsed as - captured now, before folder is detached from it below.
      FolderData* parentDir = folder->getParent();

      // Only worth stepping the view back a level when that parent is about
      // to have nothing left in it - same reasoning moveToFolder() above
      // uses for the same situation - AND it isn't the system's own ROM
      // root, since there's no "previous folder" to go back to from there;
      // an empty root just falls through to its usual "NO ENTRIES"
      // placeholder below, same as it always has.
      bool parentWillBeEmpty = parentDir != nullptr && parentDir->getChildren().size() == 1;
      bool parentIsBaseSystemPath = parentDir != nullptr &&
        Utils::FileSystem::getCanonicalPath(parentDir->getPath()) == Utils::FileSystem::getCanonicalPath(sys->getStartPath());

      ISimpleGameListView* simpleView = (view != nullptr) ? dynamic_cast<ISimpleGameListView*>(view.get()) : nullptr;
      bool browsingParent = simpleView != nullptr && parentDir != nullptr && simpleView->getCurrentFolder() == parentDir;

      // The other way this can be called (see GuiGameOptions.cpp's own
      // handling of the ".." back-entry): 'folder' isn't a sibling listed
      // inside whatever's currently browsed, it IS whatever's currently
      // browsed. There's nothing to leave in place either way - folder is
      // gone - so this always steps back at least one level, never falls
      // through to parentWillBeEmpty/parentIsBaseSystemPath below (those
      // only decide whether the OTHER case steps back); the ".." entry
      // itself never shows for the system's own ROM root (see
      // BasicGameListView - it needs a non-empty mCursorStack), so folder
      // always has a real parentDir here to land on.
      bool browsingFolder = simpleView != nullptr && simpleView->getCurrentFolder() == folder;

      char cmdRmDir[1024];
      snprintf(cmdRmDir, sizeof(cmdRmDir), "rm -rf \"%s\" 2>&1 | tee /emuelec/logs/mtf.log", path.c_str());
      system(cmdRmDir);

      // folder (and everything under it) is gone - same reason
      // createFolder() / moveToFolder(FileData*, path) / renameFolder()
      // above each do this after their own change to the folder tree.
      ApiSystem::getInstance()->removeFolderFromCache(sys->getStartPath(), path);

      if (browsingFolder && parentDir != nullptr)
      {
        // Same direct detach+goBack() as the parentWillBeEmpty case below,
        // just one level higher up: we're not leaving folder's parent for
        // lack of anything left in it, we're leaving folder itself because
        // it no longer exists at all. detach first and step back - in
        // that order - while folder is still a live object: goBack() reads
        // mCursorStack.top()->getParent() to find parentDir, and here
        // folder IS mCursorStack.top(), so deleting it before goBack() runs
        // would leave that call reading a freed object.
        // removeFromVirtualFolders() only erases folder out of parentDir's
        // own children list - it doesn't touch folder's own getParent(), so
        // that link is still good for goBack() to follow. Only once goBack()
        // is done with it is it actually safe to delete.
        sys->getRootFolder()->removeFromVirtualFolders(folder);
        static_cast<GoBackAccessor*>(simpleView)->goBack();
        delete folder;

        // Landed on parentDir. Per the requested behavior: stay there if
        // it's left with something in it (games or folders); otherwise
        // it's just as empty as folder was, so keep walking back up
        // through each ancestor that's *also* left with nothing in it,
        // stopping at the first one that still has something to show, or
        // at the system's own ROM root - goBack()'s own existing fallback
        // once mCursorStack empties, at which point getCurrentFolder()
        // returns nullptr and this loop simply stops running.
        FolderData* landedOn = simpleView->getCurrentFolder();
        while (landedOn != nullptr && landedOn->getChildren().size() == 0)
        {
          static_cast<GoBackAccessor*>(simpleView)->goBack();
          landedOn = simpleView->getCurrentFolder();
        }
      }
      else if (browsingParent && parentWillBeEmpty && !parentIsBaseSystemPath)
      {
        // Nothing will be left to show in the folder we're leaving - detach
        // and delete folder directly (same as the "no view" fallback below,
        // and the same reasoning moveToFolder() above uses for the same
        // situation) and step the view back up one level in place, instead
        // of refreshing it onto an empty list.
        sys->getRootFolder()->removeFromVirtualFolders(folder);
        delete folder;

        static_cast<GoBackAccessor*>(simpleView)->goBack();
      }
      else if (view != nullptr)
      {
        // Same pattern as everywhere else in this file (see
        // GuiGameOptions::deleteGame for the original precedent): hand the
        // removal to the view - it detaches/deletes and refreshes on its
        // own, recomputing whatever's currently displayed (an emptied
        // system root's placeholder included) from the now up-to-date tree.
        view.get()->remove(folder);
      }
      else
      {
        sys->getRootFolder()->removeFromVirtualFolders(folder);
        delete folder;
      }

      // folder is gone by now regardless of which branch above actually ran
      // - same spirit as onCreated/onRenamed elsewhere in this file, this
      // only fires once the removal actually happened, never on a NO or a
      // Back (those never reach this lambda at all).
      if (onRemoved)
        onRemoved();
    },
    _("NO"), nullptr));
}

// static
void GuiFolderOptions::rescanFolders(Window* window, FileData* file)
{
  auto sourceFile = file->getSourceFileData();
  auto sys = sourceFile->getSystem();
  if (sys->isGroupChildSystem())
    sys = sys->getParentGroupSystem();

  std::string systemRootPath = sys->getStartPath();
  std::string systemName = sys->getFullName();

  // Same "please wait" popup + background thread moveToFolder() above
  // shows the very first time a system's tree gets scanned - a forced
  // rescan pays exactly the same "find" cost, on purpose, whether or not
  // anything outside this app actually changed.
  window->pushGui(new GuiLoading<bool>(window,
    Utils::String::format(_("RESCANNING FOLDERS FOR %s...").c_str(), systemName.c_str()),
    [systemRootPath](auto /*gui*/)
    {
      ApiSystem::getInstance()->rescanFolderTree(systemRootPath);
      return true;
    },
    [window](bool /*rescanned*/)
    {
      window->pushGui(new GuiMsgBox(window, _("FOLDER DATA REBUILT"), _("OK")));
    }));
}
