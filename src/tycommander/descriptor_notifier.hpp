/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef DESCRIPTOR_NOTIFIER_HH
#define DESCRIPTOR_NOTIFIER_HH

#ifdef _WIN32
    #include <QWinEventNotifier>
#else
    #include <QSocketNotifier>
#endif

#include <functional>
#include <vector>

#include "../libty/system.h"

class DescriptorNotifier : public QObject {
    Q_OBJECT

#ifdef _WIN32
    std::vector<QWinEventNotifier *> notifiers_;
#else
    std::vector<QSocketNotifier *> notifiers_;
#endif

    bool enabled_ = true;

public:
    DescriptorNotifier(QObject *parent = nullptr)
        : QObject(parent) {}
    DescriptorNotifier(ty_descriptor desc, QObject *parent = nullptr);
    DescriptorNotifier(ty_descriptor_set *set, QObject *parent = nullptr);

    void addDescriptorSet(ty_descriptor_set *set);
    void addDescriptor(ty_descriptor desc);

    void setDescriptorSet(ty_descriptor_set *set)
    {
        clear();
        addDescriptorSet(set);
    }
    void setDescriptor(ty_descriptor desc)
    {
        clear();
        addDescriptor(desc);
    }

    bool isEnabled() const { return enabled_; }

public slots:
    void setEnabled(bool enable);
    void clear();

signals:
    void activated(ty_descriptor desc);

private:
    void execute(std::function<void()> f);
    /* On Qt 5.2.1, QMetaObject::invokeMethod() fails on templated types
       such as std::function<void()>. */
    typedef std::function<void()> std_function_void_void;
    Q_INVOKABLE void executeAsync(std_function_void_void f);
};

#endif
