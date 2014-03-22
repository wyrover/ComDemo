// ComDemo.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "LPCServer.h"

int _tmain(int argc, _TCHAR* argv[])
{
	WCHAR* sPortName=L"\\LPCServerPort";
	CLPCServer server;
	if (server.CreatePort(sPortName))
	{
		if (server.Listen())
		{

		}
		server.ClosePort();
	}
	return 0;
}

