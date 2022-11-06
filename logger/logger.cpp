#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include "logger.h"

using namespace logger;

ConsoleLogger logger::Console_Log;
FileLogger logger::File_log("build_at_" __DATE__ "_" __TIME__ ".log");
FileLogger logger::Out_File_log("Out.log");

#ifdef WIN32
#define localtime_r(_Time, _Tm) localtime_s(_Tm, _Time)
#endif

BaseLogger::LogStream BaseLogger::operator()(Level nLevel) {
    return LogStream(*this, nLevel);
}

static const std::unordered_map<Level, const char*> lev_map =
{
    { Level::Debug, "Debug" },
    { Level::Info, "Info" },
    { Level::Warning, "Warning" },
    { Level::Error, "Error" },
    { Level::Fatal, "Fatal" },
};


std::ostream& operator<<(std::ostream &out, const tm* cur_tm) {
    return out << 1900 + cur_tm->tm_year << '-'
        << std::setfill('0') << std::setw(2) << cur_tm->tm_mon+1 << '-'
        << std::setfill('0') << std::setw(2) << cur_tm->tm_mday << ' '
        << std::setfill('0') << std::setw(2) << cur_tm->tm_hour << ":"
        << std::setfill('0') << std::setw(2) << cur_tm->tm_min << ":"
        << std::setfill('0') << std::setw(2) << cur_tm->tm_sec;
}

const tm* BaseLogger::getLocalTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    localtime_r(&in_time_t, &_localTime);
    return &_localTime;
}

void BaseLogger::endline(logger::Level nlevel, std::string& oMessae) {
    _mtx.lock();
    output(getLocalTime(), lev_map.find(nlevel)->second, oMessae.c_str()); // const map 不能用operator[]
    _mtx.unlock();
}

void ConsoleLogger::output(const tm* p_tm, const char* str_level, const char* str_message) {
    std::cout << '[' << p_tm << ']' << '<' << str_level  << '>' << '\t' << str_message << std::endl;
    std::cout << std::flush;
}

FileLogger::FileLogger(std::string filename) noexcept 
    : BaseLogger()
{
    const char* valid_filename = filename.c_str();
    _outfile.open(valid_filename, std::ios::out | std::ios::app);
    if (!_outfile.is_open()) {
        abort();
    }
}

FileLogger::~FileLogger() {
    _outfile.flush();
    _outfile.close();
}

void FileLogger::output(const tm *p_tm, const char *str_level, const char *str_message) {
    _outfile << '[' << p_tm << ']' << '<' << str_level << '>' << '\t' << str_message << std::endl;
    _outfile << std::flush;
}