#include "webserver.h"

extern void addfd( int epollfd, int fd, bool one_shot, int trigMode );
extern void removefd( int epollfd, int fd );

WebServer::WebServer(int port, int listenTrig, int connTrig, int IOHandlePattern) : 
m_port(port), m_listenTrig(listenTrig), m_connTrig(connTrig), m_IOHandlePattern(IOHandlePattern),
m_pool(new threadpool<http_conn>(IOHandlePattern)), 
m_users(new http_conn[MAX_FD]), m_epollFd(epoll_create(5)){
    InitSocket();
}

WebServer::~WebServer(){
    close(m_epollFd);
    close(m_listenFd);
    delete [] m_users;
    delete m_pool;
}

void WebServer::InitSocket(){
    m_listenFd= socket( PF_INET, SOCK_STREAM, 0 );
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons( m_port );
    // 端口复用
    int reuse = 1;
    setsockopt( m_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    bind( m_listenFd, ( struct sockaddr* )&address, sizeof( address ) );
    listen( m_listenFd, 5 );
    addfd( m_epollFd, m_listenFd, false, m_listenTrig );
    http_conn::m_epollfd = m_epollFd;
}

void WebServer::start(){
    while(true){
        int num = epoll_wait( m_epollFd, m_events, MAX_EVENT_NUMBER, -1 );
        if ( ( num < 0 ) && ( errno != EINTR ) ) {
            printf( "epoll failure\n" );
            break;
        }
        for(int i=0; i<num; ++i){
            int sockfd = m_events[i].data.fd;
            if(sockfd == m_listenFd){
                DealListen();
            }else if(m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                DealClose(sockfd);
            }else if(m_events[i].events & EPOLLIN){ // 主线程监听到读事件
                DealRead(sockfd);
            }else if(m_events[i].events & EPOLLOUT){ // 主线程监听到写事件
                DealWrite(sockfd);
            }
        }
    }
}

void WebServer::DealListen(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof( client_address );
    if(m_listenTrig == 0){
        int connfd = accept( m_listenFd, ( struct sockaddr* )&client_address, &client_addrlength );
                
        if ( connfd < 0 ) {
            printf( "(listen LT connfd accept) errno is: %d\n", errno );
            return;
        } 

        if( http_conn::m_user_count >= MAX_FD ) {
            close(connfd);
            return;
        }
        // 初始化新连接的http_conn对象
        m_users[connfd].init( connfd, client_address, m_connTrig );

    }else{
        // 如果监听socket是ET模式，需要循环检测
        while(1){
            int connfd = accept( m_listenFd, ( struct sockaddr* )&client_address, &client_addrlength );              
            if ( connfd < 0 ) {
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    break;
                }
                printf( "(listen ET connfd accept) errno is: %d\n", errno );
                continue;
            } 
            if( http_conn::m_user_count >= MAX_FD ) {
                close(connfd);
                continue;
            }
            // 初始化新连接的http_conn对象
            m_users[connfd].init( connfd, client_address, m_connTrig );
        }
    }
}

void WebServer::DealClose(int sockfd){
    m_users[sockfd].close_conn();
}

void WebServer::DealRead(int sockfd){
    if(m_IOHandlePattern == 0){ // proactor
        if(m_users[sockfd].read()) { // 主线程读
            m_pool->append_p(m_users + sockfd); // 子线程只负责处理逻辑
        } else {
            m_users[sockfd].close_conn();
        }
    }else{ // reactor
        m_pool->append_r(m_users+sockfd, 0); // 子线程读 + 处理逻辑
    }
}

void WebServer::DealWrite(int sockfd){
    if(m_IOHandlePattern == 0){ // proactor
        if( !m_users[sockfd].write() ) { // 主线程读
            m_users[sockfd].close_conn();
        }
    }else{ // reactor
        m_pool->append_r(m_users+sockfd, 1); // 子线程写              
    }
}