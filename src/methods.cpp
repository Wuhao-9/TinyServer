#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "logger.h"
#include "http_conn.h"

bool add_event(int _epfd, int target_fd, bool is_Epolloneshot) {
    epoll_event events;
    events.data.fd = target_fd;
    events.events = EPOLLIN | EPOLLRDHUP;
    is_Epolloneshot ? events.events |= EPOLLONESHOT : events.events;
    if (epoll_ctl(_epfd, EPOLL_CTL_ADD, target_fd, &events) == -1) {
        return false;
    }

    int old_flags;
    if ((old_flags = fcntl64(target_fd, F_GETFL, nullptr)) == -1) {
        perror("fcntl64");
        return false;
    }
    // 设置文件描述符为O_NONBLOCK
    if (fcntl64(target_fd, F_SETFL, old_flags | O_NONBLOCK) == -1) {
        perror("fcntl64");
        return false;
    }

    return true;
}

bool close_descriptors(int _epfd, int target_fd) {
    if (epoll_ctl(_epfd, EPOLL_CTL_DEL, target_fd, nullptr) == -1) {
        return false;
    }
    close(target_fd);
    return true;
}


// 必须使用EPOLL_CTL_MOD以修改事件，不能使用EPOLL_CTL_ADD追加
bool modifiy_event(int _epfd, int target_fd, int target_event) {
    epoll_event event;
    event.data.fd = target_fd;
    event.events = target_event | EPOLLRDHUP | EPOLLONESHOT;
    if (epoll_ctl(_epfd, EPOLL_CTL_MOD, target_fd, &event) == -1) {
        std::cerr << "EPOLL_CTL_MOD failed!" << std::endl;
        return false;
    }
    return true;
}
