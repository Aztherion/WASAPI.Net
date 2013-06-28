#include "pch.h"
#include "ApiLock.h"

namespace WASAPI_Net_WindowsPhone
{
	std::recursive_mutex g_apiLock;
}