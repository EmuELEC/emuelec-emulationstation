#include "ApiSystem.h"
#include "Settings.h"
#include "Log.h"
#include "HttpReq.h"
#include "AudioManager.h"
#include "VolumeControl.h"
#include "InputManager.h"
#include "EmulationStation.h"
#include "SystemConf.h"
#include "Sound.h"
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/ThreadPool.h"
#include "RetroAchievements.h"
#include "utils/ZipFile.h"
#include "Paths.h"
#include "utils/VectorEx.h"
#include "LocaleES.h"

#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <set>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <fstream>
#include <SDL.h>
#include <pugixml/src/pugixml.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>

#if WIN32
#include <Windows.h>
#define popen _popen
#define pclose _pclose
#define WIFEXITED(x) x
#define WEXITSTATUS(x) x
#include "Win32ApiSystem.h"
#else
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#endif

/*
#define script_config "batocera-config"; // canupdate, overscan enable, overscan disable, storage 'X', storage current, storage list, forgetBT, getRootPassword, lsoutputs
#define script_overclock "batocera-overclock"; // list, set X
#define script_upgrade "batocera-upgrade";
#define script_sync "batocera-sync"; // sync
#define script_install "batocera-install"; // listDisks, listArchs, install X Y
#define script_scraper "batocera-scraper";
#define script_kodi "batocera-kodi";
#define script_wifi "batocera-wifi"; // scanlist, list, enable X Y, disable
#define script_bluetooth "batocera-bluetooth"; // trust, list, remove 
#define script_resolution "batocera-resolution"; // listModes
#define script_sync "batocera-sync";   // list
#define script_info "batocera-info"; // --full
#define script_systems "batocera-systems"; // --filter
#define script_suport "batocera-support";
#define script_gameforce "batocera-gameforce"; // buttonColorLed X, powerLed X
#define script_audio "batocera-audio"; // list, list-profiles, get-profile, set-profile 'X', get, set 'X'
#define script_bezelproject "batocera-es-thebezelproject"; // list, install X, remove X
#define script_format "batocera-format"; // listDisks, listFstypes
#define script_store "batocera-store"; // list, update, refresh, clean-all, install "X", remove "X"
#define script_preupdategamelists "batocera-preupdate-gamelists-hook";
#define script_timezones "batocera-timezone"; // get, detect, set "X"
#define script_padsinfos "batocera-padsinfo";
#define script_swissknife "batocera-es-swissknife"; // --emukill"
*/

ApiSystem::ApiSystem() { }

ApiSystem* ApiSystem::instance = nullptr;
ApiSystem::LED_TYPE ApiSystem::mSystemLedType = ApiSystem::LED_TYPE_NONE;

ApiSystem *ApiSystem::getInstance() 
{
	if (ApiSystem::instance == nullptr)
	{
#if WIN32
		ApiSystem::instance = new Win32ApiSystem();
#else
		ApiSystem::instance = new ApiSystem();
#endif
		
		IExternalActivity::Instance = ApiSystem::instance;
	}

	return ApiSystem::instance;
}

unsigned long ApiSystem::getFreeSpaceGB(std::string mountpoint) 
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

	int free = 0;

#if !WIN32
	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) >= 0)
		free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
#endif

	return free;
}

std::string ApiSystem::getFreeSpaceUserInfo()
{
#ifdef _ENABLEEMUELEC
	return getFreeSpaceInfo("/storage/roms");
#else
	return getFreeSpaceInfo(Paths::getRootPath());
#endif
}

std::string ApiSystem::getFreeSpaceSystemInfo()
{
#ifdef _ENABLEEMUELEC
  return getFreeSpaceInfo("/emuelec");
#else
	return getFreeSpaceInfo("/boot");
#endif
}

std::string ApiSystem::getFreeSpaceInfo(const std::string mountpoint)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	std::ostringstream oss;

#if !WIN32
	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) < 0)
		return "";
		
	unsigned long long total = (unsigned long long) fiData.f_blocks * (unsigned long long) (fiData.f_bsize);
	unsigned long long free = (unsigned long long) fiData.f_bfree * (unsigned long long) (fiData.f_bsize);
	unsigned long long used = total - free;
	unsigned long percent = 0;
	
	if (total != 0) 
	{  //for small SD card ;) with share < 1GB
		percent = used * 100 / total;
		oss << Utils::FileSystem::megaBytesToString(used / (1024L * 1024L)) << "/" << Utils::FileSystem::megaBytesToString(total / (1024L * 1024L)) << " (" << percent << "%)";
	}
	else
		oss << "N/A";	
#endif

	return oss.str();
}

bool ApiSystem::isFreeSpaceLimit() 
{
#ifdef _ENABLEEMUELEC
	return getFreeSpaceGB("/storage/.update") < 2;
#else
	return getFreeSpaceGB(Paths::getRootPath()) < 2;
#endif
}

std::string ApiSystem::getVersion(bool extra)
{
	LOG(LogDebug) << "ApiSystem::getVersion";
#ifdef _ENABLEEMUELEC
	std::ifstream ifs("/usr/config/EE_VERSION");
#else
	std::ifstream ifs("/usr/share/batocera/batocera.version");
#endif

	if (isScriptingSupported(VERSIONINFO)) 
	{
		std::string command = "batocera-version";
		if (extra) 
			command += " --extra";

		auto res = executeEnumerationScript(command);
		if (res.size() > 0 && !res[0].empty())
			return res[0];
	}

	if (extra)
		return "none";

	std::string localVersionFile = Paths::getVersionInfoPath();
	if (!Utils::FileSystem::exists(localVersionFile))
		localVersionFile = Paths::findEmulationStationFile("version.info");

	if (Utils::FileSystem::exists(localVersionFile))
	{
		std::string localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");
		return localVersion;
	}

	return PROGRAM_VERSION_STRING;	
}

std::string ApiSystem::getApplicationName()
{
	std::string localVersionFile = Paths::findEmulationStationFile("about.info");
	if (Utils::FileSystem::exists(localVersionFile))
	{
		std::string aboutInfo = Utils::FileSystem::readAllText(localVersionFile);
		aboutInfo = Utils::String::replace(Utils::String::replace(aboutInfo, "\r", ""), "\n", "");

		auto ver = ApiSystem::getInstance()->getVersion();
		auto cut = aboutInfo.find(" V" + ver);

		if (cut == std::string::npos)
			cut = aboutInfo.find(" " + ver);

		if (cut == std::string::npos)
			cut = aboutInfo.find(ver);

		if (cut != std::string::npos)
			aboutInfo = aboutInfo.substr(0, cut);

		return aboutInfo;
	}

#if BATOCERA
	return "BATOCERA";
#else
	return "EMULATIONSTATION";
#endif
}

bool ApiSystem::setOverscan(bool enable) 
{
	return executeScript("batocera-config overscan " + std::string(enable ? "enable" : "disable"));
}

bool ApiSystem::setOverclock(std::string mode) 
{
#ifdef _ENABLEEMUELEC
	return true;
#endif
	if (mode.empty())
		return false;

	return executeScript("batocera-overclock set " + mode);
}

// BusyComponent* ui
std::pair<std::string, int> ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
	LOG(LogDebug) << "ApiSystem::updateSystem";

#ifdef _ENABLEEMUELEC	
	std::string updatecommand = "emuelec-upgrade";
#else
	std::string updatecommand = "batocera-upgrade";
#endif

	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call update command"), -1);
	
	char line[1024] = "";
#ifdef _ENABLEEMUELEC
	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "emuelec-upgrade.log").c_str(), "w");
#else
	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-upgrade.log").c_str(), "w");
#endif
	while (fgets(line, 1024, pipe)) 
	{
	
		strtok(line, "\n");
		if (flog != nullptr) 
			fprintf(flog, "%s\n", line);

		if (func != nullptr)
			func(std::string(line));		
	}

	int exitCode = WEXITSTATUS(pclose(pipe));

	if (flog != NULL)
	{
		fprintf(flog, "Exit code : %d\n", exitCode);
		fclose(flog);
	}

	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::backupSystem(BusyComponent* ui, std::string device) 
{
	LOG(LogDebug) << "ApiSystem::backupSystem";

	std::string updatecommand = "batocera-sync sync " + device;
	FILE* pipe = popen(updatecommand.c_str(), "r");
	if (pipe == NULL)
		return std::pair<std::string, int>(std::string("Cannot call sync command"), -1);

	char line[1024] = "";

	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-sync.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");

		if (flog != NULL) 
			fprintf(flog, "%s\n", line);

		ui->setText(std::string(line));
	}

	if (flog != NULL) 
		fclose(flog);

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::installSystem(BusyComponent* ui, std::string device, std::string architecture) 
{
	LOG(LogDebug) << "ApiSystem::installSystem";

	std::string updatecommand = "batocera-install install " + device + " " + architecture;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == NULL)
		return std::pair<std::string, int>(std::string("Cannot call install command"), -1);

	char line[1024] = "";

	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-install.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		if (flog != NULL) fprintf(flog, "%s\n", line);
		ui->setText(std::string(line));
	}

	int exitCode = WEXITSTATUS(pclose(pipe));

	if (flog != NULL)
	{
		fprintf(flog, "Exit code : %d\n", exitCode);
		fclose(flog);
	}

	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::scrape(BusyComponent* ui) 
{
	LOG(LogDebug) << "ApiSystem::scrape";

	FILE* pipe = popen("batocera-scraper", "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call scrape command"), -1);

	char line[1024] = "";

#ifdef _ENABLEEMUELEC
	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "emuelec-scraper.log").c_str(), "w");
#else	
	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-scraper.log").c_str(), "w");
#endif
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");

		if (flog != NULL) 
			fprintf(flog, "%s\n", line);

		if (ui != nullptr && Utils::String::startsWith(line, "GAME: "))
			ui->setText(std::string(line));	
	}

	if (flog != nullptr)
		fclose(flog);

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(std::string(line), exitCode);
}

bool ApiSystem::ping() 
{
    // Google DNS
    if (!executeScript("ping -c 1 -W 2 -t 255 8.8.8.8"))
    {
        // Cloudflare DNS
        if (!executeScript("ping -c 1 -W 2 -t 255 1.1.1.1"))
        {
            // Quad9 DNS
            return executeScript("ping -c 1 -W 2 -t 255 9.9.9.9");
        }
    }

    return true;
}

bool ApiSystem::canUpdate(std::vector<std::string>& output) 
{
	LOG(LogDebug) << "ApiSystem::canUpdate";

	FILE *pipe = popen("batocera-config canupdate", "r");
	if (pipe == NULL)
		return false;

	char line[1024];
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		output.push_back(std::string(line));
	}

	int res = WEXITSTATUS(pclose(pipe));
	if (res == 0) 
	{
		LOG(LogInfo) << "Can update ";
		return true;
	}

	LOG(LogInfo) << "Cannot update ";
	return false;
}

void ApiSystem::launchExternalWindow_before(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchExternalWindow_before";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();
#ifdef _ENABLEEMUELEC	
	window->deinit(false);
#else
	window->deinit();
#endif

	LOG(LogDebug) << "ApiSystem::launchExternalWindow_before OK";
}

void ApiSystem::launchExternalWindow_after(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchExternalWindow_after";

#ifdef _ENABLEEMUELEC
	window->init(false);
#else
	window->init();
#endif
	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->init();
	window->normalizeNextUpdate();
	window->reactivateGui();

	AudioManager::getInstance()->playRandomMusic();

	LOG(LogDebug) << "ApiSystem::launchExternalWindow_after OK";
}

