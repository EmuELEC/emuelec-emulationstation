#pragma once

#include "GuiSettings.h"
#include "FileData.h"
#include "GuiComponent.h"

#include <functional>
#include <string>
#include <vector>

class Window;

// The "FOLDER OPTIONS" screen, pushed from GuiGameOptions.cpp for whatever
// game or folder is currently selected. Its own menu entries are each just
// a one-line call into one of the static, stateless helpers below - the
// same helpers a caller that doesn't want this whole screen (just one
// specific action) can call directly instead. Which entries actually show
// up depends on 'file's type: MOVE TO FOLDER for either a game or a folder,
// REMOVE FOLDER and RENAME FOLDER only for a folder, CREATE FOLDER for
// either.
class GuiFolderOptions : public GuiSettings
{
public:
  GuiFolderOptions(Window* window, FileData* file);

  // Shows the folder picker used by the "MOVE TO FOLDER" action for either
  // a game or a folder (every folder under 'file's own directory, however
  // many levels down - never 'file' itself, or anything still nested under
  // it, when 'file' is a folder - plus a "go up" entry for every ancestor
  // folder above it, as far as the system's own ROM root). A destination
  // more than one level down shows with its path relative to 'file's own
  // directory, slashes and all (e.g. "SubA/SubB"), so it reads the same way
  // it'll actually land rather than just its own bare folder name, which
  // could otherwise be confused with some other, same-named folder
  // elsewhere in the tree. Picking one shows a "FOLDER EXISTS" message and
  // moves nothing if that destination already has an entry named the same
  // as 'file' - same collision check createFolder() above runs before
  // creating a new folder, just run here before moving one - otherwise a
  // YES/NO confirmation before actually moving 'file' there. Backing out of
  // either window, or the name collision, cancels and leaves 'file'
  // untouched. 'onMoved', if given, runs right after the move actually
  // happens - never before, and never if any of those steps didn't go
  // through - so a caller can do something like close its own menu, the
  // same way GuiGameOptions::deleteGame()'s caller does for DELETE GAME.
  static void moveToFolder(Window* window, FileData* file, std::function<void()> onMoved = nullptr);

  // Actually performs the move, for a game or a folder alike: patches the
  // in-memory FolderData tree (creating every destination FolderData node
  // between file's current folder and the destination that exists on disk
  // but doesn't have one yet - e.g. it's empty, or was just created) and
  // refreshes whichever gamelist is currently open for file's system. When
  // 'file' is itself a folder, it - and everything still attached beneath
  // it - is reused as-is under its new parent rather than rebuilt, the same
  // way renameFolder(FolderData*, newPath) below reuses it for an in-place
  // rename (moving is really a rename to a new parent). 'path' can be any
  // folder the overload above's picker offered - one or more levels inside
  // file's current folder, or any ancestor above it up to the system's own
  // ROM root, not just the immediate parent either way. Public on its own
  // (rather than folded entirely into the overload above), same spirit as
  // createFolder(FolderData*, path)'s own reason for being public - a
  // caller that has already resolved a destination path some other way can
  // call straight into this.
  static void moveToFolder(FileData* file, const std::string& path);

  // Prompts for a folder name (an OSK keyboard popup or a plain text popup,
  // matching the "UseOSK" setting - the same choice every other free-text
  // entry in this codebase makes), then creates it as a child of
  // 'parentFolder' via the createFolder(FolderData*, path) overload below.
  // Shows a "FOLDER EXISTS" message and creates nothing if the name is
  // already taken - by another folder on disk, or by anything the current
  // gamelist is already showing for 'parentFolder' (a game's own entry
  // counts too, compared with its extension stripped, since that's how it
  // actually reads in the list). Backing out of the name popup cancels and
  // creates nothing either. 'onCreated', if given, runs right after the
  // folder is actually created - never on a cancel or a name collision -
  // same spirit as moveToFolder()'s onMoved above.
  static void createFolder(Window* window, FolderData* parentFolder, std::function<void()> onCreated = nullptr);

