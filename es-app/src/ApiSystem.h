#ifndef API_SYSTEM
#define API_SYSTEM

#include <string>
#include <map>
#ifdef _ENABLEEMUELEC
#include <ctime>
#endif
#include "Window.h"
#include "components/BusyComponent.h"
#include "resources/TextureData.h"
#include "components/IExternalActivity.h"

struct BiosFile 
{
  std::string status;
  std::string md5;
  std::string path;
};

struct BiosSystem 
{
  std::string name;
  std::vector<BiosFile> bios;
};

struct BatoceraBezel
{
	std::string name;
	std::string url;
	std::string folderPath;
	bool isInstalled;
};

struct BatoceraTheme
{
	std::string name; 
	std::string url;  
	std::string author;
	std::string lastUpdate;
	int upToDate;
	int size;
	std::string image;
	
	bool isInstalled;
};

struct PacmanPackage
{
	PacmanPackage()
	{
		download_size = 0;
		installed_size = 0;
	}

	std::string name;
	std::string repository;
	std::string available_version;
	std::string description;
	std::string url;

	std::string packager;
	std::string status;

	size_t download_size;
	size_t installed_size;

	std::string preview_url;

	std::string group;
	std::vector<std::string> licenses;	

	std::string arch;

	bool isInstalled() { return status == "installed"; }
};

struct PadInfo
{
	int id;
	std::string name;
	std::string device;
	std::string status;
	std::string path;
	int battery;
};

struct Service
{
  std::string name;
  bool enabled;
};

class ApiSystem : public IPdfHandler, public IExternalActivity
{
public:
	enum LED_TYPE {
		LED_TYPE_NONE,
		LED_TYPE_UNIFIED,
		LED_TYPE_ADDRESSABLE
	};

	enum ScriptId : unsigned int
	{
		WIFI = 0,
		RETROACHIVEMENTS = 1,
		BLUETOOTH = 2,		
		RESOLUTION = 3,
		BIOSINFORMATION = 4,
		NETPLAY = 5,
		KODI = 6,
		GAMESETTINGS = 7,
		DECORATIONS = 8,
		SHADERS = 9,
		DISKFORMAT = 10,
		OVERCLOCK = 11,
		PDFEXTRACTION = 12,
		BATOCERASTORE = 13,
		EVMAPY = 14,
		THEMESDOWNLOADER = 15,
		THEBEZELPROJECT = 16,
		PADSINFO = 17,
		BATOCERAPREGAMELISTSHOOK = 18,
		TIMEZONES = 19,
		AUDIODEVICE = 20,
		BACKUP = 21,
		INSTALL = 22,
		SUPPORTFILE = 23,
		UPGRADE = 24,
		SUSPEND = 25,
		VERSIONINFO = 26,
		VIDEOFILTERS = 27,
		SERVICES = 28,
		READPLANEMODE = 29,
		WRITEPLANEMODE = 30,
		BACKGLASS = 31,
	};

	virtual bool isScriptingSupported(ScriptId script);

    static ApiSystem* getInstance();
	virtual void deinit() { };

    virtual unsigned long getFreeSpaceGB(std::string mountpoint);

    virtual std::string getFreeSpaceUserInfo();
    virtual std::string getFreeSpaceSystemInfo();
    std::string getFreeSpaceInfo(const std::string mountpoint);

    bool isFreeSpaceLimit();

    virtual std::string getVersion(bool extra = false);
	virtual std::string getApplicationName();

    std::string getRootPassword();

    bool setOverscan(bool enable);

    bool setOverclock(std::string mode);

    virtual std::pair<std::string, int> updateSystem(const std::function<void(const std::string)>& func = nullptr);

    std::pair<std::string, int> backupSystem(BusyComponent* ui, std::string device);
    std::pair<std::string, int> installSystem(BusyComponent* ui, std::string device, std::string architecture);
    std::pair<std::string, int> scrape(BusyComponent* ui);

