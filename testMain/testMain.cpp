
#include <string.h>
#define __VER__ (strrchr(__FILE__, '_') ? strrchr(__FILE__, '_') + 1 : __FILE__)
#define _USE_LOGGER_


#ifdef _DEBUG
#pragma comment(lib, "crashDumpD")

#else
#pragma comment(lib, "crashDump")
#endif

#include "networkServer.h"
#include "networkClient.h"
#include "localServer.h"

#include "crashDump/crashDump/crashDump.h"





void printDay(UINT64 tick)
{
	int ms = tick % 1000;
	tick /= 1000;
	int sec = tick % 60;
	tick /= 60;
	int min = tick % 60;
	tick /= 60;
	int hour = tick % 60;
	tick /= 24;
	UINT64 days = tick;

	printf("[%2llu day...  %02d:%02d:%02d.%03d]\n", days, hour, min, sec, ms);
}


void main()
{
	CCrashDump cd;
	//loggerInit("errorCode");
	printf("hi %s logging ver\n", __VER__);

	//NetworkServer c;
	LocalServer c;
	if (!c.start(6000, 256, 4))
		return;

	UINT64 startTime = GetTickCount64();
	LOG(logLevel::Info, LO_TXT | LO_CMD, string("hi ") + string(__VER__));

	while (true) {
		Sleep(2000);
		UINT64 currentTime = GetTickCount64() - startTime;
		printf("hi %s logging ver\n", __VER__);
		printDay(currentTime);
		printf("\taccept TPS\t: %4d\n", c.getAcceptTPS());
		printf("\tsend TPS\t: %4d\n", c.getSendMessageTPS());
		printf("\trecv TPS\t: %4d\n", c.getRecvMessageTPS());
		auto p = c.getSessionPoolMemory();
		printf("\tsessionPool\t: %4zu / %4zu\t %d\n", p.second, p.first, c.getSessionCount());
		p = packetPoolMemoryCheck();
		printf("\tpacketPool\t: %4zu / %4zu\n\n", p.second, p.first);
		
		//LOGOUT(logLevel::Info, LO_TXT) << "\t" << c.getSendMessageTPS() << "\t" << c.getRecvMessageTPS() << LOGEND;
	}

	c.stop();
}


