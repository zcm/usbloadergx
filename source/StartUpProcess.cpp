#include <unistd.h>
#include "StartUpProcess.h"
#include "GUI/gui.h"
#include "video.h"
#include "audio.h"
#include "input.h"
#include "themes/CTheme.h"
#include "gecko.h"
#include "wpad.h"
#include "Controls/DeviceHandler.hpp"
#include "wad/nandtitle.h"
#include "SystemMenu/SystemMenuResources.h"
#include "system/IosLoader.h"
#include "libs/libruntimeiospatch/runtimeiospatch.h"
#include "utils/timer.h"
#include "settings/CSettings.h"
#include "settings/CGameSettings.h"
#include "settings/CGameStatistics.h"
#include "settings/CGameCategories.hpp"
#include "settings/GameTitles.h"
#include "usbloader/usbstorage2.h"
#include "usbloader/MountGamePartition.h"
#include "usbloader/GameBooter.hpp"
#include "usbloader/GameList.h"
#include "utils/tools.h"
#include "sys.h"
#include "svnrev.h"
#include "gitver.h"
#include "usbloader/sdhc.h"
#include "settings/meta.h"

extern bool isWiiVC; // in sys.cpp
extern u8 sdhc_mode_sd;

StartUpProcess::StartUpProcess()
{
	//! Load default font for the next text outputs
	Theme::LoadFont("");

	GXImageData = Resources::GetImageData("gxlogo.png");
	GXImage = new GuiImage(GXImageData);
	GXImage->SetAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
	GXImage->SetPosition(screenwidth / 2, screenheight / 2 - 50);

	messageTxt = new GuiText(" ", 22, (GXColor){255, 255, 255, 255});
	messageTxt->SetAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
	messageTxt->SetPosition(screenwidth / 2, screenheight / 2 + 60);

	if (strncmp(Settings.ConfigPath, "sd", 2) == 0)
		cancelTxt = new GuiText("Press B to cancel or A to enable SD card mode", 22, (GXColor){255, 255, 255, 255});
	else
		cancelTxt = new GuiText("Press B to cancel", 22, (GXColor){255, 255, 255, 255});
	cancelTxt->SetAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
	cancelTxt->SetPosition(screenwidth / 2, screenheight / 2 + 90);

	trigB = new GuiTrigger;
	trigB->SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);

	cancelBtn = new GuiButton(0, 0);
	cancelBtn->SetTrigger(trigB);

	trigA = new GuiTrigger;
	trigA->SetButtonOnlyTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	sdmodeBtn = new GuiButton(0, 0);
	if (strncmp(Settings.ConfigPath, "sd", 2) == 0)
		sdmodeBtn->SetTrigger(trigA);

	drawCancel = false;
}

StartUpProcess::~StartUpProcess()
{
	delete GXImageData;
	delete GXImage;
	delete messageTxt;
	delete cancelTxt;
	delete cancelBtn;
	delete sdmodeBtn;
	delete trigB;
	delete trigA;
}

int StartUpProcess::ParseArguments(int argc, char *argv[])
{
	int quickBoot = -1;

	//! The arguments override
	for (int i = 0; i < argc; ++i)
	{
		if (!argv[i])
			continue;

		gprintf("Boot argument %i: %s\n", i + 1, argv[i]);

		char *ptr = strcasestr(argv[i], "-ios=");
		if(ptr)
		{
			if(atoi(ptr+strlen("-ios=")) == 58)
				Settings.LoaderIOS = 58;
			else
				Settings.LoaderIOS = LIMIT(atoi(ptr+strlen("-ios=")), 200, 255);
		}

		ptr = strcasestr(argv[i], "-bootios=");
		if (ptr)
		{
			if (atoi(ptr + strlen("-bootios=")) == 58)
				Settings.BootIOS = 58;
			else
				Settings.BootIOS = LIMIT(atoi(ptr + strlen("-bootios=")), 200, 255);
		}

		ptr = strcasestr(argv[i], "-usbport=");
		if (ptr)
		{
			Settings.USBPort = LIMIT(atoi(ptr + strlen("-usbport=")), 0, 2);
		}

		ptr = strcasestr(argv[i], "-mountusb=");
		if (ptr)
		{
			Settings.USBAutoMount = LIMIT(atoi(ptr + strlen("-mountusb=")), 0, 1);
		}

		if (strncmp(Settings.ConfigPath, "sd", 2) == 0)
		{
			ptr = strcasestr(argv[i], "-sdmode=");
			if (ptr)
			{
				Settings.SDMode = LIMIT(atoi(ptr + strlen("-sdmode=")), 0, 1);
				if (Settings.SDMode)
					sdhc_mode_sd = 1;
			}
		}

		if (strlen(argv[i]) == 6 && strchr(argv[i], '=') == 0 && strchr(argv[i], '-') == 0)
			quickBoot = i;
	}

	return quickBoot;
}