bool ApiSystem::launchKodi(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchKodi";

	std::string commandline = InputManager::getInstance()->configureEmulators();
	std::string command = "batocera-kodi " + commandline;

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());

	// WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
	// https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
	if (WIFEXITED(exitCode))
		exitCode = WEXITSTATUS(exitCode);

	ApiSystem::launchExternalWindow_after(window);

	// handle end of kodi
	switch (exitCode) 
	{
	case 10: // reboot code
		Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
		return true;
		
	case 11: // shutdown code
		Utils::Platform::quitES(Utils::Platform::QuitMode::SHUTDOWN);
		return true;
	}

	return exitCode == 0;
}

bool ApiSystem::launchFileManager(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchFileManager";

#ifdef _ENABLEEMUELEC
	std::string command = "/usr/bin/emuelec-utils filemanager";
#else
	std::string command = "filemanagerlauncher";
#endif

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());
	if (WIFEXITED(exitCode))
		exitCode = WEXITSTATUS(exitCode);

	ApiSystem::launchExternalWindow_after(window);

	return exitCode == 0;
}

bool ApiSystem::enableWifi(std::string ssid, std::string key) 
{
#ifdef _ENABLEEMUELEC
	return executeScript("batocera-config wifi enable \"" + ssid + "\" \"" + key + "\"");
#else
	return executeScript("batocera-wifi enable \"" + ssid + "\" \"" + key + "\"");
#endif
}

bool ApiSystem::disableWifi() 
{
#ifdef _ENABLEEMUELEC
	return executeScript("batocera-config wifi disable");
#else
	return executeScript("batocera-wifi disable");
#endif
}

std::string ApiSystem::getIpAddress()
{
	LOG(LogDebug) << "ApiSystem::getIpAddress";
	
	std::string result = Utils::Platform::queryIPAddress(); // platform.h
	if (result.empty())
		return "NOT CONNECTED";

	return result;
}

bool ApiSystem::enableBluetooth()
{
	return executeScript("batocera-bluetooth enable 2>&1 >/dev/null");
}

bool ApiSystem::disableBluetooth()
{
	return executeScript("batocera-bluetooth disable");
}

void ApiSystem::startBluetoothLiveDevices(const std::function<void(const std::string)>& func)
{
	executeScript("batocera-bluetooth live_devices", func);
}

void ApiSystem::stopBluetoothLiveDevices()
{
	executeScript("batocera-bluetooth stop_live_devices");
}

bool ApiSystem::pairBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth trust " + deviceName);
}

bool ApiSystem::connectBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth connect " + deviceName);
}

bool ApiSystem::disconnectBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth disconnect " + deviceName);
}

bool ApiSystem::removeBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth remove " + deviceName);
}

bool ApiSystem::scanNewBluetooth(const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-bluetooth trust input", func).second == 0;
}

std::vector<std::string> ApiSystem::getPairedBluetoothDeviceList()
{
	return executeEnumerationScript("batocera-bluetooth list");
}

std::vector<std::string> ApiSystem::getAvailableStorageDevices() 
{
	return executeEnumerationScript("batocera-config storage list");
}

std::vector<std::string> ApiSystem::getVideoModes(const std::string output)
{
  if(output == "") {
    return executeEnumerationScript("batocera-resolution listModes");
  } else {
    return executeEnumerationScript("batocera-resolution --screen \"" + output + "\" listModes");
  }
}

std::vector<std::string> ApiSystem::getCustomRunners() 
{
	return executeEnumerationScript("batocera-wine-runners");
}

std::vector<std::string> ApiSystem::getAvailableBackupDevices() 
{
	return executeEnumerationScript("batocera-sync list");
}

std::vector<std::string> ApiSystem::getAvailableInstallDevices() 
{
	return executeEnumerationScript("batocera-install listDisks");
}

std::vector<std::string> ApiSystem::getAvailableInstallArchitectures() 
{
	return executeEnumerationScript("batocera-install listArchs");
}

std::vector<std::string> ApiSystem::getAvailableOverclocking() 
{
#ifdef _ENABLEEMUELEC
	return executeEnumerationScript("echo no");
#else
	return executeEnumerationScript("batocera-overclock list");
#endif
}

std::vector<std::string> ApiSystem::getSystemInformations() 
{
	return executeEnumerationScript("batocera-info --full");
}

std::vector<BiosSystem> ApiSystem::getBiosInformations(const std::string system) 
{
	std::vector<BiosSystem> res;
	BiosSystem current;
	bool isCurrent = false;

	std::string cmd = "batocera-systems";
	if (!system.empty())
		cmd += " --filter " + system;

	auto systems = executeEnumerationScript(cmd);
	for (auto line : systems)
	{
		if (Utils::String::startsWith(line, "> ")) 
		{
			if (isCurrent)
				res.push_back(current);

			isCurrent = true;
			current.name = std::string(std::string(line).substr(2));
			current.bios.clear();
		}
		else 
		{
			BiosFile biosFile;
			std::vector<std::string> tokens = Utils::String::split(line, ' ');
			if (tokens.size() >= 3) 
			{
				biosFile.status = tokens.at(0);
				biosFile.md5 = tokens.at(1);

				// concatenat the ending words
				std::string vname = "";
				for (unsigned int i = 2; i < tokens.size(); i++) 
				{
					if (i > 2) vname += " ";
					vname += tokens.at(i);
				}
				biosFile.path = vname;

				current.bios.push_back(biosFile);
			}
		}
	}

	if (isCurrent)
		res.push_back(current);

	return res;
}

bool ApiSystem::generateSupportFile() 
{
	return executeScript("batocera-support");
}

std::string ApiSystem::getCurrentStorage() 
{
	LOG(LogDebug) << "ApiSystem::getCurrentStorage";

#if WIN32
	return "DEFAULT";
#endif

	std::ostringstream oss;
	oss << "batocera-config storage current";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return "INTERNAL";
}

bool ApiSystem::setStorage(std::string selected) 
{
	return executeScript("batocera-config storage " + selected);
}

bool ApiSystem::setButtonColorGameForce(std::string selected)
{
	return executeScript("batocera-gameforce buttonColorLed " + selected);
}

bool ApiSystem::setPowerLedGameForce(std::string selected)
{
	return executeScript("batocera-gameforce powerLed " + selected);
}

bool ApiSystem::forgetBluetoothControllers() 
{
	return executeScript("batocera-config forgetBT");
}

std::string ApiSystem::getRootPassword() 
{
	LOG(LogDebug) << "ApiSystem::getRootPassword";

	std::ostringstream oss;
	oss << "batocera-config getRootPassword";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL) {
		return "";
	}

	if (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return oss.str().c_str();
}

std::vector<std::string> ApiSystem::getAvailableVideoOutputDevices() 
{
	return executeEnumerationScript("batocera-config lsoutputs");
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputDevices() 
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-audio list");
}

std::string ApiSystem::getCurrentAudioOutputDevice() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputDevice";

	std::ostringstream oss;
	oss << "batocera-audio get";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}

	return "";
}

bool ApiSystem::setAudioOutputDevice(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setAudioOutputDevice";

	std::ostringstream oss;

	oss << "batocera-audio set" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());

	Sound::get(":/checksound.ogg")->play();

	return exitcode == 0;
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputProfiles()
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-audio list-profiles");
}

std::string ApiSystem::getCurrentAudioOutputProfile() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputProfile";

	std::ostringstream oss;
	oss << "batocera-audio get-profile";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}

	return "";
}

bool ApiSystem::setAudioOutputProfile(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setAudioOutputProfile";

	std::ostringstream oss;

	oss << "batocera-audio set-profile" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());
	
	Sound::get(":/checksound.ogg")->play();

	return exitcode == 0;
}

std::string ApiSystem::getUpdateUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.updates.url");
	if (!systemsetting.empty())
		return systemsetting;

#ifdef _ENABLEEMUELEC
	return "https://raw.githubusercontent.com/EmuELEC/emuelec.github.io/master/";
#else
	return "https://updates.batocera.org";
#endif

}

std::string ApiSystem::getThemesUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.themes.url");
	if (!systemsetting.empty())
		return systemsetting;

	return getUpdateUrl() + "/themes.json";
}

std::string ApiSystem::getGitRepositoryDefaultBranch(const std::string& url)
{
	std::string ret = "master";

	std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
	if (statUrl != url)
	{
		HttpReq statreq(statUrl);
		if (statreq.wait())
		{
			const std::string default_branch = "\"default_branch\": ";

			std::string content = statreq.getContent();
			auto pos = content.find(default_branch);
			if (pos != std::string::npos)
			{
				auto end = content.find(",", pos);
				if (end != std::string::npos)
				{
					ret = Utils::String::replace(content.substr(pos + default_branch.length(), end - pos - default_branch.length()), "\"", "");
				}
			}
		}
	}

	return ret;
}

