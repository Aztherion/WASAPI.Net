#pragma once
#include <mutex>

namespace WASAPI_Net_WindowsPhone
{
	extern std::recursive_mutex g_apiLock;
}