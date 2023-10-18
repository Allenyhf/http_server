#ifndef __MY_EPOLL_HPP
#define __MY_EPOLL_HPP
#include <sys/epoll.h>
#include <string.h>

extern int epollfd;

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
    // printf("OUT fd: %d\n", fd);
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int Epoll_add_io(int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN|EPOLLOUT; // leverl trigger
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int Epoll_mod_out(int fd) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLOUT; // leverl trigger
    ev.data.fd = fd;
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

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

#endif