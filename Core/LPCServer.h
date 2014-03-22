#pragma once
#include <windows.h>
#include <map>
#include "PortHandleMgr.h"
#include <queue>
#include "ntdll.h"


class CLPCServerConnectPort : public IConnectPort, public IConnectHandler
{
public:
    CLPCServerConnectPort(IServerHandler* pServer);
    virtual ~CLPCServerConnectPort();
public:
    BOOL Create(LPCWSTR lpwszPortName);
    BOOL Close();
    HANDLE GetHandle();
public:
    BOOL OnCreate(IMessage* pMsg);
    BOOL OnClose(IMessage* pMsg);
private:
    HANDLE m_hConnectPort;
    IServerHandler* m_pServer;
};

class CLPCServerCommunicatPort : public ICommunicatePort, public ICommunicateHandler
{
public:
    CLPCServerCommunicatPort(IServerHandler* pServer);
    virtual ~CLPCServerCommunicatPort();
public:
    BOOL Connect(HANDLE hConnect);
    BOOL SendData(LPCVOID lpData, DWORD dwDataSize);
    BOOL PostData(LPCVOID lpData, DWORD dwDataSize);
    BOOL DisConnect();
public:
    BOOL OnConnect(IMessage* pMsg);
    BOOL OnDisConnect(IMessage* pMsg);
    BOOL OnRecv(IMessage* pMsg);
    BOOL OnSend(IMessage* pMsg);
protected:
    static DWORD __stdcall _SendDataThread(LPVOID lpParam);
    static DWORD __stdcall _RecvDataThread(LPVOID lpParam);
    static DWORD __stdcall _HandleDataThread(LPVOID lpParam);
    BOOL _ReceData();
    BOOL _SendData();
    BOOL _HandleData();
private:
    typedef std::queue<IMessage*> MsgQueue;
    MsgQueue m_msgSendQueue;
    MsgQueue m_msgRecvQueue;
	HANDLE m_hEventMsgSend;
	HANDLE m_hEventMsgRecv;
    HANDLE m_hCommunicatePort;
    IServerHandler* m_pServer;
	BOOL m_bConnect;
};

class CLPCMessage : public IMessage
{
public:
    CLPCMessage(ULONG ulMsgID=0, DWORD dwMsgType=0);
	CLPCMessage(const CLPCMessage& rCm);
    virtual ~CLPCMessage();
    ULONG GetMsgID();
    DWORD GetMsgType();
	BOOL IsAsyncHandle();
public:
//	friend class CLPCServer;
	friend class CLPCServerConnectPort;
	friend class CLPCServerCommunicatPort;
private:
	PORT_MESSAGE Header;
    ULONG m_MsgID;
    DWORD m_MsgType;
};

// typedef struct _TRANSFERRED_MESSAGE
// {
//     _TRANSFERRED_MESSAGE()
//     {
//         InitializeMessageHeader(&Header, sizeof(_TRANSFERRED_MESSAGE), 0);
//     }
//     PORT_MESSAGE Header;
//     ULONG   MsgID;
//     DWORD   MsgType;
//     WCHAR   MessageText[48];
// } TRANSFERRED_MESSAGE, *PTRANSFERRED_MESSAGE;

class CLPCServer : public IServerHandler
{
public:
    CLPCServer();
    virtual ~CLPCServer(void);

    BOOL CreatePort(LPCWSTR lpwszPortName);
    BOOL Listen();
    void ClosePort();
public:
    // IConnectHandler
    BOOL OnCreate(IConnectPort* pConnectPort, IMessage* pMsg);
    BOOL OnClose(IConnectPort* pConnectPort, IMessage* pMsg);

    // ICommunicateHandler
    BOOL OnConnect(ICommunicatePort* pCommunicatePort, IMessage* pMsg);
    BOOL OnDisConnect(ICommunicatePort* pCommunicatePort, IMessage* pMsg);
    BOOL OnRecv(ICommunicatePort* pCommunicatePort, IMessage* pMsg);
    BOOL OnSend(ICommunicatePort* pCommunicatePort, IMessage* pMsg);
private:
    IConnectPort* m_pConnect;
    typedef std::map<DWORD, ICommunicatePort*> CommunicateMap;
    CommunicateMap m_CommunicateMap;
};