  // Creates the directory at 'path' on disk (a no-op if it already exists).
  // Only adds it as a child of 'parentFolder' in the tree - and so to the
  // current game playlist - and refreshes whichever gamelist is currently
  // open for parentFolder's system, when the user actually has "SHOW
  // FOLDERS" enabled (anything other than "never"); otherwise the directory
  // is created but nothing in the tree or gamelist reflects it. Takes the
  // parent folder directly rather than some FileData to derive it from - a
  // FolderData already IS a real tree node with its own system,
  // so there's nothing left to unwrap here. Callers that start from a raw
  // FileData* (which might be a CollectionFileData, e.g. an entry seen
  // through a custom collection / Favorites view) resolve it down to a
  // real FolderData* once, up front, via getSourceFileData()->getParent()
  // - see the overload above for exactly that pattern. Public on its own
  // (rather than folded entirely into the overload above) for a caller
  // that has already resolved a destination path some other way, e.g.
  // moveToFolder(FileData*, path)'s "no tree node yet" fallback.
  static void createFolder(FolderData* parentFolder, const std::string& path);

  // Prompts for a new name (an OSK keyboard popup or a plain text popup,
  // matching the "UseOSK" setting, same as createFolder() above), pre-filled
  // with 'folder's current name, then renames it as a sibling of itself via
  // the renameFolder(FolderData*, path) overload below. Shows a "FOLDER
  // EXISTS" message and renames nothing if the new name is already taken -
  // by another folder on disk, or by anything the current gamelist is
  // already showing among 'folder's other siblings (a game's own entry
  // counts too, compared with its extension stripped, same collision check
  // createFolder() above runs). Backing out of the name popup, or
  // submitting the same name it already had, cancels and renames nothing
  // either. 'onRenamed', if given, runs right after the rename actually
  // happens - never on a cancel or a name collision - same spirit as
  // createFolder()'s onCreated above.
  static void renameFolder(Window* window, FolderData* folder, std::function<void()> onRenamed = nullptr);

  // Renames the directory backing 'folder' on disk to 'newPath' (a sibling
  // of its current location) and patches the in-memory tree to match:
  // 'folder's own path is updated, and so is every descendant's - each
  // FileData node stores its own absolute path rather than one computed
  // from its parent (see moveToFolder(FileData*, path) above for the same
  // quirk), so the whole subtree under 'folder' needs walking, not just
  // 'folder' itself. 'folder's own "Name" metadata - the string actually
  // shown for it, and written out, in the current game playlist - is
  // updated to match too, since that's never derived from the path on its
  // own. Every touched node's metadata is marked dirty so the rename is
  // picked up the next time its system's gamelist (the game playlist) is
  // saved. Refreshes whichever gamelist is currently open for folder's
  // system. A no-op if 'newPath' is the same as folder's current path.
  static void renameFolder(FolderData* folder, const std::string& newPath);

  // Shows a YES/NO confirmation for deleting 'folder' (and everything
  // inside it). YES runs the removal command and updates whichever gamelist
  // is currently open for its system - stepping that view back a level in
  // either of two cases: when 'folder' is itself the one currently being
  // browsed (reached via its own ".." back-entry in GuiGameOptions.cpp,
  // since there's then nothing left to refresh in place - the folder we
  // were looking at is simply gone) - and, from there, kept walking back
  // through each ancestor in turn as long as the one just landed on is
  // itself left with nothing in it (no games, no folders), stopping at the
  // first ancestor that still has something to show, or at the system's
  // own ROM root if every ancestor up to it turned out just as empty; or
  // when 'folder' is a sibling listed inside the one currently being
  // browsed and removing it leaves that parent with nothing left in it,
  // same lightweight in-place navigation moveToFolder() above steps back
  // with when a move empties the folder it's leaving - unless that parent
  // is the system's own ROM root, since there's nothing above that to go
  // back to, so an emptied root just shows its usual "NO ENTRIES"
  // placeholder instead, same as it always has (this second case only ever
  // steps back that one level - the parent it lands on always still has at
  // least the emptied sibling itself sitting in it, so there's nothing
  // further to walk back through). Neither case can arise for 'folder'
  // being the system's own ROM root itself - there's always a real folder
  // to land on either way. NO (or
  // Back) cancels and the folder is left untouched. 'onRemoved', if given,
  // runs right after the removal actually happens - never on a NO or a
  // Back - same spirit as createFolder()'s onCreated above.
  static void confirmAndRemove(Window* window, FolderData* folder, std::function<void()> onRemoved = nullptr);

