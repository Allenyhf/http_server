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

#define BACKLOG 10000
#define ERR_EXIT(msg) (perror(msg), exit(EXIT_FAILURE))
#define MAX_EVENTS 10000

int epollfd = 0;
Task tasks[10000];
int task_nr = 0;

int Set_fd_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    return fcntl(fd, F_SETFL, flags|O_NONBLOCK);
}

int Epoll_add_in(int fd, int one_shot) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN; // leverl trigger
    if (one_shot) ev.events |= EPOLLONESHOT;
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int Epoll_add_out(int fd, void* ptr, int one_shot) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLOUT; // leverl trigger
    if (one_shot) ev.events |= EPOLLONESHOT;
    ev.data.fd = fd;
    ev.data.ptr = ptr;
    printf("OUT fd: %d\n", fd);
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int Epoll_add_io(int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN|EPOLLOUT; // leverl trigger
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

// int Epoll_mod_out(int fd) {
//     struct epoll_event ev;
//     memset(&ev, 0, sizeof(struct epoll_event));
//     ev.events = EPOLLOUT; // leverl trigger
//     ev.data.fd = fd;
//     return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
// }

int Epoll_mod_in(int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN; // leverl trigger
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

int Epoll_del_fd(int fd) {
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
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
    epollfd = epoll_create(1024);
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
    char recv_buf[1024], send_buf[1024];
    int nr_to_send = 0;
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
                    tasks[cli_fd].init(cli_fd);
                    if (Set_fd_nonblock(cli_fd)==-1) {
                        ERR_EXIT("fail to set client fd nonblock!\n");
                    }
                    if (-1==Epoll_add_in(cli_fd, 1)) {
                        printf("fail to add client fd IN!\n");
                    }

                    printf("accept fd: %d\n", cli_fd);
                    // if (-1==tasks[task_nr++]()) {
                    //     close(cli_fd);
                    //     task_nr--;
                    // }
                    // if (Set_fd_nonblock(cli_fd)==-1) {
                    //     ERR_EXIT("fail to set client fd nonblock!\n");
                    // }
                    // if (-1==Epoll_add_out(cli_fd, (void*)&tasks[task_nr], 1)) {
                    //     printf("fail to add client_fd OUT!\n");
                    // }
                } else {
                    printf("sockfd: %d can be read!\n", sockfd);
                    if (sockfd>0 && sockfd<MAX_EVENTS) {
                        printf("msock is %d!\n", tasks[sockfd].msock);
                        threadpool.addTask(&tasks[sockfd]);
                        // if (-1==tasks[sockfd]()) {
                        //     printf("close one client\n");
                        //     // close(sockfd);
                        // } else {
                        //     if (-1==Epoll_mod_out(sockfd)) {
                        //         printf("fail to mod client fd OUT!\n");
                        //         printf("%s\n", strerror(errno));
                        //     }
                        //     // if (-1==Epoll_add_out(sockfd, (void*)&tasks[sockfd], 1)) {
                        //     //     printf("fail to mod client fd OUT!\n");
                        //     //     printf("%s\n", strerror(errno));
                        //     // }
                        // }
                    } else {
                        printf("sockfd isn't valid!\n");
                    }
                    // memset(recv_buf, 0, sizeof(recv_buf));
                    // int nr = recv(events[k].data.fd, recv_buf, sizeof(recv_buf), 0);
                    // if (nr==-1) {
                    //     ERR_EXIT("fail to recv from client!\n");
                    // } else if (nr==0) {
                    //     printf("client closed!\n");
                    //     if (-1==Epoll_del_fd(events[k].data.fd)) {
                    //         Epoll_del_fd(events[k].data.fd);
                    //     }
                    //     close(events[k].data.fd);
                    //     continue;
                    // } else if (nr>0) {
                    //     printf("recv %d bytes: %s", nr, recv_buf);
                    //     // printf("%d %d\n", buf[nr-2], buf[nr-1]);
                    // }
                    // if (-1==Epoll_mod_out(events[k].data.fd)) {
                    //     printf("fail to add client_fd OUT!\n");
                    // }
                    // memset(send_buf, 0, sizeof(send_buf));
                    // strncpy(send_buf, recv_buf, nr);
                    // nr_to_send = nr;
                }
            } else if (events[k].events & EPOLLOUT) {
                // if (nr_to_send>0) {
                //     int nr = send(events[k].data.fd, send_buf, nr_to_send, 0);
                //     if (nr==-1) {
                //         ERR_EXIT("fail to send to client!\n");
                //     } else {
                //         printf("send success: %d bytes!", nr);
                //     }
                //     nr_to_send = 0;
                // }
                if (sockfd!=listenfd) {
                    // sleep(5);
                    // Task& t = *((Task*)events[k].data.ptr);
                    Task& t = tasks[sockfd];
                    printf("fd: %d\n", sockfd);
                    t.write();
                    if (-1==Epoll_del_fd(sockfd)) {
                        printf("fail to del events OUT!\n");
                    }
                    // break;
                }
            } else if (events[k].events & (EPOLLERR|EPOLLHUP|EPOLLRDHUP)) {

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