    virtual bool ping();
    virtual bool canUpdate(std::vector<std::string>& output);
	virtual void setReadyFlag(bool ready = true);
	virtual bool isReadyFlagSet();

    virtual bool launchKodi(Window *window);
    bool launchFileManager(Window *window);

    bool enableWifi(std::string ssid, std::string key);
    bool disableWifi();

	virtual std::string getIpAddress();

	// BlueTooth methods
	virtual bool enableBluetooth();
	virtual bool disableBluetooth();
	virtual void startBluetoothLiveDevices(const std::function<void(const std::string)>& func);
	virtual void stopBluetoothLiveDevices();
	virtual bool pairBluetoothDevice(const std::string& deviceName);
	virtual bool connectBluetoothDevice(const std::string& deviceName);
	virtual bool disconnectBluetoothDevice(const std::string& deviceName);
	virtual bool removeBluetoothDevice(const std::string& deviceName);
	virtual bool forgetBluetoothControllers();
	virtual std::vector<std::string> getPairedBluetoothDeviceList();	
    virtual bool scanNewBluetooth(const std::function<void(const std::string)>& func = nullptr); // Obsolete

    std::vector<std::string> getAvailableBackupDevices();
    std::vector<std::string> getAvailableInstallDevices();
    std::vector<std::string> getAvailableInstallArchitectures();
    std::vector<std::string> getAvailableOverclocking();
    std::vector<BiosSystem> getBiosInformations(const std::string system = "");
    virtual std::vector<std::string> getVideoModes(const std::string output = "");
	std::vector<std::string> getCustomRunners();

	virtual std::vector<std::string> getAvailableStorageDevices();
	virtual std::vector<std::string> getSystemInformations();

    bool generateSupportFile();

    std::string getCurrentStorage();

    bool setStorage(std::string basic_string);

	bool setButtonColorGameForce(std::string basic_string);

	bool setPowerLedGameForce(std::string basic_string);    

    /* audio card */
    bool setAudioOutputDevice(std::string device);
    std::vector<std::string> getAvailableAudioOutputDevices();
    std::string getCurrentAudioOutputDevice();
    bool setAudioOutputProfile(std::string profile);
    std::vector<std::string> getAvailableAudioOutputProfiles();
    std::string getCurrentAudioOutputProfile();

    /* video output */
    std::vector<std::string> getAvailableVideoOutputDevices();

	// Themes
	virtual std::vector<BatoceraTheme> getBatoceraThemesList();
	virtual bool isThemeInstalled(const std::string& themeName, const std::string& url);
	virtual std::pair<std::string,int> installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string, int> uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);

    virtual std::vector<BatoceraBezel> getBatoceraBezelsList();
	virtual std::pair<std::string,int> installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);
	virtual std::pair<std::string,int> uninstallBatoceraTheme(std::string bezelsystem, const std::function<void(const std::string)>& func = nullptr);

	virtual std::string getCRC32(const std::string fileName, bool fromZipContents = true);
	virtual std::string getMD5(const std::string fileName, bool fromZipContents = true);

	virtual bool unzipFile(const std::string fileName, const std::string destFolder = "", const std::function<bool(const std::string)>& shouldExtract = nullptr);

	virtual int getPdfPageCount(const std::string& fileName);
	virtual std::vector<std::string> extractPdfImages(const std::string& fileName, int pageIndex = -1, int pageCount = 1, int quality = 0);

	virtual std::string getRunningArchitecture();
	virtual std::string getRunningBoard();

	std::vector<PacmanPackage> getBatoceraStorePackages();
	std::pair<std::string, int> installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
	std::pair<std::string, int> uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
	void updateBatoceraStorePackageList();
	void refreshBatoceraStorePackageList();

	void callBatoceraPreGameListsHook();

	bool	getBrightness(int& value);
	void	setBrightness(int value);

	// LED RGB sliders
	bool getLED(int& red, int& green, int& blue);
	void getLEDColours(int& red, int& green, int& blue);
	void setLEDColours(int red, int green, int blue);

	// LED Enabled?
	bool isLEDEnabled();
	void setLEDEnabled(bool enabled);

	// LED Brightness
	bool getLEDBrightness(int& value);
	void setLEDBrightness(int value);

	std::vector<std::string> getWifiNetworks(bool scan = false);

	bool downloadFile(const std::string url, const std::string fileName, const std::string label = "", const std::function<void(const std::string)>& func = nullptr);
	
	// Formating
	std::vector<std::string> getFormatDiskList();
	std::vector<std::string> getFormatFileSystems();
	int formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func = nullptr);


	virtual std::vector<std::string> getRetroachievementsSoundsList();
	virtual std::vector<std::string> getVideoFilterList(const std::string& systemName, const std::string& emulator, const std::string& core);
	virtual std::vector<std::string> getShaderList(const std::string& systemName, const std::string& emulator, const std::string& core);
	virtual std::string getSevenZipCommand() { return "7zr"; }

	virtual std::vector<std::string> getTimezones();
	virtual std::string getCurrentTimezone();
	virtual bool setTimezone(std::string tz);

	virtual std::vector<PadInfo> getPadsInfo();
	virtual std::string getHostsName();
	virtual bool emuKill();
	virtual void suspend();

  	virtual void replugControllers_sindenguns();
	virtual void replugControllers_wiimotes();
	virtual void replugControllers_steamdeckguns();

	virtual bool isPlaneMode();
	virtual bool setPlaneMode(bool enable);
	virtual bool isReadPlaneModeSupported();

	virtual std::vector<Service> getServices();
	virtual bool enableService(std::string name, bool enable);

	virtual std::vector<std::string> backglassThemes();
	virtual void restartBackglass();

