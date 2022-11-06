#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include <sys/uio.h>
#include <arpa/inet.h>
#include <sstream>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include "logger.h"

const int Max_users_size = 50000;
extern bool add_event(int _epfd, int target_fd, bool is_Epolloneshot);
extern bool close_descriptors(int _epfd, int target_fd);
extern bool modifiy_event(int _epfd, int target_fd, int target_event);

class http_conn {
    friend inline void set_exampleToep(int ep_fd);

public:
    static const int Readbuf_Maxsize = 2048;
    static const int Writebuf_Maxsize = 1024;
    /*
        NO_REQUEST 请求不完整
        GET_REQUEST 成功解析请求
        BAD_REQUEST 请求语法错误
        NO_RESOURCE 服务器没有资源
    */ 
    enum LINE_STATUS {LINE_OPEN, LINE_OK, LINE_BAD};
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, FILEOK_REQUEST, NO_RESOURCE, CLOSED_CONNECTION, INTERNAL_ERROR, Permission_denied};
    enum CHECK_STATE {CHECK_FIRST, CHECK_HEADER, CHECK_CONTENT};
    enum METHOD {GET, POST, HEAD, DELETE};

public:
    http_conn();
    ~http_conn();
    void process(); // 由线程处理请求(解析请求)并返回响应信息
    void init_to_client(int clifd, sockaddr_in* cliaddr);
    bool readmesg(); // 读取请求，读取完后加入至任务队列
    bool close_conn();
    bool write_process(HTTP_CODE stat); // 向客户端发送响应数据
    HTTP_CODE parse_request(); // 解析HTTP请求(主状态机)
    static inline bool can_add() {return _users_num >= Max_users_size ? false : true;};
    bool write2cli();

    class respones {
    public:
        constexpr static const char* ok_200_title = "OK";
        constexpr static const char* error_400_title = "Bad Request";
        constexpr static const char* error_403_form = "You don`t have permission to get file from this sever.";
        constexpr static const char* error_404 = "Not found";
        constexpr static const char* error_500 = "Internal fail";

    public:
        void add_firstline(int state,const char* title) {
            resp_header << "HTTP/1.1 " << state << " " << title << "\r\n"; 
        }
        void add_headers(int content_len, bool connect_state) {
            add_content_type();
            add_linger(connect_state);
            add_contentlen(content_len);
            add_blank_line();
        }
        void add_contentlen(int len) {
            resp_header << "content-length: " << len << "\r\n";
        }
        void add_content_type(const char* type = "text/html") {
            resp_header << "content-type: " << type << "\r\n";
        }

        void add_linger(bool connect_state) {
            if (connect_state) {
                resp_header << "Connection: " << "keep-alive\r\n";
            } else {
                resp_header << "Connection: " << "close\r\n";
            }
        }
        void add_blank_line() {
            resp_header << "\r\n";
        }

        std::stringstream resp_header;
    };

private:
    HTTP_CODE parse_request_firstline(char* content);
    HTTP_CODE parse_request_header(char* content);
    HTTP_CODE parse_request_content(char* content);
    LINE_STATUS parse_oneline(); 
    void unmap();
    void init();
    HTTP_CODE do_request();

    
private:
    static int _users_num;
    static int _epfd; // 所有socket的事件都被注册至同一个epoll实例中
    std::stringstream _outstr;
    std::string _target_file;
    struct stat _file_stat {};
    int _clifd;
    sockaddr_in _cliaddr {};
    char _read_buffer[Readbuf_Maxsize] {};
    int _read_index = 0;
    int _cur_parse_index = 0;
    int _cur_line_start = 0;
    
    CHECK_STATE _main_machine;
    
    static const char* root_path; // 资源路径
    struct {
        char* url;
        METHOD method;
        char* version;
    } _requ_info {};

    struct {
        char* host;
        bool connect_state;
        char* Encoding;
        int content_len = 0; // 默认没有请求体
    } _header_info {};

    struct {
        void* tarfile_addr;
        struct {
            const char* send_respones;
            std::string tmp;
            int respones_size;
        } head;
    } _write_addr {};

    iovec iov[2] {};
    int iov_num = 0;
    respones herd_method;
    
};
    
#endif