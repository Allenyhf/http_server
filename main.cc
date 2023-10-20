#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "task.hpp"
#include "threadpool.h"
#include "my_epoll.hpp"

#define ERR_EXIT(msg) (perror(msg), exit(EXIT_FAILURE))
#define BACKLOG     10000
#define MAX_EVENTS  10000
#define MAX_FD   65536

int epollfd = 0;
Task tasks[MAX_FD];
int task_nr = 0;

int Set_fd_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    return fcntl(fd, F_SETFL, flags|O_NONBLOCK);
}


// ./server ip port
int main(int argc, char** argv) {
    if (argc<3) {
        ERR_EXIT("Usage: ./server ip  port\n");
    }
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd<0) {
        ERR_EXIT("create socket error!\n");
    }
    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if(0==inet_aton(argv[1], &server_addr.sin_addr)) {
        ERR_EXIT("invalid address!\n");
    }    
    int enable = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&enable, sizeof(enable));
    if (-1==bind(listenfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))) {
        ERR_EXIT(strerror(errno));
    }
    if (-1==listen(listenfd, BACKLOG)) {
        ERR_EXIT("listen failed!\n");
    }

    socklen_t cli_addr_len = sizeof(struct sockaddr);
    if (Set_fd_nonblock(listenfd)==-1) {
        ERR_EXIT("fail to set fd nonblock!\n");
    }
    epollfd = epoll_create(4096);
    if (epollfd==-1) {
        ERR_EXIT(strerror(errno));   
    }
    epoll_event ev, events[MAX_EVENTS];
    memset(&ev, 0, sizeof(struct epoll_event));
    // ev.events = EPOLLIN; // leverl trigger
    // ev.data.fd = listenfd;
    memset(events, 0, sizeof(events));
    if (-1==Epoll_add_in(listenfd, 0)) {
        ERR_EXIT("fail to epoll_ctl_add listenfd!\n");
    }
    ThreadPool<Task> threadpool(16);
    threadpool.Init();
    printf("http server start running...\n");
    while (1) {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds==-1) {
            ERR_EXIT(strerror(errno));
        }
        for (int k=0; k<nfds; k++) {
            int sockfd = events[k].data.fd;
            if (events[k].events & EPOLLIN) {
                if (sockfd==listenfd) {
                    int cli_fd = accept(listenfd, (struct sockaddr*)&client_addr, &cli_addr_len);
                    if (cli_fd<0) {
                        ERR_EXIT("fail to accept client!\n");
                    }
                    if (cli_fd >= MAX_EVENTS) {
                        printf("not more events[]!\n");
                    }
                    // printf("accep one client!\n");
                    
                    if (Set_fd_nonblock(cli_fd)==-1) {
                        printf("fail to set client fd nonblock!\n");
                        continue;
                    }
                    if (-1==Epoll_add_in(cli_fd, 1)) {
                        printf("fail to add client fd IN!\n");
                        continue;
                    }
                    tasks[cli_fd].init(cli_fd);
                } else {
                    // printf("sockfd: %d can be read!\n", sockfd);
                    if (sockfd>0 && sockfd<MAX_EVENTS) {
                        threadpool.addTask(&tasks[sockfd]);
                    } else {
                        //log
                        // printf("sockfd isn't valid!\n");
                    }
                }
            } else if (events[k].events & EPOLLOUT) {
                if (sockfd<0||sockfd>10000) continue;
                if (sockfd!=listenfd) {
                    // sleep(5);
                    Task& t = tasks[sockfd];
                    int wr = t.write();
                    if (wr==-1) {
                        if (-1==Epoll_del_fd(sockfd)) {
                            printf("fail to del events OUT!\n");
                        }
                        close(sockfd);
                    } else if (wr==0) {
                        if (-1==Epoll_mod_out(sockfd)) {
                            printf("fail to add client_fd OUT: %s!\n", strerror(errno));
                        }
                    } else if (wr==1) {
                        if (-1==Epoll_del_fd(sockfd)) {
                            printf("fail to del events OUT!\n");
                        }
                        close(sockfd);
                    }
                    // t.write();
                    // if (-1==Epoll_del_fd(sockfd)) {
                    //     printf("fail to del events OUT!\n");
                    // }
                    // close(sockfd);
                }
            } else if (events[k].events & (EPOLLERR|EPOLLHUP|EPOLLRDHUP)) {
                //log
                if (sockfd>0 && sockfd<MAX_FD){
                    if (-1==Epoll_del_fd(sockfd)) {
                        printf("fail to del events OUT!\n");
                    }
                    close(sockfd);
                } 
            }
        }
    }
    // { // test
    //     int cli_fd = accept(listenfd, (struct sockaddr*)&client_addr, &cli_addr_len);
    //     if (cli_fd<0) {
    //         ERR_EXIT("fail to accept client!\n");
    //     }
    //     char buf[1024];
    //     memset(buf, 0, sizeof(buf));
    //     int nr = recv(cli_fd, buf, sizeof(buf), 0);
    //     if (nr==-1) {
    //         ERR_EXIT("fail to recv from client!\n");
    //     } else {
    //         printf("recv %d bytes: %s", nr, buf);
    //         // printf("%d %d\n", buf[nr-2], buf[nr-1]);
    //     }
    //     int ns = send(cli_fd, buf, nr, 0);
    //     if (ns==-1) {
    //         ERR_EXIT("fail to send to client!\n");
    //     } else {
    //         printf("send success: %d bytes!", ns);
    //     }
    // }

    close(listenfd);
    close(epollfd);
    return 0;
}