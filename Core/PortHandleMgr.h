#pragma once
#include <windows.h>

struct IConnectPort
{
    virtual ~IConnectPort() {}
    virtual BOOL Create(LPCWSTR lpwszPortName) = 0;
    virtual BOOL Close() = 0;
    virtual HANDLE GetHandle() = 0;
};

struct ICommunicatePort
{
    virtual ~ICommunicatePort() {}
    virtual BOOL Connect(HANDLE hConnect) = 0;
    virtual BOOL SendData(LPCVOID lpData, DWORD dwDataSize) = 0;
    virtual BOOL PostData(LPCVOID lpData, DWORD dwDataSize) = 0;
    virtual BOOL DisConnect() = 0;
};

struct IMessage
{
    virtual ~IMessage() {}
    virtual ULONG GetMsgID() = 0;
    virtual DWORD GetMsgType() = 0;
	virtual BOOL IsAsyncHandle()=0;
};

struct IConnectHandler
{
    virtual ~IConnectHandler() {}
    virtual BOOL OnCreate(IMessage* pMsg) = 0;
    virtual BOOL OnClose(IMessage* pMsg) = 0;
};

struct ICommunicateHandler
{
    virtual ~ICommunicateHandler() {}
    virtual BOOL OnConnect(IMessage* pMsg) = 0;
    virtual BOOL OnDisConnect(IMessage* pMsg) = 0;
    virtual BOOL OnRecv(IMessage* pMsg) = 0;
    virtual BOOL OnSend(IMessage* pMsg) = 0;
};

struct IServerHandler
{
    virtual ~IServerHandler() {}

    // IConnectHandler
    virtual BOOL OnCreate(IConnectPort* pConnectPort, IMessage* pMsg) = 0;
    virtual BOOL OnClose(IConnectPort* pConnectPort, IMessage* pMsg) = 0;

    // ICommunicateHandler
    virtual BOOL OnConnect(ICommunicatePort* pCommunicatePort, IMessage* pMsg) = 0;
    virtual BOOL OnDisConnect(ICommunicatePort* pCommunicatePort, IMessage* pMsg) = 0;
    virtual BOOL OnRecv(ICommunicatePort* pCommunicatePort, IMessage* pMsg) = 0;
    virtual BOOL OnSend(ICommunicatePort* pCommunicatePort, IMessage* pMsg) = 0;
};



