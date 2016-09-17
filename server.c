#include "utility.h"

#define USER_LIMIT 20 //最大用户数量
#define BUFFER_SIZE 1024

struct user_info
{
    int user_id;
    struct user_info* next;
};

void add_user(struct user_info* user_list, int* user_count, int user_id)
{
    struct user_info* new_user = (struct user_info*)malloc(sizeof(struct user_info));
    new_user->user_id = user_id;
    new_user->next = user_list->next;
    user_list->next = new_user;
    (*user_count)++;
}
void remove_user(struct user_info* user_list, int* user_count, int user_id)
{
    struct user_info* p = user_list->next;
    struct user_info* q = user_list;
    while(p != NULL)
    {
        if(p->user_id == user_id)
        {
            break;
        }
        else
        {
            p = p->next;
            q = q->next;
        }
    }
    if( p != NULL)
    {
        q->next = p->next;
        free(p);
        (*user_count)--;
    }
}

int main(int argc,char* argv[])
{
    if(argc <= 2)
    {
        printf("%s,need server_ip and server_port!",argv[0]);
        return 1;
    }
    
    const char* server_ip = argv[1];
    int server_port = atoi(argv[2]);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    

    
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
    {
        perror("create listenfd error");
        exit(1);
    }

    int ret = bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret < 0)
    {
        perror("bind listenfd failed");
        exit(1);
    }
    ret = listen(listenfd,5);
    if(ret < 0)
    {
        perror("listen failed");
        exit(1);
    }
    printf("Start to listen.\n");

    struct epoll_event events[EPOLL_SIZE];
    int epollfd = epoll_create(EPOLL_SIZE);
    if(epollfd < 0)
    {
        perror("create epollfd failed.");
        exit(1);
    }
    addfd(epollfd, listenfd, true);

    //创建user链表头部
    struct user_info users;
    users.next = NULL;
    int user_count = 0;
    char buf[BUFFER_SIZE];
    int buffer_count = 0;
    while(1)
    {
        int epoll_event_count = epoll_wait(epollfd, events, EPOLL_SIZE, -1);
        if(epoll_event_count < 0)
        {
            printf("epoll wait failed\n");
            break;
        }
        //处理事件
        for( int i = 0; i<epoll_event_count; i++)
        {
            int sockfd = events[i].data.fd;
            //新用户连接到来
            if(sockfd == listenfd)
            {
                while(1)
                {
                    struct sockaddr_in client_addr;
                    socklen_t client_addrlength = sizeof(client_addr);
                    int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addrlength);
                    if(connfd < 0)
                    {
                        if(errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            //所有连接读取完毕,退出循环
                            break;
                        }
                        else
                        {
                            perror("Accept failed.");
                            exit(1);
                        }

                    }
                    addfd(epollfd, connfd, true);

                    printf("New client %d connect from %s:%d\n",connfd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    //用户数太多，关闭新的连接
                    if(user_count >= USER_LIMIT)
                    {
                        const char* info ="Too many users\n";
                        printf("%s",info);
                        send(connfd, info, strlen(info), 0);
                        close(connfd);
                        continue;
                    }
                    //保存新用户
                    add_user(&users, &user_count, connfd);
                    printf("New user %d coming in, now there have %d users in chatroom\n", connfd, user_count);
                }
                

            }
            else if(events[i].events & EPOLLIN)
            {
                memset(buf, '\0', BUFFER_SIZE);
                buffer_count = 0;
                printf("Client %d has message coming...\n",sockfd);
                //ET模式只触发一次，需要循环读取，确保将所有数据都读取出来
                while(1)
                {
                    int ret = recv(sockfd, buf+buffer_count, BUFFER_SIZE, 0);
                    if(ret < 0)
                    {
                        //对于非阻塞IO，下面的条件成立表示数据已经全部读取完毕
                        if(errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            //将该用户发来的消息发送给其他用户
                            char message[BUFFER_SIZE];
                            sprintf(message,"Client ID %d say >> %s",sockfd, buf);
                            struct user_info* p = users.next;
                            while(p != NULL)
                            {                                
                                if( p->user_id != sockfd)
                                {
                                    if(send(p->user_id, message, BUFFER_SIZE, 0)<0)
                                    {
                                        perror("Send message error.\n");
                                        exit(1);
                                    }
                                }
                                p = p->next;

                            } 
                            break;
                        }
                        else
                        {
                            //其他错误发生
                            printf("Some error happend for %d, now close it.\n",sockfd);
                            remove_user(&users, &user_count, sockfd);
                            close(sockfd);
                            break;
                        }
                    }
                    else if(ret == 0)
                    {
                        //客户端关闭连接,关闭的socket会自动从epoll监视事件中移除
                        printf("Client %d has exit.\n",sockfd);
                        remove_user(&users, &user_count, sockfd);
                        close(sockfd);
                        break;                        
                    }
                    else
                    {
                        buffer_count += ret;//接收字节数增加
                    }
                }
            }
        }
    }

}
