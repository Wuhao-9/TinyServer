#ifndef LOCKER_H_
#define LOCKER_H_

#include <pthread.h>
#include <semaphore.h>
#include <exception>
class locker {
public:
    locker() {
        if (pthread_mutex_init(&_mutex_locker, nullptr) == -1) {
            throw std::exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&_mutex_locker);
    }

    bool lock() {
        return pthread_mutex_lock(&_mutex_locker) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&_mutex_locker) == 0;
    }
    
    pthread_mutex_t& getlock() {
        return _mutex_locker;
    }

private:
    pthread_mutex_t _mutex_locker;
};

class cond {
public:
    cond() {
        if (pthread_cond_init(&_condition, nullptr) == -1) {
            throw std::exception();
        }
    }

    ~cond() {
        pthread_cond_destroy(&_condition);
    }

    bool wait(pthread_mutex_t* mutlock) {
        return pthread_cond_wait(&_condition, mutlock) == 0;
    }

    bool timedwait(pthread_mutex_t* locker, timespec* outtime) {
        return pthread_cond_timedwait(&_condition, locker, outtime) == 0;
    }

    bool signal() {
        return pthread_cond_signal(&_condition) == 0;
    }

    bool broadcast() {
        return pthread_cond_broadcast(&_condition) == 0;
    }

private:
    pthread_cond_t _condition;
};

class sem {
public:
    sem(unsigned int resorce_num = 0) {
        if (sem_init(&_semaphore, 0, resorce_num) == -1) {
            throw std::exception();
        }
    }

    ~sem() {
        sem_destroy(&_semaphore);
    }

    bool wait() {
        return sem_wait(&_semaphore) == 0;
    }

    bool post() {
        return sem_post(&_semaphore) == 0;
    }
private:
    sem_t _semaphore;    
};

#endif