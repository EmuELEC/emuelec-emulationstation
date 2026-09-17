#pragma once

#include "GuiSettings.h"
#include "FileData.h"
#include "GuiComponent.h"

#include <functional>
#include <string>

class Window;

// The "FOLDER OPTIONS" screen, pushed from GuiGameOptions.cpp for whatever
// game or folder is currently selected. Its own menu entries are each just
// a one-line call into one of the static, stateless helpers below - the
// same helpers a caller that doesn't want this whole screen (just one
// specific action) can call directly instead. Which entries actually show
// up depends on 'file's type: MOVE TO FOLDER only for a game, REMOVE FOLDER
// only for a folder, CREATE FOLDER for either.
class GuiMoveToFolder : public GuiSettings
{
public:
  GuiMoveToFolder(Window* window, FileData* file);

  // Shows the folder picker used by the "MOVE TO FOLDER" action (current
  // subfolders of 'file's own directory, plus a "go up a level" entry when
  // applicable), then a YES/NO confirmation before actually moving 'file'
  // to whichever folder was picked. Backing out of either window cancels
  // and leaves 'file' untouched. 'onMoved', if given, runs right after the
  // move actually happens - never before, and never if either step is
  // cancelled - so a caller can do something like close its own menu, the
  // same way GuiGameOptions::deleteGame()'s caller does for DELETE GAME.
  static void moveToFolder(Window* window, FileData* file, std::function<void()> onMoved = nullptr);

  // Actually performs the move: patches the in-memory FolderData tree
  // (creating the destination FolderData node first if the folder exists
  // on disk but doesn't have one yet - e.g. it's empty, or was just
  // created) and refreshes whichever gamelist is currently open for
  // file's system.
  static void moveToFolderGame(FileData* file, const std::string& path);

  // Prompts for a folder name (an OSK keyboard popup or a plain text popup,
  // matching the "UseOSK" setting - the same choice every other free-text
  // entry in this codebase makes), then creates it as a child of
  // 'parentFolder' via the createFolder(FolderData*, path) overload below.
  // Shows a "FOLDER EXISTS" message and creates nothing if a folder by that
  // name is already there. Backing out of the name popup cancels and
  // creates nothing either. 'onCreated', if given, runs right after the
  // folder is actually created - never on a cancel or a name collision -
  // same spirit as moveToFolder()'s onMoved above.
  static void createFolder(Window* window, FolderData* parentFolder, std::function<void()> onCreated = nullptr);

  // Creates the directory at 'path' on disk (a no-op if it already exists)
  // and adds it as a child of 'parentFolder' in the tree, refreshing
  // whichever gamelist is currently open for parentFolder's system. Takes
  // the parent folder directly rather than some FileData to derive it
  // from - a FolderData already IS a real tree node with its own system,
  // so there's nothing left to unwrap here. Callers that start from a raw
  // FileData* (which might be a CollectionFileData, e.g. an entry seen
  // through a custom collection / Favorites view) resolve it down to a
  // real FolderData* once, up front, via getSourceFileData()->getParent()
  // - see the overload above for exactly that pattern. Public on its own
  // (rather than folded entirely into the overload above) for a caller
  // that has already resolved a destination path some other way, e.g.
  // moveToFolderGame()'s "no tree node yet" fallback.
  static void createFolder(FolderData* parentFolder, const std::string& path);

  // Shows a YES/NO confirmation for deleting 'folder' (and everything
  // inside it). YES runs the removal command and updates whichever
  // gamelist is currently open for its system; NO (or Back) cancels and
  // the folder is left untouched.
  static void confirmAndRemove(Window* window, FolderData* folder);

private:
  // Looks up a direct child of 'folder' by name (matched against the last
  // path component of each child), returning it only if that child is
  // itself a folder. Returns nullptr if there's no such child. Used by
  // moveToFolderGame() to resolve a "move down" destination that already
  // has a tree node.
  static FolderData* getFolderData(FolderData* folder, const std::string& name);
};
