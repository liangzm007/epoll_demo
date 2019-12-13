#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <list>

class CLocalSock
{
public:
    CLocalSock();
    ~CLocalSock();

    bool Initialize(void*);
    void Terminate();

    bool Start();
    void Stop();

protected:
    void Clear();

    // 初始化监听套接字
    bool InitSock();

    // 创建 epoll 处理线程
    bool CreateThread();

    // 关闭 epoll 处理线程
    void CloseThread();

    // 处理客户发送的请求
    void OnBusiness(char*);

    // 处理客户端连接的请求
    bool AcceptSock();

    // 处理客户端断开连接的请求
    bool CloseSock(int fd);

    // 清理 epoll 资源
    void ClearEpoll();

    // 接收来自客户端的消息
    bool RecvData(int fd);

    // epoll 处理线程
    static void* SockThread(void* pParam);

protected:
    bool  m_bStop;                  // 线程停止标识
    int   m_fdListen;               // 监听套接字
    int   m_fdEpoll;                // epoll 套接字
    void* m_pInitDlg;               // 外部指针

    pthread_t m_ptidp;              // 线程 id 标识
    std::list<int> m_listFd;        // 客户套接字集合
};

extern CLocalSock g_localSock;