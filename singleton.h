#ifndef SINGLETON_H
#define SINGLETON_H

#include <memory>
template <typename T>
class Singleton
{
protected:
    Singleton() = default;
    ~Singleton() = default;

public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    static std::shared_ptr<T> getInstance() {
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }
};

#endif // SINGLETON_H
