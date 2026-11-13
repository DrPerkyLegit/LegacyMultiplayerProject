#include "stdafx.h"
#include "TexturePack.h"

wstring TexturePack::getPath(bool bTitleUpdateTexture /*= false*/)
{
	wstring wDrive;

	if(bTitleUpdateTexture)
	{
		// Make the content package point to to the UPDATE: drive is needed
		wDrive=L"Common\\Media\\Resources\\TitleUpdate\\";
	}
	else
	{
		wDrive=L"Common\\Media\\Resources\\Base\\";
	}

	return wDrive;
}
