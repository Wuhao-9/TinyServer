#include "http_conn.h"
#include "logger.h"

extern bool close_descriptors(int _epfd, int target_fd);

const char* http_conn::root_path = "/home/wuhao/MyProject/WebServer/Resources";

int http_conn::_users_num = 1;
int http_conn::_epfd = -1;

http_conn::http_conn() {

}

http_conn::~http_conn() {
    
}

bool http_conn::readmesg() {
    if (_read_index == Readbuf_Maxsize) {
        return false;
    }
    while (true) {
        int amount = recv(_clifd, _read_buffer + _read_index, Readbuf_Maxsize-_read_index, 0);
        if (amount == 0) {
            // client连接断开，直接关闭fd || 请求报文过长，读缓冲区装不下，即（Readbuf_Maxsize - _read_index = 0）
            return false;
        } else if (amount == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // logger::File_log() << _read_buffer;
                logger::File_log() << _read_buffer;
                break;
            }
            return false;
        }
        _read_index += amount;
    }
    // std::cout << _read_buffer;
    return true;
}


void http_conn::init_to_client(int clifd, sockaddr_in* cliaddr) {
    this->_clifd = clifd;
    this->_cliaddr = *cliaddr;
    this->_users_num++;
    init();
}

void http_conn::init() {
    this->herd_method.resp_header.str("");
    memset(_read_buffer, 0, strlen(_read_buffer));
    memset(iov, 0, sizeof(iov));
    memset(&_requ_info, 0, sizeof(_requ_info));
    memset(&_header_info, 0, sizeof(_header_info));
    memset(&_write_addr, 0, sizeof(_write_addr));
    iov_num = 0;
    _write_addr.tarfile_addr = nullptr;
    _cur_parse_index = 0;
    _cur_line_start = 0;
    _read_index = 0;
    _outstr.str("");
    _target_file = "";
    _main_machine = CHECK_FIRST;
}

bool http_conn::close_conn() {
    if ( close_descriptors(this->_epfd, this->_clifd) ) {
        this->herd_method.resp_header.str("");
        memset(_read_buffer, 0, strlen(_read_buffer));
        memset(iov, 0, sizeof(iov));
        memset(&_requ_info, 0, sizeof(_requ_info));
        memset(&_header_info, 0, sizeof(_header_info));
        memset(&_write_addr, 0, sizeof(_write_addr));
        iov_num = 0;
        _main_machine = CHECK_FIRST;
        _header_info.connect_state = false;
        _write_addr.tarfile_addr = nullptr;
        _cur_parse_index = 0;
        _cur_line_start = 0;
        _read_index = 0;
        this->_clifd = -1;
        this->_users_num--;
        _outstr.str("");
        _target_file = "";
        return true;
    }    
    logger::File_log() << "close_conn-failed!";
    return false;
}

bool http_conn::write_process(HTTP_CODE stat) {
    switch (stat) {
        case BAD_REQUEST :
        {
            herd_method.add_firstline(400, herd_method.error_400_title);
            herd_method.add_headers(strlen(herd_method.error_400_title), false);
            herd_method.resp_header << herd_method.error_400_title;
            break;
        }
        case INTERNAL_ERROR :
        {
            herd_method.add_firstline(500, herd_method.error_500);
            herd_method.add_headers(strlen(herd_method.error_500), false);
            herd_method.resp_header << herd_method.error_500;
            break;
        }
        case NO_RESOURCE :
        {
            herd_method.add_firstline(404, herd_method.error_404);
            herd_method.add_headers(strlen(herd_method.error_404), false);
            herd_method.resp_header << herd_method.error_404;
            break;
        }
        case Permission_denied :
        {
            herd_method.add_firstline(403, herd_method.error_403_form);
            herd_method.add_headers(strlen(herd_method.error_403_form), false);
            herd_method.resp_header << herd_method.error_403_form;
            break;
        }

        case FILEOK_REQUEST : 
        {
            herd_method.add_firstline(200, herd_method.ok_200_title);
            herd_method.add_headers(_file_stat.st_size, _header_info.connect_state);
            _write_addr.head.tmp = herd_method.resp_header.str();
            herd_method.resp_header.str("");
            _write_addr.head.respones_size = _write_addr.head.tmp.size();
            _write_addr.head.send_respones = _write_addr.head.tmp.c_str();
            iov[0].iov_base = const_cast<char*>(_write_addr.head.send_respones);
            iov[0].iov_len = _write_addr.head.respones_size;
            iov[1].iov_base = _write_addr.tarfile_addr;
            iov[1].iov_len = _file_stat.st_size;
            iov_num = 2;
            return true;
        }
    default:
        return false;
    }
    _write_addr.head.tmp = herd_method.resp_header.str();
    herd_method.resp_header.str("");
    _write_addr.head.send_respones = _write_addr.head.tmp.c_str();
    _write_addr.head.respones_size = _write_addr.head.tmp.size();
    iov[0].iov_base = const_cast<char*>(_write_addr.head.send_respones);
    iov[0].iov_len = _write_addr.head.respones_size;
    iov_num = 1;
    return true;
}

void http_conn::process() {
    // parse current request
    HTTP_CODE read_ret = parse_request();
    if (read_ret == NO_REQUEST) {
        // 请求不完整，等待重新读取请求
        modifiy_event(_epfd, _clifd, EPOLLIN); // 再次设置监听读事件
        return;
    }

    // Response
    bool write_ret = write_process(read_ret);
    if (write_ret == false) {
        close_conn();   
        return;
    }
    modifiy_event(_epfd, _clifd, EPOLLOUT); // 若写入缓冲区成功，设置监听写事件
}


