#include "stdafx.h"
#include "TexturePack.h"

wstring TexturePack::getPath(bool bTitleUpdateTexture /*= false*/)
{
	wstring wDrive;

	if(bTitleUpdateTexture)
	{
		// Make the content package point to to the UPDATE: drive is needed
#ifdef _WINDOWS64
		wDrive=L"Media\\Resources\\TitleUpdate\\";
#else
		wDrive=L"Common\\Media\\Resources\\TitleUpdate\\";
#endif
	}
	else
	{
#ifdef _WINDOWS64
		wDrive=L"Media\\Resources\\Base\\";
#else
		wDrive=L"Common\\Media\\Resources\\Base\\";
#endif
	}

	return wDrive;
}
