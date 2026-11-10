#include "stdafx.h"
#include "Language.h"
#include "I18n.h"

Language *I18n::lang = Language::getInstance();
wstring I18n::get(const wstring& id, ...)
{
#if _MSC_VER >= 1930
	return id;
#else
	va_list va;
	va_start(va, id);
    return I18n::get(id, va);
#endif
}

wstring I18n::get(const wstring& id, va_list args)
{
	return lang->getElement(id, args);
}