http_conn::HTTP_CODE http_conn::parse_request() {
    LINE_STATUS cur_linestate = LINE_OK; // 记录当前行的读取状态(请求行/请求头)
    HTTP_CODE retcode = NO_REQUEST; // 记录http请求结果

    while ((cur_linestate = parse_oneline()) == LINE_OK) {
        char* tmp = _read_buffer+_cur_line_start; // 获取当前正在解析的行
        _cur_line_start = _cur_parse_index; // 记录下一行的起始位置

        switch (_main_machine) {
            case CHECK_FIRST : 
            {
                retcode = parse_request_firstline(tmp);
                if (retcode == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_HEADER :
            {
                retcode = parse_request_header(tmp);
                if (retcode == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (retcode == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            default :
            {
                return INTERNAL_ERROR;
                break;
            }
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_firstline(char* content) {
    char* method {};

    _requ_info.url = strpbrk(content, " \t");
    if (!_requ_info.url) {
        return BAD_REQUEST;
    } 
    *_requ_info.url++ = '\0';

    method = content;
    if (strcasecmp("GET", method) == 0) {
        _requ_info.method = GET;
    } else {
        return BAD_REQUEST;
    }
    
    _requ_info.version = strpbrk(_requ_info.url, " \t");
    if (!_requ_info.version) {
        return BAD_REQUEST;
    }
    *_requ_info.version++ = '\0';
    if (strcasecmp(_requ_info.version, "HTTP/1.1") != 0 && strcasecmp(_requ_info.version, "HTTP/1.0") != 0) { //仅支持HTTP1.1
        return BAD_REQUEST;
    }


    if (strncasecmp(_requ_info.url, "http://", 7) == 0) {
        _requ_info.url += 7;
        _requ_info.url = strchr(_requ_info.url, '/');
    }
    if (!_requ_info.url || _requ_info.url[0] != '/') {
        return BAD_REQUEST;
    }
    
    // std::cout << "The request URL is : " << _requ_info.url << std::endl;
    _main_machine = CHECK_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_header(char* content) {
    if (content[0] == '\0') {
        // if (_header_info.content_len != 0) {
        //     return NO_REQUEST; // 当前未实现读取请求内容
        // }
        return GET_REQUEST;
    } else if (strncasecmp(content, "Host:", 5) == 0) {
        content += 5;
        content++;
        _header_info.host = content;
    } else if (strncasecmp(content, "Connection:", strlen("Connection:")) == 0) {
        content += strlen("Connection:");
        content++;
        strcasecmp("keep-alive", content) == 0 ? _header_info.connect_state = true : _header_info.connect_state = false;
    } else if (strncasecmp(content, "Accept-Encoding", strlen("Accept-Encoding")) == 0) {
        content += strlen("Accept-Encoding");
        content++;
        _header_info.Encoding = content;
    } else {
        // std::cout << "Parse header content fail...\n" << std::endl;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_request_content(char* content) {
    return HTTP_CODE::GET_REQUEST;
}

http_conn::LINE_STATUS http_conn::parse_oneline() {
    char tmp;
    for (; _cur_parse_index < _read_index; _cur_parse_index++) {
        tmp = _read_buffer[_cur_parse_index];
        if (tmp == '\r') {
            if (_cur_parse_index+1 == _read_index) {
                return LINE_OPEN;
            }
            else if (_read_buffer[_cur_parse_index+1] == '\n') {
                _read_buffer[_cur_parse_index++] = '\0';
                _read_buffer[_cur_parse_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::do_request() {
    _outstr << root_path << _requ_info.url;
    _target_file = _outstr.str();
    _outstr.str("");
    // 判断目标文件的状态信息
    if (stat(_target_file.c_str(), &_file_stat) == -1) {
        return NO_RESOURCE;
    }
    
    if (!(_file_stat.st_mode & S_IROTH)) {
        return Permission_denied;
    }

    if (S_ISDIR(_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    int file_fd = open(_target_file.c_str(), O_RDONLY, nullptr);
    if (file_fd == -1) {
        return INTERNAL_ERROR;
    }

    _write_addr.tarfile_addr = mmap64(nullptr, _file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (_write_addr.tarfile_addr == (void*)-1) {
        perror("mmap64");
        return INTERNAL_ERROR;
    }
    close(file_fd);
    return FILEOK_REQUEST;
}

void http_conn::unmap() {
    if (_write_addr.tarfile_addr) {
        munmap(_write_addr.tarfile_addr, _file_stat.st_size);
    }
}

bool http_conn::write2cli() {
    int amount = 0;
    int bytes_already_send = 0;
    int bytes_need_send = _write_addr.head.respones_size + _file_stat.st_size;
    std::cout << "---------------------------------------\n";
    std::cout << static_cast<const char*>(_write_addr.head.send_respones) << std::endl;
    std::cout << "---------------------------------------\n";
    if (bytes_need_send == 0) {
        modifiy_event(_epfd, _clifd, EPOLLIN);
        init();
        return true;
    }
    logger::Out_File_log() << static_cast<const char*>(_write_addr.head.send_respones);
    while (true) {
        amount = writev(_clifd, iov, iov_num);
        if (amount == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                modifiy_event(_epfd, _clifd, EPOLLOUT);
                return true;
            }
            perror("writev");
            this->unmap();
            return false;
        }
        bytes_already_send += amount;
        bytes_need_send -= amount;
        if (bytes_need_send <= 0) {
            this->unmap();
            if (_header_info.connect_state) {
                init();
                modifiy_event(_epfd, _clifd, EPOLLIN);
                return true;
            } else {
                modifiy_event(_epfd, _clifd, EPOLLIN);
                return false;
            }
        }
    }
    return true;
}