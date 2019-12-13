#include "LocalSock.h"
#include <stdlib.h>
#include <algorithm>
#include <fcntl.h>
#include <errno.h>

#define INIT_SOCK_NAME	"/tmp/initclient-sock"
#define Max_Open_Fd     1024
#define Max_Len         4096

CLocalSock	g_localSock;

CLocalSock::CLocalSock()
{
    m_bStop     = false;
    m_ptidp    = 0;
    m_fdEpoll  = 0;
    m_fdListen = 0;
    m_pInitDlg = NULL;
}

CLocalSock::~CLocalSock()
{
}

bool CLocalSock::Initialize(void* p)
{
    Clear();

    m_pInitDlg = p;

    return true;
}

void CLocalSock::Terminate()
{
    if (!m_bStop)
    {
        Stop();
    }
    if (m_fdEpoll != 0)
    {
        close(m_fdEpoll);
    }
    if (m_fdListen != 0)
    {
        close(m_fdListen);
    }
    
    m_pInitDlg = NULL;

    Clear();
}

bool CLocalSock::Start()
{
    if (!InitSock())
    {
        return false;
    }
    if (!CreateThread())
    {
        return false;
    }

    return true;
}

void CLocalSock::Stop()
{
    CloseThread();
}

void CLocalSock::Clear()
{
    m_bStop    = false;
    m_ptidp    = 0;
    m_fdEpoll  = 0;
    m_fdListen = 0;
    m_pInitDlg = NULL;
    m_listFd.clear();
}

bool CLocalSock::InitSock()
{
    unlink (INIT_SOCK_NAME);

    // 创建监听套接字
    m_fdListen = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fdListen == -1)
    {
        perror("listen socket create failed");
        return false;
    }
    // 设置监听端口复用，忽略错误
    int opt = 1;
    setsockopt(m_fdListen, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));

    // 初始化结构体
    struct sockaddr_un server_addr = {0};
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, INIT_SOCK_NAME);

    // 绑定套接字
    if (bind(m_fdListen, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("socket bind failed");
        close(m_fdListen);
        return false;
    }
    if (listen(m_fdListen, 1024) == -1)
    {
        perror("socket listen failed");
        close(m_fdListen);
        return false;
    }

    // 创建 epoll 套接字
    m_fdEpoll = epoll_create(Max_Open_Fd);
    if (m_fdEpoll == -1)
    {
        perror("create epoll failed");
        close(m_fdListen);
        return false;
    }

    // 将监听套接字加入到 epoll 的监控队列中
    struct epoll_event event_listen = {0};
    event_listen.events  = EPOLLIN | EPOLLHUP | EPOLLERR;
    event_listen.data.fd = m_fdListen;
    if (epoll_ctl(m_fdEpoll, EPOLL_CTL_ADD, m_fdListen, &event_listen) == -1)
    {
        perror("epoll_ctl failed");
        close(m_fdListen);
        close(m_fdEpoll);
        return false;
    }

    return true;
}

bool CLocalSock::CreateThread()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&m_ptidp, &attr, &SockThread, this) != 0)
    {
        Terminate();
        return false;
    }

    return true;
}

void CLocalSock::CloseThread()
{
    m_bStop = true;
    printf("close thread:%lu\n", m_ptidp);
    pthread_join(m_ptidp, NULL);
}

void CLocalSock::OnBusiness(char* data)
{
    if (data == NULL)
    {
        return;
    }

    char code[32] = { 0 };
    char* ps = NULL;
    char* pe = NULL;
    ps = strstr(data, "<app>");
    if (!ps)
    {
        return;
    }
    pe = strstr(data, "</app>");
    if (!pe)
    {
        return;
    }

    strncpy(code, (char*)(ps+5), (int)(pe-ps-5));
    int ret = atoi(code);
    if (ret == 1)
    {
        // 通知客户端改变用户
        //m_pDlg->UserChange();
    }
    printf("recv client message:%d\n", ret);
}

