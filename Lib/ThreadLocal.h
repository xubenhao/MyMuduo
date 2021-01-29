#ifndef NLIB_THREADLOCAL_H
#define NLIB_THREADLOCAL_H
#include "Mutex.h"
#include "NonCopyable.h"

// 一个模板类对象
// 包含的m_nKey提供了所有线程共享的一个索引
// 包含的value，可供所有可访问此对象的线程调用，以得到调用线程自己的一个T对象
template<typename T>
class ThreadLocal : NonCopyable
{
public:
    ThreadLocal()
    {
        pthread_key_create(
            &m_nKey, 
            &ThreadLocal::destructor);
    }
    
    ~ThreadLocal()
    {
        pthread_key_delete(m_nKey);
    }
    
    T& value()
    {
        T* perThreadValue = (T*)(pthread_getspecific(m_nKey));
        if (!perThreadValue)
        {
          T* newObj = new T();
          pthread_setspecific(m_nKey, newObj);
          perThreadValue = newObj;
        }
        
        return *perThreadValue;
    }
    
private:
    static void destructor(void *x)
    {
        T* obj = (T*)(x);
        delete obj;
    }
    
private:
    pthread_key_t m_nKey;
};

#endif

