// ComDemo.cpp : �������̨Ӧ�ó������ڵ㡣
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