bool ApiSystem::downloadGitRepository(const std::string& url, const std::string& branch, const std::string& fileName, const std::string& label, const std::function<void(const std::string)>& func, int64_t defaultDownloadSize)
{
	if (func != nullptr)
		func("Downloading " + label);

	int64_t downloadSize = defaultDownloadSize;
	if (downloadSize == 0)
	{
		std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
		if (statUrl != url)
		{
			HttpReq statreq(statUrl);
			if (statreq.wait())
			{
				std::string content = statreq.getContent();
				auto pos = content.find("\"size\": ");
				if (pos != std::string::npos)
				{
					auto end = content.find(",", pos);
					if (end != std::string::npos)
						downloadSize = atoi(content.substr(pos + 8, end - pos - 8).c_str()) * 1024LL;
				}
			}
		}
	}

	HttpReq httpreq(url + "/archive/"+ branch +".zip", fileName);

	int curPos = -1;
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (downloadSize > 0)
		{
			int64_t pos = httpreq.getPosition();
			if (pos > 0 && curPos != pos)
			{
				if (func != nullptr)
				{
					std::string pc = std::to_string((int)(pos * 100LL / downloadSize));
					func(std::string("Downloading " + label + " >>> " + pc + " %"));
				}

				curPos = pos;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

bool ApiSystem::isThemeInstalled(const std::string& themeName, const std::string& url)
{
	std::string themeUrl = Utils::FileSystem::getFileName(url);

	std::vector<std::string> paths =
	{
		Paths::getUserThemesPath(),
		Paths::getThemesPath(),
		Paths::getUserEmulationStationPath() + "/themes"
#if !WIN32
		,"/etc/emulationstation/themes" // Backward compatibility with Retropie
#endif
	};

	for (auto path : VectorHelper::distinct(paths, [](auto x) { return x; }))
	{
		if (path.empty())
			continue;

		if (Utils::FileSystem::isDirectory(path + "/" + themeUrl))
			return true;

		if (Utils::FileSystem::isDirectory(path + "/" + themeUrl + "-master"))
			return true;

		if (Utils::FileSystem::isDirectory(path + "/" + themeName))
			return true;
	}

	return false;
}

extern std::string jsonString(const rapidjson::Value& val, const std::string& name);
extern int jsonInt(const rapidjson::Value& val, const std::string& name);

std::vector<BatoceraTheme> ApiSystem::getBatoceraThemesList()
{
	LOG(LogDebug) << "ApiSystem::getBatoceraThemesList";

	std::vector<BatoceraTheme> res;

	auto url = getThemesUrl();

	HttpReq httpreq(url);
	if (httpreq.wait())
	{
		rapidjson::Document doc;
		doc.Parse(httpreq.getContent().c_str());
		if (doc.HasParseError())
			return res;

		if (!doc.HasMember("data"))
			return res;

		for (auto& item : doc["data"].GetArray())
		{
			BatoceraTheme bt;
			bt.name = jsonString(item, "theme");
			bt.url = jsonString(item, "theme_url");
			bt.author = jsonString(item, "author");
			bt.lastUpdate = jsonString(item, "last_update");
			bt.upToDate = jsonInt(item, "up_to_date");
			bt.size = jsonInt(item, "size");
			bt.isInstalled = isThemeInstalled(bt.name, bt.url);

			auto screenShot = jsonString(item, "screenshot");
			if (!screenShot.empty())
				bt.image = Utils::FileSystem::getParent(url) + "/" + screenShot;

			res.push_back(bt);
		}
	}

	return res;	
}

std::pair<std::string, int> ApiSystem::installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (theme.name != thname)
			continue;

		std::string installFolder = Paths::getUserThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getUserEmulationStationPath() + "/themes";

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);
		std::string extractionDirectory = installFolder + "/.tmp";
		std::string zipFile = extractionDirectory +"/" + themeFileName + ".zip";

		Utils::FileSystem::createDirectory(extractionDirectory);
		Utils::FileSystem::removeFile(zipFile);
		
		std::string branch = getGitRepositoryDefaultBranch(theme.url);

		if (downloadGitRepository(theme.url, branch, zipFile, thname, func, theme.size * 1024LL * 1024))
		{
			if (func != nullptr)
				func(_("Extracting") + " " + thname);

			unzipFile(zipFile, extractionDirectory);
			
			std::string folderName = extractionDirectory + "/" + themeFileName + "-" + branch;
			if (!Utils::FileSystem::exists(folderName))
				folderName = extractionDirectory + "/" + themeFileName;

			if (Utils::FileSystem::exists(folderName))
			{
				std::string finalfolderName = installFolder  + "/" + themeFileName;
				if (Utils::FileSystem::exists(finalfolderName))
					Utils::FileSystem::deleteDirectoryFiles(finalfolderName, true);

				Utils::FileSystem::renameFile(folderName, finalfolderName);
			}

			Utils::FileSystem::removeFile(zipFile);
			Utils::FileSystem::deleteDirectoryFiles(extractionDirectory, true);

			return std::pair<std::string, int>(std::string("OK"), 0);
		}

		Utils::FileSystem::deleteDirectoryFiles(extractionDirectory, true);
		return std::pair<std::string, int>(std::string(""), 1);
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (!theme.isInstalled || theme.name != thname)
			continue;

		std::string installFolder = Paths::getUserThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getUserEmulationStationPath() + "/themes";

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);

		std::string folderName = installFolder + "/" + themeFileName;
		if (!Utils::FileSystem::exists(folderName))
			folderName = folderName + "-master";

		if (Utils::FileSystem::exists(folderName))
		{
			Utils::FileSystem::deleteDirectoryFiles(folderName, true);
			return std::pair<std::string, int>("OK", 0);
		}

		break;
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::vector<BatoceraBezel> ApiSystem::getBatoceraBezelsList()
{
	LOG(LogInfo) << "ApiSystem::getBatoceraBezelsList";

	std::vector<BatoceraBezel> res;

	auto lines = executeEnumerationScript("batocera-es-thebezelproject list");
	for (auto line : lines)
	{
		auto parts = Utils::String::splitAny(line, " \t");
		if (parts.size() < 2)
			continue;

		if (!Utils::String::startsWith(parts[0], "[I]") && !Utils::String::startsWith(parts[0], "[A]"))
			continue;

		BatoceraBezel bz;
		bz.isInstalled = (Utils::String::startsWith(parts[0], "[I]"));
		bz.name = parts[1];
		bz.url = parts.size() < 3 ? "" : (parts[2] == "-" ? parts[3] : parts[2]);
		bz.folderPath = parts.size() < 4 ? "" : parts[3];

		if (bz.name != "?")
			res.push_back(bz);
	}

	return res;
}

std::pair<std::string, int> ApiSystem::installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-es-thebezelproject install " + bezelsystem, func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-es-thebezelproject remove " + bezelsystem, func);
}

std::string ApiSystem::getMD5(const std::string fileName, bool fromZipContents)
{
	LOG(LogDebug) << "getMD5 >> " << fileName;

	// 7za x -so test.7z | md5sum
	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));
	if (ext == ".zip" && fromZipContents)
	{
		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			std::string romName;

			for (auto name : file.namelist())
			{
				if (Utils::FileSystem::getExtension(name) != ".txt" && !Utils::String::endsWith(name, "/"))
				{
					if (!romName.empty())
					{
						romName = "";
						break;
					}

					romName = name;
				}
			}

			if (!romName.empty())
				return file.getFileMd5(romName);
		}
	}

#if !WIN32
	if (fromZipContents && ext == ".7z")
	{
		auto cmd = getSevenZipCommand() + " x -so \"" + fileName + "\" | md5sum";
		auto ret = executeEnumerationScript(cmd);
		if (ret.size() == 1 && ret.cbegin()->length() >= 32)
			return ret.cbegin()->substr(0, 32);
	}
#endif

	std::string contentFile = fileName;
	std::string ret;
	std::string tmpZipDirectory;

	if (fromZipContents && ext == ".7z")
	{
		tmpZipDirectory = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), Utils::FileSystem::getStem(fileName));
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory);

		if (unzipFile(fileName, tmpZipDirectory))
		{
			auto fileList = Utils::FileSystem::getDirContent(tmpZipDirectory, true);

			std::vector<std::string> res;
			std::copy_if(fileList.cbegin(), fileList.cend(), std::back_inserter(res), [](const std::string file) { return Utils::FileSystem::getExtension(file) != ".txt";  });
		
			if (res.size() == 1)
				contentFile = *res.cbegin();
		}

		// if there's no file or many files ? get md5 of archive
	}

	ret = Utils::FileSystem::getFileMd5(contentFile);

	if (!tmpZipDirectory.empty())
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory, true);

	LOG(LogDebug) << "getMD5 << " << ret;

	return ret;
}

std::string ApiSystem::getCRC32(std::string fileName, bool fromZipContents)
{
	LOG(LogDebug) << "getCRC32 >> " << fileName;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));

	if (ext == ".7z" && fromZipContents)
	{
		LOG(LogDebug) << "getCRC32 is using 7z";

		std::string fn = Utils::FileSystem::getFileName(fileName);
		auto cmd = getSevenZipCommand() + " l -slt \"" + fileName + "\"";
		auto lines = executeEnumerationScript(cmd);
		for (std::string all : lines)
		{
			int idx = all.find("CRC = ");
			if (idx != std::string::npos)
				return all.substr(idx + 6);
			else if (all.find(fn) == (all.size() - fn.size()) && all.length() > 8 && all[9] == ' ')
				return all.substr(0, 8);
		}
	}
	else if (ext == ".zip" && fromZipContents)
	{
		LOG(LogDebug) << "getCRC32 is using ZipFile";

		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			std::string romName;

			for (auto name : file.namelist())
			{
				if (Utils::FileSystem::getExtension(name) != ".txt" && !Utils::String::endsWith(name, "/"))
				{
					if (!romName.empty())
					{
						romName = "";
						break;
					}

					romName = name;
				}
			}

			if (!romName.empty())
				return file.getFileCrc(romName);
		}
	}

	LOG(LogDebug) << "getCRC32 is using fileBuffer";
	return Utils::FileSystem::getFileCrc32(fileName);
}

bool ApiSystem::unzipFile(const std::string fileName, const std::string destFolder, const std::function<bool(const std::string)>& shouldExtract)
{
	LOG(LogDebug) << "unzipFile >> " << fileName << " to " << destFolder;

	if (!Utils::FileSystem::exists(destFolder))
		Utils::FileSystem::createDirectory(destFolder);
		
	if (Utils::String::toLower(Utils::FileSystem::getExtension(fileName)) == ".zip")
	{
		LOG(LogDebug) << "unzipFile is using ZipFile";

		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			for (auto name : file.namelist())
			{
				if (Utils::String::endsWith(name, "/"))
				{
					Utils::FileSystem::createDirectory(Utils::FileSystem::combine(destFolder, name.substr(0, name.length() - 1)));
					continue;
				}

				if (shouldExtract != nullptr && !shouldExtract(Utils::FileSystem::combine(destFolder, name)))
					continue;

				file.extract(name, destFolder);
			}

			LOG(LogDebug) << "unzipFile << OK";
			return true;
		}

		LOG(LogDebug) << "unzipFile << KO Bad format ?" << fileName;
		return false;
	}
	
	LOG(LogDebug) << "unzipFile is using 7z";

	std::string cmd = getSevenZipCommand() + " x \"" + Utils::FileSystem::getPreferredPath(fileName) + "\" -y -o\"" + Utils::FileSystem::getPreferredPath(destFolder) + "\"";
	bool ret = executeScript(cmd);
	LOG(LogDebug) << "unzipFile <<";
	return ret;
}

static std::string BACKLIGHT_BRIGHTNESS_NAME;
static std::string BACKLIGHT_BRIGHTNESS_MAX_NAME;

bool ApiSystem::getBrightness(int& value)
{	
	#if WIN32
	return false;
	#endif

	if (BACKLIGHT_BRIGHTNESS_NAME == "notfound")
		return false;

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_MAX_NAME.empty())
	{
		for (auto file : Utils::FileSystem::getDirContent("/sys/class/backlight"))
		{				
			std::string brightnessPath = file + "/brightness";
			std::string maxBrightnessPath = file + "/max_brightness";

			if (Utils::FileSystem::exists(brightnessPath) && Utils::FileSystem::exists(maxBrightnessPath))
			{
				BACKLIGHT_BRIGHTNESS_NAME = brightnessPath;
				BACKLIGHT_BRIGHTNESS_MAX_NAME = maxBrightnessPath;

				LOG(LogInfo) << "ApiSystem::getBrightness > brightness path resolved to " << file;
				break;
			}
		}
	}

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_MAX_NAME.empty())
	{
		LOG(LogInfo) << "ApiSystem::getBrightness > brightness path is not resolved";

		BACKLIGHT_BRIGHTNESS_NAME = "notfound";
		return false;
	}

	value = 0;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_MAX_NAME));
	if (max == 0)
		return false;

	if (Utils::FileSystem::exists(BACKLIGHT_BRIGHTNESS_NAME))
		value = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_NAME));

	value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
	return true;
}

void ApiSystem::setBrightness(int value)
{
#if WIN32	
	return;
#endif 

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_NAME == "notfound")
		return;

	if (value < 1)
		value = 1;

	if (value > 100)
		value = 100;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_MAX_NAME));
	if (max == 0)
		return;

	float percent = (value / 100.0f * (float)max) + 0.5f;
		
	std::string content = std::to_string((uint32_t) percent) + "\n";
	Utils::FileSystem::writeAllText(BACKLIGHT_BRIGHTNESS_NAME, content);
}

static std::string LED_COLOUR_NAME;
static std::string LED_BRIGHTNESS_VALUE;
static std::string LED_MAX_BRIGHTNESS_VALUE;

