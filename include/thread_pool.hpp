#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include <iostream>
#include <unistd.h>
#include <iomanip>
#include <pthread.h>
#include <list>
#include "locker.h"

template<class T>
class thread_pool {
public:
    thread_pool(const int thr_size = 12, const int Max_task_num = 30000);
    ~thread_pool();
    bool append_request(T* request);
private:
    static void* worker(void* agrs);
private:
    pthread_t* _pool;
    const int _thr_size;
    const int _Max_task_size;
    bool _is_stop;
    std::list<T*> _task_queue;
    locker _mutex;
    sem _semphore;
};


template<class T>
thread_pool<T>::thread_pool(const int thr_size, const int Max_task_num)
    :
    _Max_task_size(Max_task_num),
    _thr_size(thr_size),
    _is_stop(false)
    {
        _pool = new (std::nothrow) pthread_t[_thr_size];
        if (_pool == nullptr) {
            throw std::exception();
        }

        for (int i = 0; i < thr_size; i++) {
            if (pthread_create(_pool+i, nullptr, worker, this) == -1) {
                delete[] _pool;
                throw std::exception();
            }
            // std::cout << std::setw(11) << "The thread " << i+1 << " is created!\n";
        }

        for (int i = 0; i < thr_size; i++) {
            if (pthread_detach(_pool[i]) == -1) {
                delete[] _pool;
                throw std::exception();
            }
        }
    }

template<class T>
thread_pool<T>::~thread_pool() {
    delete[] _pool;
    _is_stop = true;
}

template<class T>
void* thread_pool<T>::worker(void* agrs) {
    thread_pool* const cur_thr = reinterpret_cast<thread_pool* const>(agrs);
    while (!cur_thr->_is_stop) {
        cur_thr->_semphore.wait();
        cur_thr->_mutex.lock();
        T* request = cur_thr->_task_queue.front();
        if (request == nullptr) {
            logger::File_log(logger::Level::Error) << "line" << "#" << __LINE__ << "Task-Q have a nullptr!"; 
        }
        cur_thr->_task_queue.pop_front();
        cur_thr->_mutex.unlock();
        
        request->process();
    }
    return nullptr;
}

template<class T>
bool thread_pool<T>::append_request(T* request) {
    if (_task_queue.size() >= _Max_task_size) {
        return false;
    }

    _mutex.lock();
    _task_queue.push_back(request);
    _mutex.unlock();
    _semphore.post();
    return true;
}

#endif