#ifdef _ENABLEEMUELEC
  // Every folder under 'path', however many directory levels down - not
  // just its direct children. 'path' can be the system's own root ROM
  // folder (as opposed to some subfolder a user created inside it), to get
  // its whole tree, or any folder inside that tree, to get just what's
  // under that one - either way this is served out of 'systemRootPath's
  // own cached folder tree (see the .cpp): the first call for a given
  // system scans its ROM folder with "find" - a real cost on slow storage
  // (an SD card, a network share) - and writes what it found to a
  // "folders.xml" file right in 'systemRootPath', so most calls after that
  // just read that file back instead of re-scanning. The one exception is
  // when that cache has actually fallen out of sync with what's really on
  // disk - a folder added, renamed, or removed by hand outside
  // EmulationStation entirely (over SSH/SFTP, from a USB stick, etc.), not
  // through this file's own CREATE/MOVE/RENAME/REMOVE actions (those keep
  // the cache in sync themselves as they happen - see addFolderToCache()
  // and friends below) - which a quick mtime check (see
  // isFolderTreeStale() in the .cpp) catches on every call, and pays for
  // one fresh "find" scan to catch back up. Only the reserved names right
  // under 'systemRootPath' itself (media/medias, images, manuals, videos,
  // assets, artwork, downloaded_*, anything hidden - see the .cpp) are
  // ever left out of the cached tree, since those only ever sit alongside
  // the ROMs at that top level, never as - or inside - a folder the user
  // made further down to organize games.
  virtual std::vector<std::string> getChildFolders(std::string path, std::string systemRootPath);

  // Adds 'newFolderPath' to 'systemRootPath's own cached "folders.xml"
  // (see getChildFolders() above), and refreshes whichever mtime its own
  // parent folder now has (the system ROM root's own, if that's what it
  // is) to match - called from GuiFolderOptions::createFolder() right
  // after it creates the directory on disk, so a MOVE TO FOLDER / CREATE
  // FOLDER / REMOVE FOLDER opened right after still finds it without
  // paying for a fresh "find" scan of the whole system just to pick up the
  // one new folder, and getChildFolders()'s own staleness check right
  // above doesn't mistake this in-app change for an outside one on its
  // very next call. A no-op if 'systemRootPath' has no cached tree yet
  // (nothing to keep in sync) - getChildFolders() builds one, already
  // reflecting this folder straight off disk, the first time it's
  // actually asked for.
  void addFolderToCache(std::string systemRootPath, std::string newFolderPath);

  // Rewrites every cached entry equal to, or nested under, 'oldPath' to
  // sit under 'newPath' instead, and refreshes whichever mtime(s) oldPath's
  // old parent and newPath's new one now have (same reasoning
  // addFolderToCache() above gives - either can be the system ROM root
  // itself) - called from GuiFolderOptions.cpp's own MOVE TO FOLDER
  // (moving a FOLDER, not a GAME) and RENAME FOLDER right after each
  // moves/renames the directory on disk. Both are the same operation from
  // the cache's point of view: a subtree's own root path changes, and
  // everything cached under it moves with it. Same no-op case as
  // addFolderToCache() above when there's no cached tree yet.
  void moveFolderInCache(std::string systemRootPath, std::string oldPath, std::string newPath);

  // Drops 'removedPath', and everything cached under it, from
  // 'systemRootPath's own cached tree, and refreshes whichever mtime its
  // own (former) parent folder now has (same reasoning addFolderToCache()
  // above gives) - called from GuiFolderOptions::confirmAndRemove() right
  // after its "rm -rf" removes the directory (and everything inside it)
  // from disk. Same no-op case as addFolderToCache() above when there's no
  // cached tree yet.
  void removeFolderFromCache(std::string systemRootPath, std::string removedPath);

  // Forces a fresh "find" scan of 'systemRootPath's whole folder tree and
  // rewrites its folders.xml from that, unconditionally - the on-demand
  // counterpart to getChildFolders()'s own automatic staleness check
  // above, for anyone who's just made a change outside this app entirely
  // (over SSH/SFTP, from a USB stick, etc.) and doesn't want to wait for,
  // or isn't sure will actually catch, that automatic check before the
  // next MOVE TO FOLDER. Called from GuiFolderOptions' own "RESCAN
  // FOLDERS" action. Unlike the other three above, this never no-ops -
  // it scans and (re)writes folders.xml regardless of whether one already
  // existed, same as getChildFolders() itself does the very first time.
  void rescanFolderTree(std::string systemRootPath);

  // Refreshes 'folderPath's own stored mtime in this system's
  // folders.xml cache (or the cache's separate root mtime, if
  // 'folderPath' canonicalizes to the system's own ROM root) to match
  // what's actually on disk right now, in place - without adding,
  // removing or moving anything in the cached path list itself, unlike
  // addFolderToCache() / moveFolderInCache() / removeFolderFromCache()
  // above. Those three only ever run for a FOLDER changing hands, so a
  // plain GAME move never touches the cache at all through them - but
  // the move still changes its old folder's and its new folder's mtimes
  // on disk (a file left one and landed in the other), which is exactly
  // what isFolderTreeStale() below checks for on the very next call.
  // Call this once for each folder whose direct contents actually
  // changed - the game's old folder, its new one, or both - right after
  // the move, so that in-app change doesn't look like an outside edit
  // and force a fresh "find" scan it doesn't need. No-ops the same way
  // the other three do when there's no cached tree yet (nothing to keep
  // in sync), and also when 'folderPath' itself isn't a tracked entry
  // (nothing there to refresh).
  void refreshFolderMtime(std::string systemRootPath, std::string folderPath);

  // Builds and writes this system's folders.xml cache from a folder list
  // the caller already has in hand, rather than discovering it here with
  // scanSystemFolderTree()'s own "find" calls. SystemData::SystemData()
  // calls this right after populateFolder() walks a system's ROM tree in
  // memory to build its FolderData children in the first place - the exact
  // same folders (same reserved-name exclusions) scanSystemFolderTree()
  // would otherwise have to rediscover itself the first time the user opens
  // MOVE TO FOLDER for that system - so folders.xml ends up already built,
  // and already fresh, by the time a system finishes loading, at the cost
  // of only the one stat() per folder this still needs for its own mtime
  // bookkeeping (fanned out across a thread pool, same as everywhere else
  // in this cache). Always (re)writes folders.xml from 'knownFolderPaths'
  // as given - since the caller just walked the real filesystem to produce
  // it, there's nothing to gain by reading the old cache back first to
  // compare against it.
  void primeFolderTreeFromKnownFolders(std::string systemRootPath, const std::vector<std::string>& knownFolderPaths);