bool ApiSystem::getLED(int& red, int& green, int& blue)
{	
#if WIN32
	return false;
#endif

	if (mSystemLedType != LED_TYPE_NONE)
		return true;

	auto entries = Utils::FileSystem::getDirContent("/sys/class/leds");
	bool found_addressable = false;

	for (const auto& entry : entries)
	{
		if (entry.find("multicolor") != std::string::npos || entry.find(":rgb:joystick_rings") != std::string::npos)
		{
			std::string ledColourPath = entry + "/multi_intensity";				
			if (Utils::FileSystem::exists(ledColourPath))
			{
				LED_COLOUR_NAME = ledColourPath;
				mSystemLedType = LED_TYPE_UNIFIED;
				LOG(LogInfo) << "ApiSystem::getLED > Found UNIFIED LED at " << entry;
				break;
			}
		}
		if (entry.find("l:b1") != std::string::npos)
		{
			found_addressable = true;
		}
	}

	if (mSystemLedType == LED_TYPE_NONE && found_addressable) {
		mSystemLedType = LED_TYPE_ADDRESSABLE;
		LOG(LogInfo) << "ApiSystem::getLED > Found ADDRESSABLE LED type";
	}

	if (mSystemLedType == LED_TYPE_NONE) {
		LED_COLOUR_NAME = "notfound";
		return false;
	}

    if (mSystemLedType == LED_TYPE_UNIFIED && Utils::FileSystem::exists(LED_COLOUR_NAME)) {
        std::string colourValue = Utils::FileSystem::readAllText(LED_COLOUR_NAME);
        std::stringstream ss(colourValue);
        std::string token;
        // Extract red value
        std::getline(ss, token, ' ');
        red = std::stoi(token);

        // Extract green value
        std::getline(ss, token, ' ');
        green = std::stoi(token);

        // Extract blue value
        std::getline(ss, token);
        blue = std::stoi(token);

        executeScript("batocera-led-handheld block_color_changes"); // temporarily prevent changes from external daemon
        LOG(LogInfo) << "ApiSystem::getLED > LED colours are:" << red << " " << green << " " << blue;

        return true;
    }
	else if (mSystemLedType == LED_TYPE_ADDRESSABLE) {
		getLEDColours(red, green, blue);
        executeScript("batocera-led-handheld block_color_changes");
		return true;
	}

	return false;
}

void ApiSystem::getLEDColours(int& red, int& green, int& blue)
{
	std::string colourValue = SystemConf::getInstance()->get("led.colour");
	if (colourValue.empty())
		colourValue = "255 0 165";

    std::stringstream ss(colourValue);
    std::string token;

	// Extract red value
    std::getline(ss, token, ' ');
    red = std::stoi(token);

	// Extract green value
    std::getline(ss, token, ' ');
    green = std::stoi(token);

	// Extract blue value
    std::getline(ss, token);
    blue = std::stoi(token);

	LOG(LogInfo) << "ApiSystem::getLEDColours > LED colours are: " << red << " " << green << " " << blue;
}

void ApiSystem::setLEDColours(int red, int green, int blue)
{
#if WIN32    
    return;
#endif 

	if (mSystemLedType == LED_TYPE_NONE)
		return;

    // Ensure RGB values are within valid range
	if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;

	if (mSystemLedType == LED_TYPE_UNIFIED)
	{
		if (LED_COLOUR_NAME.empty() || LED_COLOUR_NAME == "notfound") return;
		std::string content = std::to_string(red) + " " + std::to_string(green) + " " + std::to_string(blue);
		Utils::FileSystem::writeAllText(LED_COLOUR_NAME, content);
	}
	else if (mSystemLedType == LED_TYPE_ADDRESSABLE)
	{
		static std::vector<std::string> r_files, g_files, b_files;
		if (r_files.empty()) {
			auto all_files = Utils::FileSystem::getDirContent("/sys/class/leds");
			for(const auto& file : all_files) {
				if (file.find(":r") != std::string::npos) r_files.push_back(file + "/brightness");
				if (file.find(":g") != std::string::npos) g_files.push_back(file + "/brightness");
				if (file.find(":b") != std::string::npos) b_files.push_back(file + "/brightness");
			}
		}

		for(const auto& file : r_files) Utils::FileSystem::writeAllText(file, std::to_string(red));
		for(const auto& file : g_files) Utils::FileSystem::writeAllText(file, std::to_string(green));
		for(const auto& file : b_files) Utils::FileSystem::writeAllText(file, std::to_string(blue));
	}
}

bool ApiSystem::getLEDBrightness(int& value)
{   
#if WIN32
    return false;
#endif
    if (mSystemLedType != LED_TYPE_UNIFIED)
        return false;

    if (LED_BRIGHTNESS_VALUE == "notfound")
        return false;

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        auto directories = Utils::FileSystem::getDirContent("/sys/class/leds");

        for (const auto& directory : directories)
        {
            if (directory.find("multicolor") != std::string::npos || directory.find(":rgb:joystick_rings") != std::string::npos)
            {
                std::string ledBrightnessPath = directory + "/brightness";
                std::string ledMaxBrightnessPath = directory + "/max_brightness";

                if (Utils::FileSystem::exists(ledBrightnessPath) && Utils::FileSystem::exists(ledMaxBrightnessPath))
                {
                    LED_BRIGHTNESS_VALUE = ledBrightnessPath;
                    LED_MAX_BRIGHTNESS_VALUE = ledMaxBrightnessPath;

                    LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path resolved to " << directory;
                    break;
                }
            }
        }
    }

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path is not resolved";

        LED_BRIGHTNESS_VALUE = "notfound";
        return false;
    }

    value = 0;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max == 0)
        return false;

    if (Utils::FileSystem::exists(LED_BRIGHTNESS_VALUE))
        value = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_BRIGHTNESS_VALUE));

    value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
    return true;
}

void ApiSystem::setLEDBrightness(int value) 
{
#if WIN32
    return;
#endif
    if (mSystemLedType != LED_TYPE_UNIFIED)
		return;

    if (LED_BRIGHTNESS_VALUE.empty() || LED_BRIGHTNESS_VALUE == "notfound")
        return;

    if (value < 0) value = 0;
	if (value > 100) value = 100;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max == 0)
        return;

    float percent = static_cast<float>(value) / 100.0f;
	int brightnessValue = static_cast<int>(percent * max + 0.5f);
    std::string content = std::to_string(brightnessValue) + "\n";
    Utils::FileSystem::writeAllText(LED_BRIGHTNESS_VALUE, content);
}

bool ApiSystem::isLEDEnabled()
{
#if WIN32
	return false;
#else
	// Check batocera.conf for "led.enabled" setting, default to "1" (true) if not found
	return SystemConf::getInstance()->get("led.enabled") != "0";
#endif
}

void ApiSystem::setLEDEnabled(bool enabled)
{
#if WIN32
    return;
#else
	SystemConf::getInstance()->set("led.enabled", enabled ? "1" : "0");

	if (!enabled)
	{
		setLEDColours(0, 0, 0);
	}
	else
	{
		std::string lastColorStr = SystemConf::getInstance()->get("led.colour");
		if (lastColorStr.empty())
			lastColorStr = "255 0 165";

		std::stringstream ss(lastColorStr);
		int r, g, b;
		ss >> r >> g >> b;

		setLEDColours(r, g, b);
	}

	SystemConf::getInstance()->saveSystemConf();
#endif
}

std::vector<std::string> ApiSystem::getWifiNetworks(bool scan)
{
	return executeEnumerationScript(scan ? "batocera-wifi scanlist" : "batocera-wifi list");
}

std::vector<std::string> ApiSystem::executeEnumerationScript(const std::string command)
{
	LOG(LogDebug) << "ApiSystem::executeEnumerationScript -> " << command;

	std::vector<std::string> res;

	FILE *pipe = popen(command.c_str(), "r");

	if (pipe == NULL)
		return res;

	char line[1024];
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}

	pclose(pipe);
	return res;
}

std::pair<std::string, int> ApiSystem::executeScript(const std::string command, const std::function<void(const std::string)>& func)
{
	LOG(LogInfo) << "ApiSystem::executeScript -> " << command;

	FILE *pipe = popen(command.c_str(), "r");
	if (pipe == NULL)
	{
		LOG(LogError) << "Error executing " << command;
		return std::pair<std::string, int>("Error starting command : " + command, -1);
	}

	char line[1024];
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");

		if (func != nullptr)
			func(std::string(line));
	}

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(line, exitCode);
}

bool ApiSystem::executeScript(const std::string command)
{	
	LOG(LogInfo) << "Running " << command;

	if (system(command.c_str()) == 0)
		return true;
	
	LOG(LogError) << "Error executing " << command;
	return false;
}

bool ApiSystem::isScriptingSupported(ScriptId script)
{
	std::vector<std::string> executables;

	switch (script)
	{
	case ApiSystem::THEMESDOWNLOADER:
		return true;
	case ApiSystem::RETROACHIVEMENTS:
#ifdef CHEEVOS_DEV_LOGIN
		return true;
#endif
		break;
	case ApiSystem::KODI:
		executables.push_back("kodi");
		break;
	case ApiSystem::WIFI:
		executables.push_back("batocera-wifi");
		break;
	case ApiSystem::BLUETOOTH:
		executables.push_back("batocera-bluetooth");
		break;
	case ApiSystem::RESOLUTION:
		executables.push_back("batocera-resolution");
		break;
	case ApiSystem::BIOSINFORMATION:
		executables.push_back("batocera-systems");
		break;
	case ApiSystem::DISKFORMAT:
		executables.push_back("batocera-format");
		break;
	case ApiSystem::OVERCLOCK:
		executables.push_back("batocera-overclock");
		break;
	case ApiSystem::NETPLAY:
		executables.push_back("7zr");
		break;
	case ApiSystem::PDFEXTRACTION:
		executables.push_back("pdftoppm");
		executables.push_back("pdfinfo");
		break;
	case ApiSystem::BATOCERASTORE:
		executables.push_back("batocera-store");
		break;
	case ApiSystem::THEBEZELPROJECT:
		executables.push_back("batocera-es-thebezelproject");
		break;		
	case ApiSystem::PADSINFO:
		executables.push_back("batocera-padsinfo");
		break;
	case ApiSystem::EVMAPY:
		executables.push_back("evmapy");
		break;
	case ApiSystem::BATOCERAPREGAMELISTSHOOK:
		executables.push_back("batocera-preupdate-gamelists-hook");
		break;
	case ApiSystem::TIMEZONES:
		executables.push_back("batocera-timezone");
		break;
	case ApiSystem::AUDIODEVICE:
		executables.push_back("batocera-audio");
		break;		
	case ApiSystem::BACKUP:
		executables.push_back("batocera-sync");
		break;
	case ApiSystem::INSTALL:
		executables.push_back("batocera-install");
		break;	
	case ApiSystem::SUPPORTFILE:
		executables.push_back("batocera-support");
		break;
	case ApiSystem::UPGRADE:
#ifdef _ENABLEEMUELEC
		executables.push_back("emuelec-upgrade");
#else
		executables.push_back("batocera-upgrade");
#endif
		break;
	case ApiSystem::SUSPEND:
		return (Utils::FileSystem::exists("/usr/sbin/pm-suspend") && Utils::FileSystem::exists("/usr/bin/pm-is-supported") && executeScript("/usr/bin/pm-is-supported --suspend"));
	case ApiSystem::VERSIONINFO:
		executables.push_back("batocera-version");
		break;
	case ApiSystem::READPLANEMODE:
	case ApiSystem::WRITEPLANEMODE:
		executables.push_back("batocera-planemode");
		break;
	case ApiSystem::SERVICES:
		executables.push_back("batocera-services");
		break;
	case ApiSystem::BACKGLASS:
		executables.push_back("batocera-backglass");
		break;
	}

	if (executables.size() == 0)
		return true;

	for (auto executable : executables)
#ifdef _ENABLEEMUELEC
		if (!Utils::FileSystem::exists("/usr/bin/batocera/" + executable))
			return false;
#else
		if (!Utils::FileSystem::exists("/usr/bin/" + executable))
			return false;

#endif
	return true;
}