void StartUpProcess::SetTextf(const char *format, ...)
{
	char *tmp = NULL;
	va_list va;
	va_start(va, format);
	if ((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
		gprintf(tmp);
		messageTxt->SetText(tmp);
		Draw();
	}
	va_end(va);

	if (tmp)
		free(tmp);
}

bool StartUpProcess::USBSpinUp()
{
	drawCancel = true;
	Timer countDown;
	bool started0 = false;
	bool started1 = false;

	const DISC_INTERFACE *handle0 = NULL;
	const DISC_INTERFACE *handle1 = NULL;
	if (Settings.USBPort == 0 || Settings.USBPort == 2)
		handle0 = DeviceHandler::GetUSB0Interface();
	if (Settings.USBPort == 1 || Settings.USBPort == 2)
		handle1 = DeviceHandler::GetUSB1Interface();

	// wait 20 sec for the USB to spin up...stupid slow ass HDD
	do
	{
		if (handle0)
			started0 = (handle0->startup() && handle0->isInserted());

		if (handle1)
			started1 = (handle1->startup() && handle1->isInserted());

		if ((!handle0 || started0) && (!handle1 || started1))
			break;

		UpdatePads();
		for (int i = 0; i < 4; ++i)
		{
			cancelBtn->Update(&userInput[i]);
			sdmodeBtn->Update(&userInput[i]);
		}

		if (cancelBtn->GetState() == STATE_CLICKED)
			break;

		if (sdmodeBtn->GetState() == STATE_CLICKED)
		{
			Settings.SDMode = ON;
			sdhc_mode_sd = 1;
			break;
		}

		messageTxt->SetTextf("Waiting for HDD: %i sec left\n", 20 - (int)countDown.elapsed());
		Draw();
		usleep(50000);
	} while (countDown.elapsed() < 20.f);

	drawCancel = false;

	return (started0 || started1);
}

int StartUpProcess::Run(int argc, char *argv[])
{
	// A normal launch should always have the first arg be the path
	char *ptr = strrchr(argv[0], '/');
	if (ptr && (argv[0][2] == ':' || argv[0][3] == ':'))
	{
		*ptr = 0;
		// HBC doesn't specify the USB port
		if (strncmp(argv[0], "usb", 3) == 0)
		{
			snprintf(Settings.BootDevice, sizeof(Settings.BootDevice), "usb1");
			snprintf(Settings.ConfigPath, sizeof(Settings.ConfigPath), "usb1:%s/", argv[0] + 4);
		}
		else if (strncmp(argv[0], "sd", 2) == 0)
			snprintf(Settings.ConfigPath, sizeof(Settings.ConfigPath), "%s/", argv[0]);
		gprintf("Loader path: %s\n", Settings.ConfigPath);
	}
	int quickGameBoot = ParseArguments(argc, argv);

	StartUpProcess Process;

	int ret = Process.Execute(quickGameBoot != -1);

	if (quickGameBoot != -1)
		return QuickGameBoot(argv[quickGameBoot]);

	return ret;
}

void StartUpProcess::LoadIOS(u8 ios, bool boot)
{
	SetTextf("Reloading to IOS%d%s\n", ios, boot ? " requested in meta.xml" : "");
	if (IosLoader::LoadAppCios(ios) < 0)
	{
		SetTextf("Failed to load an IOS. USB Loader GX requires a cIOS or IOS58 with AHB access. Exiting...\n");
		sleep(5);
		Sys_BackToLoader();
	}
	SetTextf("Reloaded to IOS%d r%d\n", Settings.LoaderIOS, IOS_GetRevision());
}

static void *StartUpISFS(void *arg) {
	bool *result = (bool *) arg;

	// Enable isfs permission if using Hermes v4 without AHB, or WiiU WiiVC (IOS255 fw.img)
	if (IOS_GetVersion() < 200 || (IosLoader::IsHermesIOS() && IOS_GetRevision() == 4) || isWiiVC)
	{
		//SetTextf("Patching IOS%d\n", IOS_GetVersion());
		if (IosPatch_RUNTIME(!isWiiVC, false, false, isWiiVC, false) == ERROR_PATCH)
			gprintf("Patching IOS%d failed!\n", IOS_GetVersion());
		else
			NandTitles.Get(); // get NAND channel's titles

		gprintf("Current IOS: %d - have AHB access: %s\n", IOS_GetVersion(), AHBPROT_DISABLED ? "yes" : "no");
	}

	// We only initialize once for the whole session
	ISFS_Initialize();

	// Check MIOS version
	//SetTextf("Checking installed MIOS\n");
	IosLoader::GetMIOSInfo();

	//SetTextf("Loading resources\n");
	*result = SystemMenuResources::Instance()->IsLoaded()
		        || SystemMenuResources::Instance()->Init();

	return NULL;
}

int StartUpProcess::Execute(bool quickGameBoot)
{
	Settings.EntryIOS = IOS_GetVersion();
	// Disable AHBPROT
	IosPatch_AHBPROT(false);

	// Store dx2 cIOS info
	IosLoader::GetD2XInfo();

	gprintf("Current IOS: %d - have AHB access: %s\n", Settings.EntryIOS, AHBPROT_DISABLED ? "yes" : "no");
	// Reload to a cIOS if we're using both USB ports
	if (Settings.USBPort == 2 && !Settings.SDMode)
		LoadIOS(Settings.LoaderIOS, false);

	// Reload to a cIOS if required (old forwarder?) or requested
	else if (!AHBPROT_DISABLED || (Settings.EntryIOS != Settings.BootIOS))
		LoadIOS(Settings.BootIOS, true);

	// Setup the pads
	SetupPads();

	// Mount the SD card
	SetTextf("Initializing SD card\n");
	DeviceHandler::Instance()->MountSD();

	// Do not mount USB if not needed. USB is not available with WiiU WiiVC injected channel
	bool USBSuccess = false;
	if (Settings.USBAutoMount == ON && !isWiiVC && !Settings.SDMode)
	{
		SetTextf("Initializing USB devices\n");
		if (USBSpinUp())
		{
			DeviceHandler::Instance()->MountAllUSB(false);
			USBSuccess = true;
			gprintf("Completed initialization of USB devices\n");
		}
	}

	SetTextf("Loading config files\n");

#ifdef NO_DEBUG
#define STORE_LOAD_RESULT(x) x
#else
#define STORE_LOAD_RESULT(x) load_result = x

	bool load_result;
#endif  /* NO_DEBUG */

	STORE_LOAD_RESULT(Settings.Load());
	gprintf("\tLoading config...%s\n", load_result ? "done" : "failed");

	STORE_LOAD_RESULT(Settings.LoadLanguage(Settings.language_path, CONSOLE_DEFAULT));
	gprintf("\tLoading language...%s\n", load_result ? "done" : "failed");

	STORE_LOAD_RESULT(GameSettings.Load(Settings.ConfigPath));
	gprintf("\tLoading game settings...%s\n", load_result ? "done" : "failed");

	STORE_LOAD_RESULT(GameStatistics.Load(Settings.ConfigPath));
	gprintf("\tLoading game statistics...%s\n", load_result ? "done" : "failed");

	STORE_LOAD_RESULT(GameCategories.Load(Settings.ConfigPath));
	gprintf("\tLoading game categories...%s\n", load_result ? "done" : "failed");

	STORE_LOAD_RESULT(GameTitles.ReadCachedTitles(Settings.titlestxt_path));
	gprintf("\tLoading cached titles...%s\n", load_result ? "done" : "failed (using default)");

	// Some settings need to be enabled to boot directly into games
	gprintf("Quick game boot: %s\n", quickGameBoot ? "yes" : "no");
	if (quickGameBoot)
	{
		Settings.USBAutoMount = ON;
		Settings.LoaderMode = MODE_ALL;
		Settings.skipSaving = true;
	}

	// Reload to users settings if different than current IOS, and if not using an injected WiiU WiiVC IOS255 (fw.img)
	if (Settings.LoaderIOS != IOS_GetVersion() && !isWiiVC)
	{
		// Shutdown pads
		sleep(1); // Some Wiimotes won't reconnect as player 1 without this
		Wpad_Disconnect();

		// Unmount devices
		DeviceHandler::DestroyInstance();
		if (Settings.USBAutoMount == ON)
			USBStorage2_Deinit();

		// Now load the cIOS that was set in the settings menu
		if (IosLoader::LoadAppCios(Settings.LoaderIOS) > -1)
		{
			SetTextf("Reloaded to IOS%d r%d\n", Settings.LoaderIOS, IOS_GetRevision());
			// Re-Mount devices
			SetTextf("Reinitializing devices\n");
		}
		gprintf("Current IOS: %d - have AHB access: %s\n", IOS_GetVersion(), AHBPROT_DISABLED ? "yes" : "no");

		// Start pads again
		SetupPads();

		DeviceHandler::Instance()->MountSD();
		if (Settings.USBAutoMount == ON && !Settings.SDMode && USBSuccess)
		{
			if (USBSpinUp())
				DeviceHandler::Instance()->MountAllUSB(false);
		}
	}

	// Need to wait until all reloads are complete before initializing ISFS
	lwp_t isfsWorker;
	u8 stack[8192];
	bool systemMenuResourcesReady = false;

	LWP_CreateThread(&isfsWorker, StartUpISFS, &systemMenuResourcesReady, stack, sizeof(stack), 60);

	gprintf("\tLoading language...%s\n", Settings.LoadLanguage(Settings.language_path, CONSOLE_DEFAULT) ? "done" : "failed");
	gprintf("\tLoading game settings...%s\n", GameSettings.Load(Settings.ConfigPath) ? "done" : "failed");
	gprintf("\tLoading game statistics...%s\n", GameStatistics.Load(Settings.ConfigPath) ? "done" : "failed");
	gprintf("\tLoading game categories...%s\n", GameCategories.Load(Settings.ConfigPath) ? "done" : "failed");
	gprintf("\tLoading cached titles...%s\n", GameTitles.ReadCachedTitles(Settings.titlestxt_path) ? "done" : "failed (using default)");

	if (sdhc_mode_sd)
		editMetaArguments();

	if (!IosLoader::IsHermesIOS() && !IosLoader::IsD2X() && !Settings.SDMode)
	{
		Settings.USBPort = 0;
	}
	else if (Settings.USBPort == 1 && USBStorage2_GetPort() != Settings.USBPort && !Settings.SDMode)
	{
		if (Settings.USBAutoMount == ON && !isWiiVC)
		{
			SetTextf("Changing USB port to %i\n", Settings.USBPort);
			DeviceHandler::Instance()->UnMountAllUSB();
			DeviceHandler::Instance()->MountAllUSB();
		}
	}
	else if (Settings.USBPort == 2 && !Settings.SDMode)
	{
		if (Settings.USBAutoMount == ON && !isWiiVC)
		{
			SetTextf("Mounting USB port to 1\n");
			DeviceHandler::Instance()->MountUSBPort1();
		}
	}

	SetTextf("Waiting for resources");
	LWP_JoinThread(isfsWorker, NULL);

	// Do not allow banner grid mode without AHBPROT
	// this function does nothing if it was already initiated before
	if (!systemMenuResourcesReady && Settings.gameDisplay == BANNERGRID_MODE)
	{
		Settings.gameDisplay = LIST_MODE;
		Settings.GameWindowMode = GAMEWINDOW_DISC;
	}

	STORE_LOAD_RESULT(Theme::LoadFont(Settings.ConfigPath));
	gprintf("\tLoading font...%s\n", load_result ? "done" : "failed (using default)");

	STORE_LOAD_RESULT(Theme::Load(Settings.theme));
	gprintf("\tLoading theme...%s\n", load_result ? "done" : "failed (using default)");

#undef STORE_LOAD_RESULT

	//! Init the rest of the system
	Sys_Init();
	InitAudio();
	setlocale(LC_CTYPE, "en_US.UTF-8");
	setlocale(LC_MESSAGES, "en_US.UTF-8");
	AdjustOverscan(Settings.AdjustOverscanX, Settings.AdjustOverscanY);

	return 0;
}

void StartUpProcess::Draw()
{
	GXImage->Draw();
	messageTxt->Draw();
	if (drawCancel)
		cancelTxt->Draw();
	Menu_Render();
}

int StartUpProcess::QuickGameBoot(const char *gameID)
{
	MountGamePartition(false);

	struct discHdr *header = NULL;
	for (int i = 0; i < gameList.size(); ++i)
	{
		if (strncasecmp((char *)gameList[i]->id, gameID, 6) == 0)
			header = gameList[i];
	}

	if (!header)
		return -1;

	GameStatistics.SetPlayCount(header->id, GameStatistics.GetPlayCount(header->id) + 1);
	GameStatistics.Save();

	return GameBooter::BootGame(header);
}
