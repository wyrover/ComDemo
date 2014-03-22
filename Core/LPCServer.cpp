#include "stdafx.h"
#include "LPCServer.h"
#include "PortHandleMgr.h"

CLPCServerConnectPort::CLPCServerConnectPort(IServerHandler* pServer)
    : m_hConnectPort(NULL)
    , m_pServer(pServer)
{
}

CLPCServerConnectPort::~CLPCServerConnectPort()
{
}

BOOL CLPCServerConnectPort::Create(LPCWSTR lpwszPortName)
{
    SECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES ObjAttr;
    UNICODE_STRING PortName;
    NTSTATUS Status;

    if(!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
        return FALSE;

    if(!SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
        return FALSE;

    RtlInitUnicodeString(&PortName, lpwszPortName);
    InitializeObjectAttributes(&ObjAttr, &PortName, 0, NULL, &sd);
    Status = NtCreatePort(&m_hConnectPort, &ObjAttr, NULL, sizeof(PORT_MESSAGE) + MAX_LPC_DATA, 0);

    if(!NT_SUCCESS(Status))
        return FALSE;

    return OnCreate(NULL);
}

BOOL CLPCServerConnectPort::Close()
{
    if(NULL != m_hConnectPort)
    {
        NtClose(m_hConnectPort);
        m_hConnectPort = NULL;
        CLPCMessage lpcMsg(0, 1);
        return OnClose(&lpcMsg);
    }

    return FALSE;
}

HANDLE CLPCServerConnectPort::GetHandle()
{
    return m_hConnectPort;
}

BOOL CLPCServerConnectPort::OnCreate(IMessage* pMsg)
{
    // 进行一些必要的处理

    // 转发给用户处理
    if(NULL != m_pServer)
        return m_pServer->OnCreate(this, pMsg);

    return TRUE;
}

BOOL CLPCServerConnectPort::OnClose(IMessage* pMsg)
{
    //

    if(NULL != m_pServer)
        return m_pServer->OnClose(this, pMsg);

    return FALSE;
}

CLPCServerCommunicatPort::CLPCServerCommunicatPort(IServerHandler* pServer)
    : m_hCommunicatePort(NULL)
    , m_pServer(pServer)
    , m_hEventMsgSend(NULL)
    , m_hEventMsgRecv(NULL)
    , m_bConnect(FALSE)
{
}

CLPCServerCommunicatPort::~CLPCServerCommunicatPort()
{
}

BOOL CLPCServerCommunicatPort::Connect(HANDLE hConnect)
{
    // 连接端口
    CLPCMessage lpcMessage;
    NTSTATUS ntStatus = NtListenPort(hConnect, &lpcMessage.Header);

    if(!NT_SUCCESS(ntStatus))
        return FALSE;

    ntStatus = NtAcceptConnectPort(&m_hCommunicatePort, NULL, &lpcMessage.Header, TRUE, NULL, NULL);

    if(!NT_SUCCESS(ntStatus))
        return FALSE;

    ntStatus = NtCompleteConnectPort(m_hCommunicatePort);

    if(!NT_SUCCESS(ntStatus))
        return FALSE;

    if(NULL != m_hEventMsgRecv)
        CloseHandle(m_hEventMsgRecv);

    m_hEventMsgRecv = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(NULL == m_hEventMsgRecv)
        return FALSE;

    if(NULL != m_hEventMsgSend)
        CloseHandle(m_hEventMsgSend);

    m_hEventMsgSend = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(NULL == m_hEventMsgSend)
        return FALSE;

    if(!OnConnect(NULL))
        return FALSE;

    m_bConnect = TRUE;
    // 开启数据发送线程
    HANDLE hSendThread = CreateThread(NULL, 0, _SendDataThread, this, 0, NULL);
    // 开启数据接收线程
    HANDLE hRecvThread = CreateThread(NULL, 0, _RecvDataThread, this, 0, NULL);
    // 开启数据处理线程
    HANDLE hDataHandleThread = CreateThread(NULL, 0, _HandleDataThread, this, 0, NULL);

    WaitForSingleObject(hRecvThread, INFINITE);
    WaitForSingleObject(hDataHandleThread, INFINITE);

    return (NULL != hSendThread && NULL != hRecvThread && NULL != hDataHandleThread);
}

BOOL CLPCServerCommunicatPort::DisConnect()
{
    m_bConnect = FALSE;

    if(NULL != m_hEventMsgSend)
    {
        SetEvent(m_hEventMsgSend);
        WaitForSingleObject(m_hEventMsgSend, INFINITE);
        CloseHandle(m_hEventMsgSend);
    }

    if(NULL != m_hEventMsgRecv)
    {
        SetEvent(m_hEventMsgRecv);
        WaitForSingleObject(m_hEventMsgRecv, INFINITE);
        CloseHandle(m_hEventMsgRecv);
    }

    // 清除发送队列中的所有数据
    while(!m_msgSendQueue.empty())
    {
        IMessage* pMsg = m_msgSendQueue.front();
        m_msgSendQueue.pop();

        if(NULL == pMsg)
            continue;

        delete pMsg;
        pMsg = NULL;
    }

    // 清除接收队列中的所有数据
    while(!m_msgRecvQueue.empty())
    {
        IMessage* pMsg = m_msgRecvQueue.front();
        m_msgRecvQueue.pop();

        if(NULL == pMsg)
            continue;

        delete pMsg;
        pMsg = NULL;
    }

    // 关闭通信端口
    NtClose(m_hCommunicatePort);
    m_hCommunicatePort = NULL;

    OnDisConnect(NULL);
    return TRUE;
}

BOOL CLPCServerCommunicatPort::SendData(LPCVOID lpData, DWORD dwDataSize)
{
    CLPCMessage msg;

    if(msg.IsAsyncHandle())
    {
        return PostData(lpData, dwDataSize);
    }

    NTSTATUS ntStatus = NtReplyPort(m_hCommunicatePort, &msg.Header);
    OnSend(&msg);
    return NT_SUCCESS(ntStatus);
}

BOOL CLPCServerCommunicatPort::PostData(LPCVOID lpData, DWORD dwDataSize)
{
    CLPCMessage msg;
    m_msgSendQueue.push(new CLPCMessage(msg));
    SetEvent(m_hEventMsgSend);
    return TRUE;
}

DWORD __stdcall CLPCServerCommunicatPort::_HandleDataThread(LPVOID lpParam)
{
    CLPCServerCommunicatPort* pPort = (CLPCServerCommunicatPort*)lpParam;

    if(NULL == pPort)
        return FALSE;

    // 将接收队列中的消息进行出队处理
    return pPort->_HandleData();
}

DWORD __stdcall CLPCServerCommunicatPort::_SendDataThread(LPVOID lpParam)
{
    CLPCServerCommunicatPort* pPort = (CLPCServerCommunicatPort*)lpParam;

    if(NULL == pPort)
        return FALSE;

    // 将发送队列中的消息进行队列处理
    return pPort->_SendData();
}

DWORD __stdcall CLPCServerCommunicatPort::_RecvDataThread(LPVOID lpParam)
{
    CLPCServerCommunicatPort* pPort = (CLPCServerCommunicatPort*)lpParam;

    if(NULL == pPort)
        return FALSE;

    // 接收数据并投递到数据处理线程
    return pPort->_ReceData();
}

BOOL CLPCServerCommunicatPort::_ReceData()
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    while(m_bConnect)
    {
        // 接收消息
        CLPCMessage lpcMessage;
        ntStatus = NtReplyWaitReceivePort(m_hCommunicatePort, NULL, NULL, &lpcMessage.Header);

        if(!NT_SUCCESS(ntStatus))
            continue;

        // 发生错误就退出
        if(LPC_CLIENT_DIED == lpcMessage.Header.u2.s2.Type ||
                LPC_PORT_CLOSED == lpcMessage.Header.u2.s2.Type
          )
        {
            // 断开当前通讯端口
            DisConnect();
            return FALSE;
        }

        if(LPC_REQUEST == lpcMessage.Header.u2.s2.Type)
            ntStatus = NtReplyPort(m_hCommunicatePort, &lpcMessage.Header);

        if(!NT_SUCCESS(ntStatus))
            continue;

        // 如果是异步处理,就投递到消息接收队列
        if(lpcMessage.IsAsyncHandle())
        {
            // 将接收到的数据投递到_DataHandleThread进行队列处理
            m_msgRecvQueue.push(new CLPCMessage(lpcMessage));
            SetEvent(m_hEventMsgRecv);
        }
        else
        {
            if(!OnRecv(NULL))
                continue;
        }
    }

    return TRUE;
}

BOOL CLPCServerCommunicatPort::_SendData()
{
    while(m_bConnect)
    {
        if(m_msgSendQueue.empty()) continue;

        WaitForSingleObject(m_hEventMsgSend, INFINITE);

        IMessage* pMsg = m_msgSendQueue.front();
        m_msgSendQueue.pop();

        if(NULL == pMsg)
            continue;

        OnSend(pMsg);

        // 处理消息
        delete pMsg;
        pMsg = NULL;
    }

    return FALSE;
}

BOOL CLPCServerCommunicatPort::_HandleData()
{
    while(m_bConnect)
    {
        if(m_msgRecvQueue.empty()) continue;

        WaitForSingleObject(m_hEventMsgRecv, INFINITE);

        IMessage* pMsg = m_msgRecvQueue.front();
        m_msgRecvQueue.pop();

        if(NULL == pMsg)
            continue;

        OnRecv(pMsg);

        // 处理消息
        delete pMsg;
        pMsg = NULL;
    }

    return FALSE;
}

BOOL CLPCServerCommunicatPort::OnConnect(IMessage* pMsg)
{
    if(NULL != m_pServer)
        return m_pServer->OnConnect(this, pMsg);

    return FALSE;
}

BOOL CLPCServerCommunicatPort::OnDisConnect(IMessage* pMsg)
{
    if(NULL != m_pServer)
        return m_pServer->OnDisConnect(this, pMsg);

    return FALSE;
}

BOOL CLPCServerCommunicatPort::OnRecv(IMessage* pMsg)
{
    if(NULL != m_pServer)
        return m_pServer->OnRecv(this, pMsg);

    return FALSE;
}

BOOL CLPCServerCommunicatPort::OnSend(IMessage* pMsg)
{
    if(NULL != m_pServer)
        return m_pServer->OnSend(this, pMsg);

    return FALSE;
}

CLPCServer::CLPCServer(): m_pConnect(NULL)
{
    m_pConnect = new CLPCServerConnectPort(this);
}


CLPCServer::~CLPCServer(void)
{
    if(NULL != m_pConnect)
    {
        delete m_pConnect;
        m_pConnect = NULL;
    }
}

BOOL CLPCServer::CreatePort(LPCWSTR lpwszPortName)
{
    if(NULL == m_pConnect)
        return FALSE;

    if(m_pConnect->Create(lpwszPortName))
    {
        return TRUE;
    }

    return FALSE;
}

void CLPCServer::ClosePort()
{
    CommunicateMap::const_iterator cit;

    for(cit = m_CommunicateMap.begin(); cit != m_CommunicateMap.end(); cit++)
    {
        ICommunicatePort* pCommunicatePort = (ICommunicatePort*)cit->second;

        if(NULL == pCommunicatePort)
            continue;

        pCommunicatePort->DisConnect();
    }

    if(NULL != m_pConnect)
    {
        m_pConnect->Close();
    }
}

BOOL CLPCServer::Listen()
{
    BOOL bExit = FALSE;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    while(FALSE == bExit)
    {
        ICommunicatePort* pCommunicatePort = new CLPCServerCommunicatPort(this);

        if(NULL == pCommunicatePort)
            continue;

        if(!pCommunicatePort->Connect(m_pConnect->GetHandle()))
        {
            delete pCommunicatePort;
            pCommunicatePort = NULL;
            continue;
        }
    }

    return FALSE;
}

BOOL CLPCServer::OnCreate(IConnectPort* pConnectPort, IMessage* pMsg)
{
    return TRUE;
}

BOOL CLPCServer::OnClose(IConnectPort* pConnectPort, IMessage* pMsg)
{
    return TRUE;
}

BOOL CLPCServer::OnConnect(ICommunicatePort* pCommunicatePort, IMessage* pMsg)
{
    m_CommunicateMap.insert(std::make_pair(rand(), pCommunicatePort));
    return TRUE;
}

BOOL CLPCServer::OnDisConnect(ICommunicatePort* pCommunicatePort, IMessage* pMsg)
{
//     CommunicateMap::const_iterator cit = std::find(m_CommunicateMap.begin(), m_CommunicateMap.end(), pCommunicatePort);
//
//     if(cit != m_CommunicateMap.end())
//     {
//         delete cit->second;
//         (ICommunicatePort*)cit->second = NULL;
//         m_CommunicateMap.erase(cit);
//     }

    return TRUE;
}

BOOL CLPCServer::OnRecv(ICommunicatePort* pCommunicatePort, IMessage* pMsg)
{
    return TRUE;
}

BOOL CLPCServer::OnSend(ICommunicatePort* pCommunicatePort, IMessage* pMsg)
{
    return TRUE;
}

CLPCMessage::CLPCMessage(ULONG ulMsgID, DWORD dwMsgType)
{
    InitializeMessageHeader(&Header, sizeof(CLPCMessage), 0);
    m_MsgID = ulMsgID;
    m_MsgType = dwMsgType;
}

CLPCMessage::CLPCMessage(const CLPCMessage& rCm)
{

}

CLPCMessage::~CLPCMessage()
{
}

ULONG CLPCMessage::GetMsgID()
{
    return m_MsgID;
}

DWORD CLPCMessage::GetMsgType()
{
    return m_MsgType;
}

// 是否异步处理
BOOL CLPCMessage::IsAsyncHandle()
{
    return TRUE;
}
