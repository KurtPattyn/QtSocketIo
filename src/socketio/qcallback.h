#ifndef QCALLBACK_H
#define QCALLBACK_H

#include <functional>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QAbstractCallback
{
public:
    QAbstractCallback() {}
    virtual ~QAbstractCallback() {}

    void operator()(QJsonArray array) {
        callback(array);
    }

protected:
    virtual void callback(QJsonArray array) = 0;
};

template <typename...>
struct List
{
    enum { size = 0 };
};

template <typename Head, typename... Tail>
struct List<Head, Tail...>
{
    typedef Head Car;
    typedef List<Tail...> Cdr;
    enum { size = 1 + sizeof...(Tail) };
};

template <typename L, int N>
struct ListAt
{
    typedef typename ListAt<typename L::Cdr, N - 1>::Value Value;
};

template <typename L>
struct ListAt<L, 0>
{
    typedef typename L::Car Value;
};

template <typename L>
struct ListLast
{
    typedef typename ListAt<L, L::size - 1>::Value Value;
};

template<typename Function>
struct function_traits
{
    template <typename Func> static char test(decltype(&Func::operator()));
    template <typename Func> static long test(...);

    enum
    {
        is_function = sizeof(test<Function>(0)) == sizeof(char),
        is_lambda = is_function && true,
        is_static_function = false,
        is_member_function = false,
        is_const_function = false
    };

    enum { argument_count = function_traits<decltype(&Function::operator())>::argument_count };
    typedef typename function_traits<decltype(&Function::operator())>::arguments arguments;
    typedef typename function_traits<decltype(&Function::operator())>::return_type return_type;
    typedef typename function_traits<decltype(&Function::operator())>::object_type object_type;
    typedef Function function_type;
};

template <typename Object, typename ReturnValue, typename...Arguments>
struct function_traits<ReturnValue (Object::*)(Arguments...)>
{
    enum
    {
        is_function = true,
        is_lambda = false,
        is_static_function = false,
        is_member_function = true,
        is_const_function = false
    };

    enum { argument_count = sizeof...(Arguments) };
    typedef List<Arguments...> arguments;
    typedef ReturnValue return_type;
    typedef Object object_type;
    typedef ReturnValue (Object::*function_type)(Arguments...);
};

template <typename Object, typename ReturnValue, typename...Arguments>
struct function_traits<ReturnValue (Object::*)(Arguments...) const>
{
    enum
    {
        is_function = true,
        is_lambda = false,
        is_static_function = false,
        is_member_function = true,
        is_const_function = true
    };
    enum { argument_count = sizeof...(Arguments) };
    typedef List<Arguments...> arguments;
    typedef ReturnValue return_type;
    typedef Object object_type;
    typedef ReturnValue (Object::*function_type)(Arguments...) const;
};

template <typename ReturnValue, typename...Arguments>
struct function_traits<ReturnValue (*)(Arguments...)>
{
    enum
    {
        is_function = true,
        is_lambda = false,
        is_static_function = true,
        is_member_function = false,
        is_const_function = false
    };
    enum { argument_count = sizeof...(Arguments) };
    typedef List<Arguments...> arguments;
    typedef ReturnValue return_type;
    typedef ReturnValue (*function_type)(Arguments...);
    typedef void object_type;
};

template <typename ReturnValue, typename...Arguments>
struct function_traits<ReturnValue (&)(Arguments...)>
{
    enum
    {
        is_function = true,
        is_lambda = false,
        is_static_function = true,
        is_member_function = false,
        is_const_function = false
    };
    enum { argument_count = sizeof...(Arguments) };
    typedef List<Arguments...> arguments;
    typedef ReturnValue return_type;
    typedef ReturnValue (&function_type)(Arguments...);
    typedef void object_type;
};

template <typename Callback>
class FunctionCallback:public QAbstractCallback
{
public:
    FunctionCallback(Callback callback) : QAbstractCallback(), m_callback(callback)
    {
    }
    ~FunctionCallback() {}

protected:
    void callback(QJsonArray array) {
        m_callback(array);
    }

private:
    Callback m_callback;
};

/*class SlotCallback:public QAbstractCallback
{
public:
    SlotCallback(QObject *object, const char *member);

protected:
    void callback(QJsonArray array) {
    }

private:
    QObject *m_pObject;
    QByteArray m_member;
};*/

QT_END_NAMESPACE

#endif // QCALLBACK_H
