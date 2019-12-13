#include "LocalSock.h"

int main()
{
    g_localSock.Initialize(NULL);
    g_localSock.Start();

    int count = 60;
    while(count-- > 0)
    {
        sleep(1);
    }

    g_localSock.Stop();
    g_localSock.Terminate();

    printf("server main process exit\n");

    return true;
}

// 编译指令
// g++ LocalSock.h LocalSock.cpp main.cpp -o server -lpthread