#endif

protected:
	ApiSystem();

	virtual bool executeScript(const std::string command);  
	virtual std::pair<std::string, int> executeScript(const std::string command, const std::function<void(const std::string)>& func);
	virtual std::vector<std::string> executeEnumerationScript(const std::string command);
	virtual bool downloadGitRepository(const std::string& url, const std::string& branch, const std::string& fileName, const std::string& label, const std::function<void(const std::string)>& func, int64_t defaultDownloadSize = 0);
	virtual std::string getGitRepositoryDefaultBranch(const std::string& url);
		
	virtual std::string getUpdateUrl();
	virtual std::string getThemesUrl();

#ifdef _ENABLEEMUELEC
	// One cached folder's own path, and the mtime it actually had on disk
	// the moment this was last written to folders.xml (see
	// getChildFolders() above). isFolderTreeStale() below compares that
	// against the real filesystem on every call, and addFolderToCache() /
	// moveFolderInCache() / removeFolderFromCache() each refresh it (or the
	// system ROM root's own separate mtime, tracked alongside the whole
	// list rather than as one of its entries) right after their own change
	// - so a normal in-app CREATE/MOVE/RENAME/REMOVE never looks like an
	// outside edit to the very next lookup.
	struct FolderCacheEntry
	{
		std::string path;
		time_t mtime;
	};

	// The actual "find"-based scan getChildFolders() only ever has to run
	// once per system, the very first time folders.xml (see getChildFolders()
	// above) doesn't exist yet for it, or isFolderTreeStale() below finds
	// its cached tree no longer matches what's actually on disk. First lists
	// 'systemRootPath's own direct children and drops the reserved ones
	// (media, images, etc) in C++, then recursively walks each surviving
	// top-level folder with its own "find <folder> -type d -print" -
	// deliberately plain, portable syntax, just paths, no mtimes - fanned
	// out across a small thread pool so a ROM library with several
	// top-level folders walks them all at once instead of one "find"
	// process working through the whole tree alone. (An earlier version
	// used "-noleaf"/"-printf" to fold mtime collection into a single
	// whole-tree "find" pass, both GNU-findutils-specific options this
	// device's "find" doesn't recognize - and since it refuses to run at
	// all on an option it doesn't understand, and executeEnumerationScript()
	// only captures stdout, not stderr, that came back as a silently empty
	// result rather than a visible error.) rebuildFolderTree() below is what
	// fills in each FolderCacheEntry::mtime afterward, one stat() per result.
	std::vector<std::string> scanSystemFolderTree(const std::string& systemRootPath);

	// Reads a system's cached folder tree back from its folders.xml, along
	// with 'rootMtime' - the system ROM root's own mtime at the moment this
	// was last written. Sets 'ok' to whether the file actually parsed - a
	// genuinely empty list (a system with no subfolders at all) is a valid,
	// ok=true result that getChildFolders() should trust as-is (once also
	// checked for staleness - see isFolderTreeStale() below), not treat as
	// "never cached" and re-scan; ok=false is only for a missing or
	// corrupted file, which it does re-scan.
	std::vector<FolderCacheEntry> readSystemFolderTree(const std::string& xmlPath, time_t& rootMtime, bool& ok);

	// Writes 'folders' - and 'rootMtime', the system ROM root's own mtime
	// right now - out to 'xmlPath' as getChildFolders()'s cache for the
	// system rooted where that path lives.
	void writeSystemFolderTree(const std::string& xmlPath, const std::vector<FolderCacheEntry>& folders, time_t rootMtime);

	// Used by isFolderTreeStale() below to make sense of 'systemRootPath's
	// own mtime having changed: that mtime bumps for ANY direct child
	// appearing or disappearing, including a reserved one (media, images,
	// downloaded_*, etc) scanSystemFolderTree() already excludes from this
	// cache entirely - most commonly a scraper creating "media"/"images" the
	// first time it saves artwork for a game in this system, which has
	// nothing to do with the folder list this cache actually tracks. Re-runs
	// the same cheap, non-recursive top-level listing and reserved-name
	// filter scanSystemFolderTree()'s own first step uses, and compares the
	// surviving set against 'master's own top-level entries (those whose
	// parent is 'systemRootPath' itself) - true only if a real, tracked
	// top-level folder actually appeared or disappeared.
	bool topLevelFolderSetChanged(const std::string& systemRootPath, const std::vector<FolderCacheEntry>& master);

	// True if anything under 'systemRootPath' has actually changed since
	// 'master' and 'rootMtime' were written to folders.xml by something
	// other than this file's own CREATE/MOVE/RENAME/REMOVE actions (those
	// keep both in sync themselves as they happen, so they never trip this
	// - see addFolderToCache() and friends above) - most likely a folder
	// added, renamed, or removed by hand outside EmulationStation entirely,
	// e.g. over SSH/SFTP or from a USB stick. Checks 'systemRootPath's own
	// current mtime against 'rootMtime' first, and if that's moved on,
	// confirms with topLevelFolderSetChanged() above that a real top-level
	// folder is actually behind it rather than incidental churn (like a
	// scraper creating "media" for the first time) before treating that
	// alone as reason enough to rebuild - then checks every entry in
	// 'master' (fanned out across a thread pool, same as everywhere else in
	// this cache - this runs on every getChildFolders() lookup now, not just
	// the first one, so it needs to stay cheap even on a system with a lot
	// of folders) - that it still exists at all, and that its own current
	// mtime still matches what's stored (catches the same two things
	// happening anywhere deeper in the tree, a folder having been removed
	// outright, or one of its own children changing in a way that would
	// reveal a new folder underneath it) - any mismatch found either way is
	// already reason enough for getChildFolders() to fall back to a fresh
	// "find" scan rather than trust a tree that's fallen out of sync with
	// what's actually on disk.
	bool isFolderTreeStale(const std::string& systemRootPath, time_t rootMtime, const std::vector<FolderCacheEntry>& master);

	// Shared by rebuildFolderTree() below and the public
	// primeFolderTreeFromKnownFolders() above: stats every path in
	// 'folderPaths' to fill in its own FolderCacheEntry::mtime (fanned out
	// across a thread pool - on a big system this can be just as much work
	// as finding the folders in the first place), records 'systemRootPath's
	// own current mtime alongside them, writes it all out to 'xmlPath' as
	// the new cache, and hands back the same list so a caller that already
	// has it in hand doesn't have to turn around and read back what it just
	// wrote. Always writes - callers only ever reach this once they've
	// already decided the cache needs rebuilding (or, for
	// primeFolderTreeFromKnownFolders(), once they've already walked the
	// real filesystem to produce 'folderPaths'), so there's nothing to gain
	// by reading the old cache back first to compare against it.
	std::vector<FolderCacheEntry> buildAndWriteFolderTree(const std::string& systemRootPath, const std::string& xmlPath, const std::vector<std::string>& folderPaths);

	// Actually performs the rescan-and-write getChildFolders() above falls
	// back to whenever its cached tree is missing, corrupted, or stale (see
	// isFolderTreeStale() above), and rescanFolderTree() above runs
	// unconditionally on demand: scans 'systemRootPath' fresh with
	// scanSystemFolderTree(), then hands the result straight to
	// buildAndWriteFolderTree() above to stat, record, and write out as
	// 'xmlPath's new cache.
	std::vector<FolderCacheEntry> rebuildFolderTree(const std::string& systemRootPath, const std::string& xmlPath);
#endif

    static ApiSystem* instance;

    void launchExternalWindow_before(Window *window);
    void launchExternalWindow_after(Window *window);

private:
	static LED_TYPE mSystemLedType;
};

#endif
