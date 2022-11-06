#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>

struct tm;

namespace logger 
{
// 类作用域枚举,防止名称污染
enum class Level { Debug, Info, Warning, Error, Fatal };
class FileLogger; // 写文档用的日志类
class ConsoleLogger; // 控制台输出用的日志类
class BaseLogger; // 纯虚基类

class BaseLogger {
private:  
    class LogStream; // 用于文本缓冲的内部类声明
public:
    BaseLogger() = default;
    virtual ~BaseLogger() = default;
    // 重载 operator() 返回缓冲区对象
    virtual LogStream operator()(Level nLevel = Level::Debug);  
private:
    const tm* getLocalTime();
    // 供缓冲区对象析构时调用（函数加锁保证线程安全）
    void endline(Level nLevel, std::string& oMessage);
    // 纯虚函数，预留接口，由派生类实现
    virtual void output(const tm* p_tm, const char* str_level, const char* str_message) = 0;
private:
    std::mutex _mtx;
    tm _localTime;
};

// 用于文本缓冲区的类，继承 std::ostringstream
class BaseLogger::LogStream : public std::ostringstream {
public:
    LogStream(BaseLogger &logger, Level level)
        : _Logger(logger), _Level(level) {}
    
    LogStream(const LogStream &ls)
        : _Logger(ls._Logger), _Level(ls._Level) {}
    // 重写析构函数，在对象析构时打日志
    ~LogStream() {
        std::string tmp = this->str();
        this->str("");
        _Logger.endline(_Level, tmp);
    }
private:
    BaseLogger &_Logger;
    Level   _Level;
};

class ConsoleLogger : public BaseLogger {
    virtual void output(const tm* p_tm, const char* str_level, const char* str_message);
};

class FileLogger : public BaseLogger {
public:
    FileLogger(std::string filename) noexcept;
    FileLogger(const FileLogger&) = delete;
    // FileLogger(FileLogger&&) = delete;
    virtual ~FileLogger();
private:
    virtual void output(const tm* p_tm, const char* str_level, const char* str_message);

private:
    std::ofstream _outfile;
};

    extern ConsoleLogger Console_Log;
    extern FileLogger File_log;
    extern FileLogger Out_File_log;
}
#endif