#include "utility.h"
#define BUFFER_SIZE 1024
int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("Need server_ip and server_port to connect.");
        exit(1);
    }
    const char* server_ip = argv[1];
    int server_port = atoi(argv[2]);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("create socket failed");
        exit(1);
    }
    if( connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
    {
        perror("connection fialed");
        close(sockfd);
        exit(1);
    }
    char buf[BUFFER_SIZE];
    int buffer_count = 0;
    //创建管道
    int pipefd[2];
    if(pipe(pipefd) < 0)
    {
        perror("create pipe failed.");
        exit(1);
    }
    struct epoll_event events[2];
    int epollfd = epoll_create(EPOLL_SIZE);;
    if(epollfd < 0)
    {
        perror("epoll create failed.");
        exit(1);
    }

    addfd(epollfd, sockfd, true);
    addfd(epollfd, 0, true);//监听标准输入

    while(1)
    {
        int epoll_event_count = epoll_wait(epollfd, events, 2, -1);
        if(epoll_event_count < 0)
        {
            printf("epoll wait failed.");
            break;
        }
        //处理事件
        for(int i=0; i<epoll_event_count; i++)
        {
            memset(buf, '\0', BUFFER_SIZE);
            buffer_count = 0;
            if(events[i].data.fd == sockfd)
            {
                //服务端发来消息
                while(1)
                {
                    int ret = recv(sockfd, buf+buffer_count, BUFFER_SIZE, 0);
                    if(ret < 0)
                    {
                        //数据读取完毕
                        if(errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            printf("%s",buf);
                            break;
                        }
                    }
                    else if(ret == 0)
                    {
                        printf("The connection has closed by server, now exit.");
                        close(sockfd);
                        return 0;
                    }
                    else 
                    {
                        buffer_count += ret;
                    }
                }
            }
            else if( events[i].events & EPOLLIN)
            {
                fgets(buf, BUFFER_SIZE, stdin);

                if(send(sockfd, buf, BUFFER_SIZE, 0) < 0)
                {
                    perror("Send message error.");
                    exit(1);
                }

            }
        }
    }


}