bool ApiSystem::downloadFile(const std::string url, const std::string fileName, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func("Downloading " + label);

	HttpReq httpreq(url, fileName);
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (func != nullptr)
			func(std::string("Downloading " + label + " >>> " + std::to_string(httpreq.getPercent()) + " %"));

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

void ApiSystem::setReadyFlag(bool ready)
{
	if (!ready)
	{
		Utils::FileSystem::removeFile("/tmp/emulationstation.ready");
		return;
	}

	FILE* fd = fopen("/tmp/emulationstation.ready", "w");
	if (fd != NULL) 
		fclose(fd);
}

bool ApiSystem::isReadyFlagSet()
{
	return Utils::FileSystem::exists("/tmp/emulationstation.ready");
}

std::vector<std::string> ApiSystem::getFormatDiskList()
{
#if WIN32 && _DEBUG
	std::vector<std::string> ret;
	ret.push_back("d:\\ DRIVE D:");
	ret.push_back("e:\\ DRIVE Z:");
	return ret;
#endif
	return executeEnumerationScript("batocera-format listDisks");
}

std::vector<std::string> ApiSystem::getFormatFileSystems()
{
#if WIN32 && _DEBUG
	std::vector<std::string> ret;
	ret.push_back("exfat");	
	ret.push_back("brfs");
	return ret;
#endif
	return executeEnumerationScript("batocera-format listFstypes");
}

int ApiSystem::formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-format format " + disk + " " + format, func).second;
}

int ApiSystem::getPdfPageCount(const std::string& fileName)
{
	auto lines = executeEnumerationScript("pdfinfo \"" + fileName + "\"");
	for (auto line : lines)
	{
		auto splits = Utils::String::split(line, ':', true);
		if (splits.size() == 2 && splits[0] == "Pages")
			return atoi(Utils::String::trim(splits[1]).c_str());
	}

	return 0;
}

std::vector<std::string> ApiSystem::extractPdfImages(const std::string& fileName, int pageIndex, int pageCount, int quality)
{
	auto pdfFolder = Utils::FileSystem::getPdfTempPath();

	std::vector<std::string> ret;

	if (pageIndex < 0)
	{
		Utils::FileSystem::deleteDirectoryFiles(pdfFolder);

		int hardWareCoreCount = std::thread::hardware_concurrency();
		if (hardWareCoreCount > 1)
		{
			int lastTime = SDL_GetTicks();

			int numberOfPagesToProcess = 1;
			if (hardWareCoreCount < 8)
				numberOfPagesToProcess = 2;

			int pc = getPdfPageCount(fileName);
			if (pc > 0)
			{
				Utils::ThreadPool pool(1);

				for (int i = 0; i < pc; i += numberOfPagesToProcess)
					pool.queueWorkItem([this, fileName, i, numberOfPagesToProcess] { extractPdfImages(fileName, i + 1, numberOfPagesToProcess); });

				pool.wait();

				int time = SDL_GetTicks() - lastTime;
				std::string timeText = std::to_string(time) + "ms";

				for (auto file : Utils::FileSystem::getDirContent(pdfFolder, false))
				{
					auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
					if (ext != ".jpg" && ext != ".png" && ext != ".ppm")
						continue;

					ret.push_back(file);
				}

				std::sort(ret.begin(), ret.end());
			}

			return ret;
		}
	}

	int lastTime = SDL_GetTicks();

	std::string page;

	std::string squality = Renderer::isSmallScreen() ? "96" : "125";
	if (quality > 0)
		squality = std::to_string(quality); // "300";

	std::string prefix = "extract";
	if (pageIndex >= 0)
	{
		char buffer[12];
		sprintf(buffer, "%08d", (uint32_t)pageIndex);
		
		if (pageIndex < 0)
			prefix = "page-" + squality + "-" + std::string(buffer) + "-pdf"; // page
		else
			prefix = Utils::FileSystem::getFileName(fileName) + "-" + squality + "-" + std::string(buffer) + "-pdf"; // page

		page = " -f " + std::to_string(pageIndex) + " -l " + std::to_string(pageIndex + pageCount - 1);
	}

#if WIN32
	executeEnumerationScript("pdftoppm -r "+ squality + page +" \"" + fileName + "\" \""+ pdfFolder +"/" + prefix +"\"");
#else
	executeEnumerationScript("pdftoppm -jpeg -r "+ squality +" -cropbox" + page + " \"" + fileName + "\" \"" + pdfFolder + "/" + prefix + "\"");
#endif

	int time = SDL_GetTicks() - lastTime;
	std::string text = std::to_string(time);
	
	for (auto file : Utils::FileSystem::getDirContent(pdfFolder, false))
	{
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
		if (ext != ".jpg" && ext != ".png" && ext != ".ppm")
			continue;

		if (pageIndex >= 0 && !Utils::String::startsWith(Utils::FileSystem::getFileName(file), prefix))
			continue;

		ret.push_back(file);
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}


std::vector<PacmanPackage> ApiSystem::getBatoceraStorePackages()
{
	std::vector<PacmanPackage> packages;

	LOG(LogDebug) << "ApiSystem::getBatoceraStorePackages";

	auto res = executeEnumerationScript("batocera-store list");
	std::string data = Utils::String::join(res, "\n");
	if (data.empty())
	{
		LOG(LogError) << "Package list is empty";
		return packages;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		return packages;
	}

	pugi::xml_node root = doc.child("packages");
	if (!root)
	{
		LOG(LogError) << "Could not find <packages> node";
		return packages;
	}

	for (pugi::xml_node pkgNode = root.child("package"); pkgNode; pkgNode = pkgNode.next_sibling("package"))
	{
		PacmanPackage package;

		for (pugi::xml_node node = pkgNode.first_child(); node; node = node.next_sibling())
		{
			std::string tag = node.name();
			if (tag == "name")
				package.name = node.text().get();
			if (tag == "repository")
				package.repository = node.text().get();
			if (tag == "available_version")
				package.available_version = node.text().get();
			if (tag == "description")
				package.description = node.text().get();
			if (tag == "group")
				package.group = node.text().get(); // groups.push_back(
			if (tag == "license")
				package.licenses.push_back(node.text().get());
			if (tag == "packager")
				package.packager = node.text().get();
			if (tag == "status")
				package.status = node.text().get();
			if (tag == "repository")
				package.repository = node.text().get();
			if (tag == "url")
				package.url = node.text().get();			
			if (tag == "arch")
				package.arch = node.text().get();
			if (tag == "download_size")
				package.download_size = node.text().as_llong();
			if (tag == "installed_size")
				package.installed_size = node.text().as_llong();
			if (tag == "preview_url")
				package.preview_url = node.text().get();
		}

		if (!package.name.empty())
			packages.push_back(package);		
	}

	return packages;
}

std::pair<std::string, int> ApiSystem::installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store install \"" + name + "\"", func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store remove \"" + name + "\"", func);
}

void ApiSystem::refreshBatoceraStorePackageList()
{
	executeScript("batocera-store refresh");
	executeScript("batocera-store clean-all");
}

void ApiSystem::callBatoceraPreGameListsHook()
{
	executeScript("batocera-preupdate-gamelists-hook");
}

void ApiSystem::updateBatoceraStorePackageList()
{
	executeScript("batocera-store update");
}

