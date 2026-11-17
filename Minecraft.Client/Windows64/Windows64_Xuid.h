#pragma once

#ifdef _WINDOWS64

#include <Windows.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace Win64Xuid
{
	inline PlayerUID GetLegacyEmbeddedHostXuid()
	{
		return (PlayerUID)0xe000d45248242f2eULL;
	}

	inline bool IsLegacyEmbeddedXuid(PlayerUID xuid)
	{
		const PlayerUID base = GetLegacyEmbeddedHostXuid();
		return xuid >= base && xuid < base + MINECRAFT_NET_MAX_PLAYERS;
	}

	inline bool IsValidPersistentXuid(PlayerUID xuid)
	{
		return xuid != INVALID_XUID && !IsLegacyEmbeddedXuid(xuid);
	}

	inline bool GetUidFilePath(char *path, size_t pathSize)
	{
		if(path == NULL || pathSize == 0) return false;

		char executablePath[MAX_PATH] = {};
		DWORD length = GetModuleFileNameA(NULL, executablePath, MAX_PATH);
		if(length == 0 || length >= MAX_PATH) return false;

		char *separator = strrchr(executablePath, '\\');
		if(separator == NULL) return false;
		*(separator + 1) = '\0';

		return strcpy_s(path, pathSize, executablePath) == 0
			&& strcat_s(path, pathSize, "uid.dat") == 0;
	}

	inline bool ReadUidFile(PlayerUID *xuid)
	{
		if(xuid == NULL) return false;

		char path[MAX_PATH] = {};
		if(!GetUidFilePath(path, sizeof(path))) return false;

		FILE *file = NULL;
		if(fopen_s(&file, path, "rb") != 0 || file == NULL) return false;

		char buffer[128] = {};
		size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
		fclose(file);
		if(length == 0) return false;

		if(length == sizeof(PlayerUID))
		{
			PlayerUID raw = INVALID_XUID;
			memcpy(&raw, buffer, sizeof(raw));
			if(IsValidPersistentXuid(raw))
			{
				*xuid = raw;
				return true;
			}
		}

		buffer[length] = '\0';
		char *begin = buffer;
		while(*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n') ++begin;

		errno = 0;
		char *end = NULL;
		unsigned __int64 value = _strtoui64(begin, &end, 0);
		if(begin == end || errno != 0) return false;
		while(*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
		if(*end != '\0') return false;

		PlayerUID parsed = (PlayerUID)value;
		if(!IsValidPersistentXuid(parsed)) return false;

		*xuid = parsed;
		return true;
	}

	inline bool WriteUidFile(PlayerUID xuid)
	{
		char path[MAX_PATH] = {};
		if(!GetUidFilePath(path, sizeof(path))) return false;

		FILE *file = NULL;
		if(fopen_s(&file, path, "wb") != 0 || file == NULL) return false;

		int result = fprintf_s(file, "0x%016llX\n", (unsigned long long)xuid);
		bool success = result > 0 && fflush(file) == 0;
		if(fclose(file) != 0) success = false;
		return success;
	}

	inline unsigned __int64 Mix(unsigned __int64 value)
	{
		value += 0x9E3779B97F4A7C15ULL;
		value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
		value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
		return value ^ (value >> 31);
	}

	inline PlayerUID CreatePersistentXuid()
	{
		FILETIME time = {};
		GetSystemTimeAsFileTime(&time);
		unsigned __int64 seed = ((unsigned __int64)time.dwHighDateTime << 32) | time.dwLowDateTime;

		LARGE_INTEGER counter = {};
		QueryPerformanceCounter(&counter);
		seed ^= (unsigned __int64)counter.QuadPart;
		seed ^= (unsigned __int64)GetCurrentProcessId() << 32;
		seed ^= (unsigned __int64)GetCurrentThreadId();
		seed ^= (unsigned __int64)GetTickCount();
		seed ^= (unsigned __int64)(size_t)&counter;

		unsigned __int64 value = Mix(seed) ^ Mix(seed + 0xA0761D6478BD642FULL);
		value ^= 0x8F4B2D6C1A93E705ULL;
		value |= 0x8000000000000000ULL;

		PlayerUID xuid = (PlayerUID)value;
		if(!IsValidPersistentXuid(xuid))
		{
			xuid = (PlayerUID)(value ^ 0x0100000000000001ULL);
		}
		if(!IsValidPersistentXuid(xuid))
		{
			xuid = (PlayerUID)0xD15EA5E000000001ULL;
		}
		return xuid;
	}

	inline PlayerUID XuidForPad(PlayerUID baseXuid, int iPad)
	{
		if(iPad < 0 || iPad >= XUSER_MAX_COUNT) return INVALID_XUID;
		if(iPad == 0) return baseXuid;

		unsigned __int64 value = Mix((unsigned __int64)baseXuid + (unsigned __int64)iPad);
		value |= 0x8000000000000000ULL;
		PlayerUID xuid = (PlayerUID)value;
		if(!IsValidPersistentXuid(xuid))
		{
			xuid = (PlayerUID)(value ^ 0x0100000000000001ULL);
		}
		if(!IsValidPersistentXuid(xuid))
		{
			xuid = (PlayerUID)(0xD15EA5E000000001ULL + iPad);
		}
		return xuid;
	}

	inline PlayerUID ResolvePersistentXuid()
	{
		static std::mutex resolveMutex;
		std::lock_guard<std::mutex> lock(resolveMutex);
		static bool resolved = false;
		static PlayerUID xuid = INVALID_XUID;
		if(resolved) return xuid;

		if(!ReadUidFile(&xuid))
		{
			xuid = CreatePersistentXuid();
			if(!WriteUidFile(xuid))
			{
				OutputDebugStringA("Unable to persist uid.dat; this player's save identity may change on restart.\n");
			}
		}

		resolved = true;
		return xuid;
	}

	inline PlayerUID ResolveXuidForPad(int iPad, bool isHost)
	{
		if(isHost && iPad == 0) return GetLegacyEmbeddedHostXuid();
		return XuidForPad(ResolvePersistentXuid(), iPad);
	}
}

#endif
