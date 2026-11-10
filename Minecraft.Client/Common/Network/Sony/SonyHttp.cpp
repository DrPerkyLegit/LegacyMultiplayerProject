#include "stdafx.h"
#include "SonyHttp.h"





bool SonyHttp::init()
{
	return g_SonyHttp.init();
}

void SonyHttp::shutdown()
{
	g_SonyHttp.shutdown();
}

bool SonyHttp::getDataFromURL(const char* szURL, void** ppOutData, int* pDataSize)
{
	return g_SonyHttp.getDataFromURL(szURL, ppOutData, pDataSize);
}
