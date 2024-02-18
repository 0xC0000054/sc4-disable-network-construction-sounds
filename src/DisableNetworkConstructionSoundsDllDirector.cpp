///////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-disable-network-construction-sounds, a DLL Plugin
// for SimCity 4 that disables the sounds that are played during the network
// construction animation.
//
// Copyright (c) 2024 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
///////////////////////////////////////////////////////////////////////////////

#include "version.h"
#include "Logger.h"
#include "SC4VersionDetection.h"

#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cRZCOMDllDirector.h"

#include <filesystem>

#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"

static constexpr uint32_t kDisableNetworkConstructionSoundsDirectorID = 0xC2D4CE53;

static constexpr std::string_view PluginLogFileName = "SC4DisableNetworkConstructionSounds.log";

namespace
{
	std::filesystem::path GetDllFolderPath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		std::filesystem::path temp(modulePath.get());

		return temp.parent_path();
	}

	void OverwriteMemory(uintptr_t address, uint8_t newValue)
	{
		DWORD oldProtect;
		// Allow the executable memory to be written to.
		THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
			reinterpret_cast<LPVOID>(address),
			sizeof(newValue),
			PAGE_EXECUTE_READWRITE,
			&oldProtect));

		// Patch the memory at the specified address.
		*((uint8_t*)address) = newValue;
	}

	void DisableNetworkConstructionAnimationSounds()
	{
		Logger& logger = Logger::GetInstance();

		const uint16_t gameVersion = SC4VersionDetection::GetInstance().GetGameVersion();

		if (gameVersion == 641)
		{
			try
			{
				// Note that disabling the animation sounds will also disable 2 in-game messages that are sent
				// at the start and end of the animation sequence to notify the component that is responsible
				// for playing the sounds. The messages are: kSC4MsgConstructionRoadCrewStart (0x89F10866) and
				// kSC4MsgConstructionRoadCrewEnd (0xA9F10E45).
				//
				// The class that manages the network construction animations (cSC4NetworkConstructionCrew) checks
				// that a class member pointer to the game's sound system is not null before playing the sounds
				// that go with the animations.
				// The class member pointer is initialized in cSC4NetworkConstructionCrew::Init, where it leaves
				// the pointer null if the game's global sound service pointer is also null, which presumably
				// would be the case if the game's audio has been disabled.
				//
				// We modify that check to make the game think the global pointer is always null by replacing the
				// conditional short jump that is taken when the pointer is null with an unconditional short jump.
				//
				// Original instruction: 0x74 (JZ rel8).
				// New instruction: 0xEB (JMP rel8).
				OverwriteMemory(0x6071FC, 0xEB);
				logger.WriteLine(LogLevel::Info, "Disabled the network construction animation sounds.");
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to disable the network construction animation sounds: %s",
					e.what());
			}
		}
		else
		{
			logger.WriteLineFormatted(
				LogLevel::Error,
				"Unable to disable the network construction animation sounds. Requires "
				"game version 641, found game version %d.",
				gameVersion);
		}
	}
}

class DisableNetworkConstructionSoundsDllDirector final : public cRZCOMDllDirector
{
public:

	DisableNetworkConstructionSoundsDllDirector()
	{
		std::filesystem::path dllFolderPath = GetDllFolderPath();

		std::filesystem::path logFilePath = dllFolderPath;
		logFilePath /= PluginLogFileName;

		Logger& logger = Logger::GetInstance();
		logger.Init(logFilePath, LogLevel::Error, false);
		logger.WriteLogFileHeader("SC4DisableNetworkConstructionSounds v" PLUGIN_VERSION_STR);
	}

	uint32_t GetDirectorID() const
	{
		return kDisableNetworkConstructionSoundsDirectorID;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		DisableNetworkConstructionAnimationSounds();

		return true;
	}
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static DisableNetworkConstructionSoundsDllDirector sDirector;
	return &sDirector;
}