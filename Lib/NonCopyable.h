#ifndef NLIB_NONCOPYABLE_H
#define NLIB_NONCOPYABLE_H

class NonCopyable
{
public:   
    NonCopyable(const NonCopyable&) = delete;
    void operator=(const NonCopyable&) = delete;
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
};

#endif  