std::vector<std::string> ApiSystem::getShaderList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	for (auto folder : { Paths::getUserShadersPath(), Paths::getShadersPath() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, true))
		{
			if (Utils::FileSystem::getFileName(file) == "rendering-defaults.yml")
			{
				auto parent = Utils::FileSystem::getFileName(Utils::FileSystem::getParent(file));
				if (parent == "configs")
					continue;

				if (std::find(ret.cbegin(), ret.cend(), parent) == ret.cend())
					ret.push_back(parent);
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getVideoFilterList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getVideoFilterList";

	for (auto folder : { Paths::getUserVideoFilters(), Paths::getVideoFilters() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, false))
		{
			auto videofilter = Utils::FileSystem::getFileName(file);
			if (videofilter.substr(videofilter.find_last_of('.') + 1) == "filt")
			{
				if (std::find(ret.cbegin(), ret.cend(), videofilter) == ret.cend())
					ret.push_back(videofilter.substr(0, videofilter.find_last_of('.')));
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getRetroachievementsSoundsList()
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getRetroAchievementsSoundsList";

	for (auto folder : { Paths::getUserRetroachivementSounds(), Paths::getRetroachivementSounds() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, false))
		{
			auto sound = Utils::FileSystem::getFileName(file);
			if (sound.substr(sound.find_last_of('.') + 1) == "ogg")
			{
				if (std::find(ret.cbegin(), ret.cend(), sound) == ret.cend())
				  ret.push_back(sound.substr(0, sound.find_last_of('.')));
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getTimezones()
{
	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getTimezones";

	auto folder = Paths::getTimeZonesPath();
	if (Utils::FileSystem::isDirectory(folder))
	{
		for (auto continent : Utils::FileSystem::getDirContent(folder, false))
		{
			std::string short_continent = continent.substr(continent.find_last_of('/') + 1);
			if (short_continent == "Africa" || short_continent == "America"
				|| short_continent == "Antarctica" || short_continent == "Asia"
				|| short_continent == "Atlantic" || short_continent == "Australia"
				|| short_continent == "Etc" || short_continent == "Europe"
				|| short_continent == "Indian" || short_continent == "Pacific")
			{
				for (auto file : Utils::FileSystem::getDirContent(continent, false))
				{
					if (!Utils::FileSystem::isDirectory(file))
					{
						auto tz = Utils::FileSystem::getFileName(file);
						if (std::find(ret.cbegin(), ret.cend(), tz) == ret.cend())
						ret.push_back(short_continent + "/" + tz);
					}
				}
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::string ApiSystem::getCurrentTimezone()
{
	LOG(LogInfo) << "ApiSystem::getCurrentTimezone";
	auto cmd = executeEnumerationScript("batocera-timezone get");
	std::string tz = Utils::String::join(cmd, "");
	remove_if(tz.begin(), tz.end(), isspace);
	if (tz.empty()) {
		cmd = executeEnumerationScript("batocera-timezone detect");
		tz = Utils::String::join(cmd, "");
	}
	return tz;
}

bool ApiSystem::setTimezone(std::string tz)
{
	if (tz.empty())
		return false;
	return executeScript("batocera-timezone set \"" + tz + "\"");
}

std::vector<PadInfo> ApiSystem::getPadsInfo()
{
	LOG(LogInfo) << "ApiSystem::getPadsInfo";

	std::vector<PadInfo> ret;

	auto res = executeEnumerationScript("batocera-padsinfo");
	std::string data = Utils::String::join(res, "\n");
	if (data.empty())
	{
		LOG(LogError) << "Package list is empty";
		return ret;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		return ret;
	}

	pugi::xml_node root = doc.child("pads");
	if (!root)
	{
		LOG(LogError) << "Could not find <pads> node";
		return ret;
	}

	for (pugi::xml_node pad = root.child("pad"); pad; pad = pad.next_sibling("pad"))
	{
		PadInfo pi;

		if (pad.attribute("device"))
			pi.device = pad.attribute("device").as_string();

		if (pad.attribute("id"))
			pi.id = Utils::String::toInteger(pad.attribute("id").as_string());

		if (pad.attribute("name"))
			pi.name = pad.attribute("name").as_string();

		if (pad.attribute("battery"))
			pi.battery = Utils::String::toInteger(pad.attribute("battery").as_string());

		if (pad.attribute("status"))
			pi.status = pad.attribute("status").as_string();

		if (pad.attribute("path"))
			pi.path = pad.attribute("path").as_string();

		ret.push_back(pi);
	}

	return ret;
}

std::string ApiSystem::getRunningArchitecture()
{
	auto res = executeEnumerationScript("uname -m");
	if (res.size() > 0)
		return res[0];

	return "";
}

std::string ApiSystem::getRunningBoard()
{
	auto res = executeEnumerationScript("cat /boot/boot/batocera.board");
	if (res.size() > 0)
		return res[0];

	return "";
}

std::string ApiSystem::getHostsName()
{
	auto hostName = SystemConf::getInstance()->get("system.hostname");
	if (!hostName.empty())
		return hostName;

	return "127.0.0.1";
}

bool ApiSystem::emuKill()
{
	LOG(LogDebug) << "ApiSystem::emuKill";
	return executeScript("batocera-es-swissknife --emukill");
}

void ApiSystem::suspend()
{
	LOG(LogDebug) << "ApiSystem::suspend";
	executeScript("/usr/bin/batocera-shutdown gui");
}

void ApiSystem::replugControllers_sindenguns()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_sindenguns";
	executeScript("/usr/bin/virtual-sindenlightgun-remap");
}

void ApiSystem::replugControllers_wiimotes()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_wiimotes";
	executeScript("/usr/bin/virtual-wii-mouse-bar-remap");
}

void ApiSystem::replugControllers_steamdeckguns()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_steamdeckguns";
	executeScript("/usr/bin/steamdeckgun-remap");
}

bool ApiSystem::isPlaneMode()
{
	auto res = executeEnumerationScript("batocera-planemode status");
	if (res.size() > 0)
		return res[0] == "on";

	return false;
}

bool ApiSystem::isReadPlaneModeSupported()
{
	return isScriptingSupported(READPLANEMODE);
}

bool ApiSystem::setPlaneMode(bool enable)
{
	LOG(LogDebug) << "ApiSystem::setPlaneMode";
	return executeScript("batocera-planemode " + std::string(enable ? "enable" : "disable"));
}

std::vector<Service> ApiSystem::getServices()
{
	std::vector<Service> services;

	LOG(LogDebug) << "ApiSystem::getServices";

	auto slines = executeEnumerationScript("batocera-services list");

	for (auto sline : slines) 
	{
		auto splits = Utils::String::split(sline, ';', true);
		if (splits.size() == 2) 
		{
			Service s;
			s.name = splits[0];
			s.enabled = (splits[1] == "*");
			services.push_back(s);
		}
	}
	return services;
}

std::vector<std::string> ApiSystem::backglassThemes() {
  std::vector<std::string> themes;

  LOG(LogDebug) << "ApiSystem::backglassThemes";

  auto slines = executeEnumerationScript("batocera-backglass list-themes");

  for (auto sline : slines) 
    {
      themes.push_back(sline);
    }
  return themes;
}

void ApiSystem::restartBackglass() {
  LOG(LogDebug) << "ApiSystem::restartBackglass";
  executeScript("/usr/bin/batocera-backglass restart");
}

bool ApiSystem::enableService(std::string name, bool enable) 
{
	std::string serviceName = name;
	if (serviceName.find(" ") != std::string::npos)
		serviceName = "\"" + serviceName + "\"";

	LOG(LogDebug) << "ApiSystem::enableService " << serviceName;

	bool res = executeScript("batocera-services " + std::string(enable ? "enable" : "disable") + " " + serviceName);
	if (res)
		res = executeScript("batocera-services " + std::string(enable ? "start" : "stop") + " " + serviceName);
	
	return res;
}

#ifdef _ENABLEEMUELEC
namespace
{
  // Runs 'work(i)' once for each i in [0, count). Only actually spreads that
  // across a Utils::ThreadPool (one thread per core) when this device
  // reports more than one core - the same "hardWareCoreCount > 1" check
  // ApiSystem::extractPdfImages() above already makes before parallelizing
  // its own work, and for the same reason: std::thread::hardware_concurrency()
  // can come back 0 (when it can't tell) or 1 on a single-core device, and a
  // ThreadPool with zero worker threads for its queue to run on spins
  // forever in wait() rather than actually finishing, while even a single
  // worker there just means that thread and wait()'s own busy-wait loop
  // fighting each other for the same one core, for no gain over doing the
  // work directly, in order, right here.
  void runFanned(size_t count, const std::function<void(size_t)>& work)
  {
    if (count == 0)
      return;

    // Diagnostic timing - temporary, see ApiSystem::getChildFolders()'s own
    // note. Reports the core count runFanned() actually saw so a genuinely
    // single/zero-core device (forced onto the serial fallback below, no
    // parallelism at all) can be told apart from one that should be getting
    // real concurrency here but for some other reason isn't.
    unsigned cores = std::thread::hardware_concurrency();
    StopWatch stopWatch("ApiSystem::runFanned(" + std::to_string(count) + " items, " + std::to_string(cores) + " cores reported) :", LogDebug);

    if (cores > 1)
    {
      Utils::ThreadPool pool(1);

      for (size_t i = 0; i < count; i++)
        pool.queueWorkItem([&work, i] { work(i); });

      pool.wait();
    }
    else
    {
      LOG(LogDebug) << "ApiSystem::runFanned - falling back to serial (hardware_concurrency() <= 1)";

      for (size_t i = 0; i < count; i++)
        work(i);
    }
  }

  // Lists 'systemRootPath's own direct child folders, filtering out the same
  // reserved names (media/medias, images, manuals, videos, assets, artwork,
  // downloaded_* and anything hidden) scanSystemFolderTree() and
  // topLevelFolderSetChanged() both used to filter after running this exact
  // listing as a separate "find <root> -mindepth 1 -maxdepth 1 -type d
  // -print" subprocess.
  //
  // That "find" turned out to be the actual multi-minute hang reported for
  // MAME's ROM root - a directory holding tens of thousands of flat ROM
  // files with no subfolders of their own. Logging confirmed the freeze sat
  // right at that command, never returning. The device's "find" is BusyBox's,
  // which - unlike GNU find - does not appear to trust "d_type" from
  // readdir() to answer "-type d" and instead lstat()s every single entry to
  // find out, turning "list the immediate children" into tens of thousands
  // of blocking stat() calls on exactly the kind of slow storage (SD card,
  // USB stick, SMB/NFS share) that makes each one expensive. Piping that many
  // result lines back through executeEnumerationScript()'s popen()+fgets()
  // loop only adds to it.
  //
  // SystemData::populateFolder() walks this identical MAME directory, with
  // this identical reserved-name exclusion list, at every single ES boot
  // with no reported slowness, because it never shells out to "find" at all
  // - it calls Utils::FileSystem::getDirectoryFiles(), which opendir()s the
  // folder once and reads each entry's type straight off dirent::d_type (a
  // stat() is only paid for a symlink entry, to resolve what it points at).
  // Reusing that same in-process listing here - instead of a second,
  // subprocess-based walk of the same folder - is what actually fixes the
  // hang, not just hides it behind a spinner.
  std::vector<std::string> listTopLevelSubfolders(const std::string& systemRootPath)
  {
    StopWatch stopWatch("ApiSystem::listTopLevelSubfolders(" + systemRootPath + ") :", LogDebug);

    std::vector<std::string> subRoots;

    for (auto& fi : Utils::FileSystem::getDirectoryFiles(systemRootPath))
    {
      if (!fi.directory)
        continue;

      std::string name = Utils::String::toLower(Utils::FileSystem::getFileName(fi.path));

      if (name.size() && name[0] == '.')
        continue;

      if (name == "media" || name == "medias" || name == "images" || name == "manuals" ||
        name == "videos" || name == "assets" || name == "artwork" ||
        Utils::String::startsWith(name, "downloaded_"))
        continue;

      subRoots.push_back(fi.path);
    }

    LOG(LogDebug) << "ApiSystem::listTopLevelSubfolders(" << systemRootPath << ") - " << subRoots.size() << " top-level folders found";

    return subRoots;
  }
}

std::vector<std::string> ApiSystem::scanSystemFolderTree(const std::string& systemRootPath)
{
  // Same reserved-name list SystemData::populateFolder() already skips when
  // it scans a system's ROM folder to build the actual gamelist tree
  // (media/medias, images, manuals, videos, assets, downloaded_* and
  // artwork, plus anything hidden) - only ever excluded here at
  // 'systemRootPath's own direct-child level, exactly as it was when this
  // was a single "-ipath"/"-prune" find call over the whole tree (kept
  // deliberately plain, portable "-print" rather than "-noleaf"/"-printf" -
  // see the git history on this function for why: those are GNU-findutils
  // extensions this device's "find" doesn't recognize, and an unsupported
  // option makes "find" refuse to run at all, which - since
  // executeEnumerationScript() only captures stdout, not stderr - came back
  // as a silent empty result rather than a visible error).
  //
  // What's different now is that this exclusion decision, and the recursive
  // walk of everything that survives it, are split into two steps so the
  // second one can be fanned out across threads instead of running as one
  // serial "find" process:
  //
  // Step 1: a fast, non-recursive listing of just 'systemRootPath's direct
  // children, via listTopLevelSubfolders() above - an in-process
  // opendir()/readdir() walk (the same one SystemData::populateFolder()
  // already relies on for this exact folder at every boot) rather than a
  // "find -mindepth 1 -maxdepth 1 -type d" subprocess. That "find" is what
  // was actually hanging for multiple minutes on MAME-sized flat ROM
  // directories - see listTopLevelSubfolders()'s own comment for why.
  //
  // Diagnostic timing - temporary, see getChildFolders()'s own note above.
  StopWatch stopWatch("ApiSystem::scanSystemFolderTree(" + systemRootPath + ") total :", LogDebug);

  std::vector<std::string> subRoots = listTopLevelSubfolders(systemRootPath);

  if (subRoots.empty())
    return std::vector<std::string>();

  // Step 2: one plain recursive "find <subRoot> -type d -print" per
  // surviving top-level folder - no "-prune" needed here at all, since the
  // reserved-name exclusion only ever applied at the top level above, so
  // everything under a kept folder is fair game - fanned out with
  // runFanned() above (falls back to running them one at a time in order on
  // a single-core device) so a ROM library with several top-level folders -
  // one per organizational folder the user made, one per arcade
  // compilation, etc - walks them all at the same time instead of one
  // process working through the whole tree alone. This only ever runs on
  // the very first scan for a system, or when isFolderTreeStale() below
  // catches an outside change - every other lookup is served straight out
  // of folders.xml - but that first scan is exactly the one that can take a
  // while on a big library, so it's the one worth parallelizing.
  //
  // Each queued item writes into its own index of a vector sized up front,
  // so no locking is needed between them.
  LOG(LogDebug) << "ApiSystem::scanSystemFolderTree(" << systemRootPath << ") - " << subRoots.size() << " top-level folders to recurse into";

  std::vector<std::vector<std::string>> perRootResults(subRoots.size());

  runFanned(subRoots.size(), [this, &perRootResults, &subRoots](size_t i)
  {
    std::string cmd = "find \"" + subRoots[i] + "\" -type d -print";
    perRootResults[i] = executeEnumerationScript(cmd);
  });

  std::vector<std::string> result;
  for (auto& sub : perRootResults)
    for (auto& path : sub)
      result.push_back(path);

  return result;
}

std::vector<ApiSystem::FolderCacheEntry> ApiSystem::readSystemFolderTree(const std::string& xmlPath, time_t& rootMtime, bool& ok)
{
  std::vector<FolderCacheEntry> entries;
  rootMtime = 0;

  pugi::xml_document doc;
  ok = (bool)doc.load_file(WINSTRINGW(xmlPath).c_str());
  if (!ok)
    return entries;

  pugi::xml_node root = doc.child("folders");
  rootMtime = (time_t)strtoll(root.attribute("root_mtime").as_string("0"), nullptr, 10);

  for (pugi::xml_node node = root.child("folder"); node; node = node.next_sibling("folder"))
  {
    FolderCacheEntry entry;
    entry.path = node.text().as_string();
    entry.mtime = (time_t)strtoll(node.attribute("mtime").as_string("0"), nullptr, 10);
    entries.push_back(entry);
  }

  return entries;
}

void ApiSystem::writeSystemFolderTree(const std::string& xmlPath, const std::vector<FolderCacheEntry>& folders, time_t rootMtime)
{
  pugi::xml_document doc;
  pugi::xml_node root = doc.append_child("folders");
  root.append_attribute("root_mtime").set_value(std::to_string((long long)rootMtime).c_str());

  for (auto& folder : folders)
  {
    pugi::xml_node node = root.append_child("folder");
    node.append_attribute("mtime").set_value(std::to_string((long long)folder.mtime).c_str());
    node.text().set(folder.path.c_str());
  }

  if (!doc.save_file(WINSTRINGW(xmlPath).c_str()))
    LOG(LogError) << "ApiSystem::writeSystemFolderTree - Error saving \"" << xmlPath << "\"!";
}

bool ApiSystem::topLevelFolderSetChanged(const std::string& systemRootPath, const std::vector<FolderCacheEntry>& master)
{
  // Diagnostic timing - temporary, see getChildFolders()'s own note above.
  StopWatch stopWatch("ApiSystem::topLevelFolderSetChanged(" + systemRootPath + ") :", LogDebug);

  // Same cheap, non-recursive top-level listing and reserved-name filter
  // scanSystemFolderTree()'s own "step 1" uses, via listTopLevelSubfolders()
  // above - an in-process directory read, not a "find" subprocess, so
  // isFolderTreeStale() below can afford to re-run this every time the ROM
  // root's own mtime has moved, rather than treating that alone as reason
  // enough to redo the whole (expensive) recursive rebuild. This used to
  // shell out to "find" itself, which is what was actually hanging for
  // minutes on MAME's flat ROM directory - see listTopLevelSubfolders()'s
  // own comment for why.
  std::set<std::string> currentTopLevel;
  for (auto& path : listTopLevelSubfolders(systemRootPath))
    currentTopLevel.insert(path);

  std::set<std::string> cachedTopLevel;
  for (auto& entry : master)
    if (Utils::FileSystem::getParent(entry.path) == systemRootPath)
      cachedTopLevel.insert(entry.path);

  return currentTopLevel != cachedTopLevel;
}

bool ApiSystem::isFolderTreeStale(const std::string& systemRootPath, time_t rootMtime, const std::vector<FolderCacheEntry>& master)
{
  // The ROM root's own mtime bumps for ANY direct child appearing or
  // disappearing - not just a real, tracked top-level folder, but also a
  // reserved one (media, images, downloaded_*, etc) scanSystemFolderTree()
  // already excludes from this cache entirely, most commonly a scraper
  // creating "media"/"images" the first time it saves artwork for a game in
  // this system. Treating that alone as "stale" used to mean throwing this
  // whole cache away and paying for a full recursive rebuild the next time
  // MOVE TO FOLDER was opened - silently, since folders.xml already existed
  // so the "building for the first time" notice never fired either, which
  // is exactly the kind of long, unexplained pause a scrape landing between
  // two "move to folder" uses would cause. Confirm with
  // topLevelFolderSetChanged() above that a real top-level folder actually
  // appeared or disappeared before trusting that.
  //
  // Diagnostic timing - temporary, see getChildFolders()'s own note above.
  StopWatch stopWatch("ApiSystem::isFolderTreeStale(" + systemRootPath + ", " + std::to_string(master.size()) + " entries) total :", LogDebug);

  if (Utils::FileSystem::getFileModificationDate(systemRootPath).getTime() != rootMtime)
  {
    LOG(LogDebug) << "ApiSystem::isFolderTreeStale(" << systemRootPath << ") - root mtime changed, checking top-level set";

    if (topLevelFolderSetChanged(systemRootPath, master))
    {
      LOG(LogDebug) << "ApiSystem::isFolderTreeStale(" << systemRootPath << ") - top-level folder set actually changed - stale";
      return true;
    }
  }

  // Same check, one level at a time, for every folder already known about:
  // gone entirely, or its own mtime has moved on from what was last
  // written - either way something changed inside it (a subfolder of ITS
  // own added, renamed, or removed) since this cache was last trusted. This
  // runs on every getChildFolders() lookup now, not just the first one, so
  // it's fanned out with runFanned() above the same as everywhere else in
  // this cache rather than checked one entry at a time.
  StopWatch perEntryStopWatch("ApiSystem::isFolderTreeStale(" + systemRootPath + ") - per-entry check of " + std::to_string(master.size()) + " folders :", LogDebug);

  std::atomic<bool> stale(false);

  runFanned(master.size(), [&master, &stale](size_t i)
  {
    auto& entry = master[i];

    if (!Utils::FileSystem::exists(entry.path) ||
      Utils::FileSystem::getFileModificationDate(entry.path).getTime() != entry.mtime)
      stale = true;
  });

  return stale.load();
}

std::vector<ApiSystem::FolderCacheEntry> ApiSystem::buildAndWriteFolderTree(const std::string& systemRootPath, const std::string& xmlPath, const std::vector<std::string>& folderPaths)
{
  // One stat() per folder to fill in its own FolderCacheEntry::mtime. On a
  // big system that can be just as many stat() calls as there are folders,
  // each one its own blocking disk/network round trip on exactly the kind
  // of slow ROM storage (SD card, USB stick, SMB/NFS share) this whole
  // cache exists to avoid re-paying on every lookup - so fan it out with
  // runFanned() above rather than walking the list one entry at a time.
  //
  // Diagnostic timing - temporary, see getChildFolders()'s own note above.
  StopWatch stopWatch("ApiSystem::buildAndWriteFolderTree(" + systemRootPath + ", " + std::to_string(folderPaths.size()) + " folders) total :", LogDebug);

  std::vector<FolderCacheEntry> master(folderPaths.size());

  runFanned(folderPaths.size(), [&master, &folderPaths](size_t i)
  {
    master[i].path = folderPaths[i];
    master[i].mtime = Utils::FileSystem::getFileModificationDate(folderPaths[i]).getTime();
  });

  time_t rootMtime = Utils::FileSystem::getFileModificationDate(systemRootPath).getTime();
  writeSystemFolderTree(xmlPath, master, rootMtime);

  return master;
}

std::vector<ApiSystem::FolderCacheEntry> ApiSystem::rebuildFolderTree(const std::string& systemRootPath, const std::string& xmlPath)
{
  // scanSystemFolderTree() only hands back each found folder's path - it
  // stays on plain, portable "find ... -print" (see that function's own
  // comment for why) - so buildAndWriteFolderTree() above is what actually
  // fills in each FolderCacheEntry::mtime and writes the result out.
  return buildAndWriteFolderTree(systemRootPath, xmlPath, scanSystemFolderTree(systemRootPath));
}

void ApiSystem::primeFolderTreeFromKnownFolders(std::string systemRootPath, const std::vector<std::string>& knownFolderPaths)
{
  // SystemData::SystemData() calls this right after populateFolder() walks
  // a system's ROM tree in memory to build its FolderData children - the
  // exact same folders (same reserved-name exclusions) scanSystemFolderTree()
  // would otherwise have to rediscover itself later, by spawning "find" all
  // over again, the first time the user opens MOVE TO FOLDER for this
  // system. Handing that list straight to buildAndWriteFolderTree() below
  // means folders.xml is already built - and already fresh, since it's
  // sourced from the walk that just happened - by the time this system
  // finishes loading, with no extra directory walk of its own: only the
  // one stat() per folder that function still needs for its own mtime
  // bookkeeping.
  std::string xmlPath = systemRootPath + "/folders.xml";

  // But if folders.xml already exists from a previous run, check first
  // whether anything it describes has actually changed - the exact same
  // mtime check getChildFolders() below runs on every lookup - before
  // paying to rebuild it. If nothing's stale, the cache already matches
  // disk exactly, so rebuilding it here would only write back the same
  // paths and mtimes it already holds: a stat() per folder and a flash
  // write for every system, on every single boot, whether or not the user
  // actually added, removed or moved anything since EmulationStation last
  // ran. Skip straight past all of that in the common case where nothing
  // changed, and only fall through to an actual rebuild when it did (or
  // when there's no usable cache yet at all).
  if (Utils::FileSystem::exists(xmlPath))
  {
    time_t rootMtime;
    bool ok;
    std::vector<FolderCacheEntry> master = readSystemFolderTree(xmlPath, rootMtime, ok);

    if (ok && !isFolderTreeStale(systemRootPath, rootMtime, master))
      return;
  }

  buildAndWriteFolderTree(systemRootPath, xmlPath, knownFolderPaths);
}

std::vector<std::string> ApiSystem::getChildFolders(std::string path, std::string systemRootPath)
{
  // The tree this describes normally only ever changes through
  // GuiFolderOptions.cpp's own CREATE/MOVE/RENAME/REMOVE FOLDER actions,
  // and each of those calls addFolderToCache() / moveFolderInCache() /
  // removeFolderFromCache() right afterwards to patch this same
  // folders.xml (and its stored mtimes) in place - so as long as it
  // exists and isFolderTreeStale() below doesn't find a mismatch, it's
  // always as current as the last change made through this app, and
  // there's no live scan to run here: the entire cost this exists to
  // avoid - "find" walking a folder cold on slow storage (an SD card, a
  // network share) - is paid only the first time for a given system, and
  // again only if something changes it from outside this app entirely
  // (over SSH/SFTP, from a USB stick, etc. - isFolderTreeStale() is what
  // actually catches that case), rather than every time EmulationStation
  // happens to restart, or every time one of those four actions changes
  // something in it.
  // Diagnostic timing - temporary, while tracking down where MOVE TO
  // FOLDER's own delay is actually going. Enable debug logging (Settings ->
  // Developer -> Enable debug logs, or "LogLevel" -> "debug" in
  // es_settings.xml) to see these in es_log.txt.
  StopWatch stopWatch("ApiSystem::getChildFolders(" + systemRootPath + ") total :", LogDebug);

  std::string xmlPath = systemRootPath + "/folders.xml";

  std::vector<FolderCacheEntry> master;
  bool haveCachedTree = false;
  time_t rootMtime = 0;

  if (Utils::FileSystem::exists(xmlPath))
    master = readSystemFolderTree(xmlPath, rootMtime, haveCachedTree);

  LOG(LogDebug) << "ApiSystem::getChildFolders(" << systemRootPath << ") - cache "
    << (haveCachedTree ? "loaded, " + std::to_string(master.size()) + " entries" : "missing or unreadable");

  if (haveCachedTree && isFolderTreeStale(systemRootPath, rootMtime, master))
  {
    LOG(LogDebug) << "ApiSystem::getChildFolders(" << systemRootPath << ") - cache is stale, rebuilding";
    haveCachedTree = false;
  }

  if (!haveCachedTree)
  {
    // Missing, corrupted (failed to parse), or stale (something changed
    // underneath it since it was last written, caught above) - either
    // way, nothing here can be trusted as-is, so scan for real and write
    // what this finds as the fresh cache from now on (see
    // rebuildFolderTree()'s own comment for exactly what that involves).
    // A system with no subfolders at all legitimately scans to an empty
    // list here - written out and trusted as-is on every call after this
    // one, never mistaken for "not cached" the way an empty 'master'
    // alone would be.
    LOG(LogDebug) << "ApiSystem::getChildFolders(" << systemRootPath << ") - rebuilding from a live scan";
    master = rebuildFolderTree(systemRootPath, xmlPath);
    LOG(LogDebug) << "ApiSystem::getChildFolders(" << systemRootPath << ") - rebuild found " << master.size() << " folders";
  }

  // Every caller from here down only ever wants the plain path list, same
  // as before this cache started tracking a per-entry mtime alongside it.
  std::vector<std::string> masterPaths;
  masterPaths.reserve(master.size());
  for (auto& entry : master)
    masterPaths.push_back(entry.path);

  // 'path' is the system's own ROM root itself when this is asking for its
  // whole tree (moveToFolder()'s own picker does this when 'file' lives
  // right at that root) - canonicalized on both sides for this one check
  // since 'path' is usually computed with base_path() rather than coming
  // from 'systemRootPath' directly, and the two can differ in trivial ways
  // (a trailing slash, a "." component) even when they name the same real
  // directory.
  if (Utils::FileSystem::getCanonicalPath(path) == Utils::FileSystem::getCanonicalPath(systemRootPath))
    return masterPaths;

  // Otherwise 'path' is some folder further down the tree - every entry
  // already in 'masterPaths' that's under IT, however many levels down,
  // same as this always returned (moveToFolder()'s own "down" picker can
  // offer a destination several levels under the current folder, not just
  // a direct child of it). Matched against the raw (non-canonicalized)
  // 'path', same as the "find" scan above always searched under it raw -
  // every path in 'masterPaths' comes from that same raw 'systemRootPath'
  // family, so a plain prefix match is all that's needed here.
  std::string prefix = path + "/";

  std::vector<std::string> filtered;
  for (auto& folder : masterPaths)
    if (Utils::String::startsWith(folder, prefix))
      filtered.push_back(folder);

  return filtered;
}

void ApiSystem::addFolderToCache(std::string systemRootPath, std::string newFolderPath)
{
  std::string xmlPath = systemRootPath + "/folders.xml";
  if (!Utils::FileSystem::exists(xmlPath))
    return;

  bool ok = false;
  time_t rootMtime = 0;
  std::vector<FolderCacheEntry> master = readSystemFolderTree(xmlPath, rootMtime, ok);
  if (!ok)
    return;

  bool alreadyCached = false;
  for (auto& entry : master)
    if (entry.path == newFolderPath)
      alreadyCached = true;

  if (!alreadyCached)
  {
    FolderCacheEntry entry;
    entry.path = newFolderPath;
    entry.mtime = Utils::FileSystem::getFileModificationDate(newFolderPath).getTime();
    master.push_back(entry);
  }

  // newFolderPath's own creation just changed its parent directory's
  // contents - refresh whichever mtime that parent has stored (the system
  // ROM root's own separate one, if that's what it is, or its own entry in
  // 'master' otherwise) to match, so getChildFolders()'s own staleness
  // check doesn't mistake this in-app change for an outside one on the
  // very next call and pay for a fresh "find" scan it doesn't need.
  std::string parentPath = Utils::FileSystem::getParent(newFolderPath);

  if (Utils::FileSystem::getCanonicalPath(parentPath) == Utils::FileSystem::getCanonicalPath(systemRootPath))
    rootMtime = Utils::FileSystem::getFileModificationDate(systemRootPath).getTime();
  else
  {
    for (auto& entry : master)
      if (entry.path == parentPath)
        entry.mtime = Utils::FileSystem::getFileModificationDate(parentPath).getTime();
  }

  writeSystemFolderTree(xmlPath, master, rootMtime);
}

void ApiSystem::moveFolderInCache(std::string systemRootPath, std::string oldPath, std::string newPath)
{
  std::string xmlPath = systemRootPath + "/folders.xml";
  if (!Utils::FileSystem::exists(xmlPath))
    return;

  bool ok = false;
  time_t rootMtime = 0;
  std::vector<FolderCacheEntry> master = readSystemFolderTree(xmlPath, rootMtime, ok);
  if (!ok)
    return;

  std::string oldPrefix = oldPath + "/";
  bool haveNewPath = false;

  for (auto& entry : master)
  {
    if (entry.path == oldPath)
      entry.path = newPath;
    else if (Utils::String::startsWith(entry.path, oldPrefix))
      entry.path = newPath + entry.path.substr(oldPath.size());

    if (entry.path == newPath)
      haveNewPath = true;
  }

  // oldPath's own entry is what usually carries this rename/move through
  // to newPath above - this only has to add it separately in the rare
  // case oldPath itself was never cached (e.g. this system's tree was
  // scanned before oldPath even existed), so a fresh "find" scan would
  // still be the ONLY thing that ever lists newPath's own entry rather
  // than losing it. Any of oldPath's own descendants, if there were any,
  // are already carried over by the loop above regardless.
  if (!haveNewPath)
  {
    FolderCacheEntry entry;
    entry.path = newPath;
    entry.mtime = Utils::FileSystem::getFileModificationDate(newPath).getTime();
    master.push_back(entry);
  }

  // oldPath leaving its old parent, and newPath's entry landing in its new
  // one (the same parent, for a plain rename - refreshed once either way,
  // since both lookups resolve to the same stored mtime), are both
  // directory-content changes that would otherwise look like an outside
  // edit to getChildFolders()'s own staleness check on its very next call
  // - same reasoning addFolderToCache() above gives for its own new
  // folder's parent.
  auto refreshParentMtime = [&](const std::string& parentOfChange)
  {
    if (Utils::FileSystem::getCanonicalPath(parentOfChange) == Utils::FileSystem::getCanonicalPath(systemRootPath))
      rootMtime = Utils::FileSystem::getFileModificationDate(systemRootPath).getTime();
    else
    {
      for (auto& entry : master)
        if (entry.path == parentOfChange)
          entry.mtime = Utils::FileSystem::getFileModificationDate(parentOfChange).getTime();
    }
  };

  refreshParentMtime(Utils::FileSystem::getParent(oldPath));
  refreshParentMtime(Utils::FileSystem::getParent(newPath));

  writeSystemFolderTree(xmlPath, master, rootMtime);
}

void ApiSystem::removeFolderFromCache(std::string systemRootPath, std::string removedPath)
{
  std::string xmlPath = systemRootPath + "/folders.xml";
  if (!Utils::FileSystem::exists(xmlPath))
    return;

  bool ok = false;
  time_t rootMtime = 0;
  std::vector<FolderCacheEntry> master = readSystemFolderTree(xmlPath, rootMtime, ok);
  if (!ok)
    return;

  std::string prefix = removedPath + "/";

  std::vector<FolderCacheEntry> filtered;
  for (auto& entry : master)
    if (entry.path != removedPath && !Utils::String::startsWith(entry.path, prefix))
      filtered.push_back(entry);

  if (filtered.size() == master.size())
    return;

  // removedPath leaving its (former) parent is a directory-content change
  // that would otherwise look like an outside edit to getChildFolders()'s
  // own staleness check on its very next call - same reasoning
  // addFolderToCache() above gives for its own new folder's parent.
  // removedPath itself is gone from disk by now, but getParent() is a
  // plain string operation - it never has to stat 'removedPath' itself to
  // find where it used to live.
  std::string parentPath = Utils::FileSystem::getParent(removedPath);

  if (Utils::FileSystem::getCanonicalPath(parentPath) == Utils::FileSystem::getCanonicalPath(systemRootPath))
    rootMtime = Utils::FileSystem::getFileModificationDate(systemRootPath).getTime();
  else
  {
    for (auto& entry : filtered)
      if (entry.path == parentPath)
        entry.mtime = Utils::FileSystem::getFileModificationDate(parentPath).getTime();
  }

  writeSystemFolderTree(xmlPath, filtered, rootMtime);
}

void ApiSystem::rescanFolderTree(std::string systemRootPath)
{
  // Unlike addFolderToCache() and friends above, this never no-ops on a
  // missing folders.xml - rebuildFolderTree() below scans and writes a
  // fresh one regardless, same as getChildFolders() itself does the very
  // first time it's asked about a system that's never had one.
  std::string xmlPath = systemRootPath + "/folders.xml";
  rebuildFolderTree(systemRootPath, xmlPath);
}

void ApiSystem::refreshFolderMtime(std::string systemRootPath, std::string folderPath)
{
  std::string xmlPath = systemRootPath + "/folders.xml";
  if (!Utils::FileSystem::exists(xmlPath))
    return;

  bool ok = false;
  time_t rootMtime = 0;
  std::vector<FolderCacheEntry> master = readSystemFolderTree(xmlPath, rootMtime, ok);
  if (!ok)
    return;

  // A plain GAME move never adds, removes or renames anything in
  // 'master' - the file that actually moved was never a cache entry to
  // begin with - so this only ever touches one existing entry's stored
  // mtime (or the cache's separate root mtime) in place, same as
  // addFolderToCache() and friends above do for their own affected
  // parent, just without any of their own path-list bookkeeping around
  // it.
  if (Utils::FileSystem::getCanonicalPath(folderPath) == Utils::FileSystem::getCanonicalPath(systemRootPath))
  {
    rootMtime = Utils::FileSystem::getFileModificationDate(systemRootPath).getTime();
    writeSystemFolderTree(xmlPath, master, rootMtime);
    return;
  }

  for (auto& entry : master)
  {
    if (entry.path != folderPath)
      continue;

    entry.mtime = Utils::FileSystem::getFileModificationDate(folderPath).getTime();
    writeSystemFolderTree(xmlPath, master, rootMtime);
    return;
  }

  // 'folderPath' isn't a tracked entry (e.g. it was never picked up by
  // the last "find" scan) - nothing here to refresh, and nothing to
  // write back either, since 'master' and 'rootMtime' are otherwise
  // exactly what's already on disk.
}
#endif
