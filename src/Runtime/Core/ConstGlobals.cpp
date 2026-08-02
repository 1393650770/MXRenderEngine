#include "ConstGlobals.h"
#include "ConstDefine.h"

std::atomic<UInt64>	g_frame_number_render_thread{ 0 };
CONST UInt64 g_max_frame_number = 18446744073709551614;

//  默认单线程模式
EThreadingMode g_thread_mode = EThreadingMode::Single;
