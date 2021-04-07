#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "threadpool.h"
#include "http_conn.h"

const int MAX_FD = 65536; // 最大的文件描述符个数
const int MAX_EVENT_NUMBER  = 10000; // 监听的最大事件数量

class WebServer{
public:
    WebServer(int port, int listenTrig, int connTrig, int m_IOHandlePattern);
    ~WebServer();
    
    void start(); 

private:
    int m_port;
    int m_listenFd;
    int m_epollFd;
    int m_listenTrig;
    int m_connTrig;
    int m_IOHandlePattern;
    threadpool<http_conn> * m_pool;
    http_conn * m_users;
    struct epoll_event m_events[MAX_EVENT_NUMBER];

private:
    void InitSocket();
    void DealListen();
    void DealClose(int sockfd);
    void DealRead(int sockfd);
    void DealWrite(int sockfd);
};

#endif