  // Forces 'file's system's own cached folder tree (see
  // ApiSystem::getChildFolders() in ApiSystem.h/.cpp) to be rebuilt right
  // now, straight off disk, rather than waiting for that cache's own
  // automatic staleness check to catch a change made outside this app
  // entirely - e.g. right after adding, renaming, or removing a folder
  // over SSH/SFTP or from a USB stick, before ever opening MOVE TO FOLDER
  // again. Shows a "please wait" popup while the scan runs, same reason
  // moveToFolder() above shows one the very first time a system's tree is
  // scanned - a full rescan pays exactly the same cost, on purpose,
  // whether or not anything outside this app actually changed - then a
  // confirmation once it's done. Leaves this screen open afterwards,
  // unlike CREATE/RENAME/REMOVE FOLDER above, since this doesn't do
  // anything to 'file' itself.
  static void rescanFolders(Window* window, FileData* file);

private:
  // Looks up a direct child of 'folder' by name (matched against the last
  // path component of each child), returning it only if that child is
  // itself a folder. Returns nullptr if there's no such child. Used by
  // resolveDescendant() below to resolve one level of its own walk at a
  // time.
  static FolderData* getFolderData(FolderData* folder, const std::string& name);

  // Resolves 'path' - 'ancestor's own path plus one or more "/name"
  // components, e.g. a "move down" destination moveToFolder()'s own picker
  // offered - to its FolderData node, walking one path component at a time
  // via getFolderData() above rather than a single lookup, since 'path' no
  // longer has to be a direct child of 'ancestor' (the picker can now offer
  // one buried several levels down). When 'createIfMissing' is true, any
  // level along the way that exists on disk but has no tree node of its own
  // yet - e.g. it's empty, or everything below it is - gets one created (see
  // SystemData::populateFolder()); when false, the walk stops and returns
  // nullptr the moment it hits an untracked level, since a destination
  // that's never been resolved-and-created this way can't have anything
  // tracked underneath it either. Returns 'ancestor' itself, unchanged, if
  // 'path' is exactly its own path.
  static FolderData* resolveDescendant(FolderData* ancestor, const std::string& path, bool createIfMissing);

  // Walks every descendant of 'folder' (games and subfolders alike) and, for
  // each one whose stored path actually lives under 'oldPath', rewrites that
  // path to live under 'newPath' instead, marking its metadata dirty so the
  // gamelist picks up the change on next save. Used by renameFolder() to
  // keep the whole subtree consistent after the folder itself is renamed,
  // and by moveToFolder(FileData*, path) for the same reason after a moved
  // folder's own path changes to reflect its new parent.
  static void updateChildPaths(FolderData* folder, const std::string& oldPath, const std::string& newPath);

  // True if 'path' - a destination moveToFolder()'s own picker offered -
  // already has, at the tree level, an entry named the same as 'file' (its
  // extension stripped, same comparison createFolder()'s own collision
  // check above makes). Resolves 'path' the same way moveToFolder(FileData*,
  // path) itself does (an ancestor of file's current folder, or a descendant
  // of it, however many levels down), but only to look at what's already
  // there, never to create anything - a destination that doesn't have a
  // tree node yet can't have any children to collide with, since
  // SystemData::populateFolder() only ever leaves a folder untracked when it
  // has none.
  static bool destinationHasNameCollision(FileData* file, const std::string& path);

  // The rest of what moveToFolder(Window*, FileData*, ...) above used to do
  // in one go, once 'folders' - its system's whole cached folder tree, from
  // ApiSystem::getChildFolders() - is actually in hand: builds the ancestor
  // ("move up", however many ".." levels back) and descendant ("move down")
  // destination options for 'file' and shows the picker itself. Split out
  // on its own purely so that function can show a "building folder data"
  // popup and run getChildFolders() on a background thread first, the very
  // first time a system's folders.xml doesn't exist yet (see its own
  // comment there), without duplicating everything that happens once the
  // folder list is finally in hand either way - fetched instantly from that
  // cache, or freshly scanned. 'gameDir' and 'lastFolder' are exactly what
  // moveToFolder(Window*, FileData*, ...) already computed before fetching
  // 'folders' - passed through rather than recomputed here.
  static void showMoveToFolderPicker(Window* window, FileData* file, std::function<void()> onMoved, const std::string& gameDir, const std::string& lastFolder, const std::vector<std::string>& folders);
};