bool CLocalSock::AcceptSock()
{
    // accept 客户连接
    struct sockaddr_in client_addr = {0};
    socklen_t sockadd_len = sizeof(client_addr);
    int client_fd = accept(m_fdListen, (struct sockaddr*)&client_addr, &sockadd_len);
    if (client_fd == -1)
    {
        return false;
    }
    // 设置套接字为非阻塞方式
    if (fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK) == -1)
    {
        // 忽略错误
    }

    // 将新的套接字加入 epoll 监控队列
    struct epoll_event event_client = {0};
    event_client.events  = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLERR;
    event_client.data.fd = client_fd;
    if (epoll_ctl(m_fdEpoll, EPOLL_CTL_ADD, client_fd, &event_client) == -1)
    {
        close(client_fd);
        perror("epoll add client fd failed");
        return false;
    }
    m_listFd.push_back(client_fd);

    return true;
}

bool CLocalSock::CloseSock(int conn_fd)
{
    printf("client socket exit\n");

    epoll_ctl(m_fdEpoll, EPOLL_CTL_DEL, conn_fd, NULL);
    close(conn_fd);

    std::list<int>::iterator it = std::find(m_listFd.begin(), m_listFd.end(), conn_fd);
    if (it != m_listFd.end())
    {
        m_listFd.erase(it);
    }

    return true;
}

void CLocalSock::ClearEpoll()
{
    // 释放资源
    for (std::list<int>::iterator it = m_listFd.begin();
        it != m_listFd.end();
        ++it)
    {
        int fd = *it;
        epoll_ctl(m_fdEpoll, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
    }
    m_listFd.clear();
}

bool CLocalSock::RecvData(int fd)
{
    char buffer[Max_Len] = {0};
    // 读取客户发送的数据, 这里根据业务需要看是否循环读
    int bytes = read(fd, buffer, Max_Len);
    // 数据的长度是0，表示客户端主动关闭了套接字
    if ((bytes == 0))
    {
        if (!CloseSock(fd))
        {
            return false;
        }
    }
    else if (bytes > 0)
    {
        // 如果客户发送的数据大于 Max_Len, 被网络层分包的话，根据实际场景处理
        printf("client[%d] send data:%s\n", fd, buffer);
        
        OnBusiness(buffer);
    }
    else
    {
        // 读取错误
        if (errno != EAGAIN)
        {
            if (!CloseSock(fd))
            {
                return false;
            }
        }
    }

    return true;
}

void* CLocalSock::SockThread(void* param)
{
    CLocalSock* pThis = (CLocalSock*)param;
    if (pThis == NULL)
    {
        return 0;
    }

    struct epoll_event event_feed[Max_Open_Fd] = {0};

    // 死循环，一直等待客户的事件
    for (;;)
    {
        // 退出循环
        if (pThis->m_bStop)
        {
            break;
        }

        // 等待 epoll 事件
        size_t count_ready = epoll_wait(pThis->m_fdEpoll, event_feed, Max_Open_Fd, 100);
        if (count_ready == -1)
        {
            perror("epoll_wait failed");
            break;
        }
        
        // 处理 epoll 事件
        for (int i = 0; i < count_ready; ++i)
        {
            if (pThis->m_bStop)
            {
                break;
            }

            int conn_fd = event_feed[i].data.fd;
            // 套接字错误
            if ((event_feed[i].events & EPOLLERR) || (event_feed[i].events & EPOLLHUP))
            {
                // 监听套接字错误
                if (conn_fd == pThis->m_fdListen)
                {
                    perror("listen socket error, thread exit\n");
                    pThis->ClearEpoll();
                    return 0;
                }
                else
                {
                    pThis->CloseSock(conn_fd);
                    continue;
                }                
            }

            // 监控到的套接字是服务器的监听套接字，那么是有新的客户连接请求
            if (event_feed[i].data.fd == pThis->m_fdListen)
            {
                if (!pThis->AcceptSock())
                {
                    break;
                }
            }
            // 处理客户发来的请求
            else
            {
                pThis->RecvData(conn_fd);               
            }            
        }
        usleep(100);
    }

    pThis->ClearEpoll();
    printf("epoll thread exit\n");
    return 0;
}