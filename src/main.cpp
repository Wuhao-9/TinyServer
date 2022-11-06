#include <arpa/inet.h>
#include <sys/epoll.h>
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <libgen.h>
#include <unistd.h>
#include <exception>
#include <fcntl.h>
#include <cstdlib>
#include "logger.h"
#include "locker.h"
#include "http_conn.h"
#include "thread_pool.hpp"

extern bool add_event(int _epfd, int target_fd, bool is_Epolloneshot);
extern bool close_descriptors(int _epfd, int target_fd);
extern bool modifiy_event(int _epfd, int target_fd, int target_event);
inline void set_exampleToep(int ep_fd) {http_conn::_epfd = ep_fd;}

const int Max_events_amount = 20000;
int main(int argc, char* argv[]) {
    // 处理SIGPIPE

    if (argc < 2) {
        std::cerr << "Please specificed a port number\n";
        std::cerr << "format : " << basename(argv[0]) << " <port>\n";
        exit(EXIT_FAILURE);
    }

    int port_num = atoi(argv[1]);
    
    int lfd = -1;
    if ((lfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    } 

    // 设置端口复用
    int optval = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // bind
    sockaddr_in seraddr;
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(port_num);
    inet_pton(AF_INET, "192.168.109.132", &seraddr.sin_addr.s_addr);
    if (bind(lfd, reinterpret_cast<sockaddr*>(&seraddr), sizeof(seraddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE); 
    }
    
    if (listen(lfd, 10) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    thread_pool<http_conn>* pool = nullptr;
    try {
        pool = new thread_pool<http_conn>;
    } catch (std::exception &except) {
        std::cerr << "thread pool create failed!" << std::endl;
        std::cerr << "application will now exit!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 创建http_conn数组，保存加入连接的client信息
    http_conn* users = new http_conn[Max_users_size];

    int ep_fd = epoll_create1(0);
    if (ep_fd == -1) {
        perror("epoll_create1");
        getchar();
        exit(EXIT_FAILURE);
    }
    set_exampleToep(ep_fd);
    add_event(ep_fd, lfd, false);

    sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int amount = -1;
    epoll_event ready_events[Max_users_size] {};

    while (true) {
        amount = epoll_wait(ep_fd, ready_events, Max_events_amount, -1);
        if (amount == -1 && errno == EINTR) {
            continue;
        }
        if (amount == -1 && errno != EAGAIN) {
            perror("epoll_wait");
            break;
        } else if (amount == 0) {
            continue;
        } else {
            for (int i = 0; i < amount; i++) {
                int cur_fd = ready_events[i].data.fd;
                if (cur_fd == lfd) {
                    int clifd = accept(lfd, reinterpret_cast<sockaddr*>(&cli_addr), &clilen);
                    if (clifd == -1) {
                        std::cerr << "One user failed to <accept>\n";
                        continue;
                    }
                    
                    if (http_conn::can_add()) {
                        users[clifd].init_to_client(clifd, &cli_addr);
                        add_event(ep_fd, clifd, true);
                    } else {
                        // 提示用户当前服务器人数过多，不能加入，稍后尝试
                    }
                } else {
                    if (ready_events[i].events & (EPOLLRDHUP | EPOLLERR)) {
                        users[cur_fd].close_conn();
                        continue;
                    } else if (ready_events[i].events & EPOLLIN) {
                        if (users[cur_fd].readmesg()) {
                            while (!pool->append_request(users + cur_fd)) 
                                usleep(3);
                        } else {
                            std::cerr << "Read message fail.\n";
                            users[cur_fd].close_conn();
                        }
                    } else if (ready_events[i].events & EPOLLOUT) {
                        if (users[cur_fd].write2cli()) {
                            continue;
                        }
                        users[cur_fd].close_conn();
                    }
                } 

            }
        }

    }
    close(ep_fd);
    close(lfd);
    delete[] users;
    delete pool;
}