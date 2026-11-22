#pragma once

#include <windows.h>

inline bool EnsureWindows64GameHddDirectory()
{
	const wchar_t* directories[] = { L"Windows64", L"Windows64\\GameHDD" };
	for (size_t index = 0; index < _countof(directories); ++index)
	{
		DWORD attributes = GetFileAttributesW(directories[index]);
		if (attributes != INVALID_FILE_ATTRIBUTES)
		{
			if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return false;
			continue;
		}

		if (!CreateDirectoryW(directories[index], NULL))
		{
			if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
			attributes = GetFileAttributesW(directories[index]);
			if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return false;
		}
	}

	return true;
}