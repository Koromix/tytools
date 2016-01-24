/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QThread>

#include "descriptor_notifier.hh"

using namespace std;

DescriptorNotifier::DescriptorNotifier(ty_descriptor desc, QObject *parent)
    : QObject(parent)
{
    addDescriptor(desc);
}

DescriptorNotifier::DescriptorNotifier(ty_descriptor_set *set, QObject *parent)
    : QObject(parent)
{
    if (set)
        addDescriptorSet(set);
}

void DescriptorNotifier::addDescriptorSet(ty_descriptor_set *set)
{
    for (unsigned int i = 0; i < set->count; i++)
        addDescriptor(set->desc[i]);
}

void DescriptorNotifier::addDescriptor(ty_descriptor desc)
{
    execute([=]() {
#ifdef _WIN32
        auto notifier = new QWinEventNotifier(desc, this);
        connect(notifier, &QWinEventNotifier::activated, this, &DescriptorNotifier::activated);
#else
        auto notifier = new QSocketNotifier(desc, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, &DescriptorNotifier::activated);
#endif

        notifier->setEnabled(enabled_);
        notifiers_.push_back(notifier);
    });
}

void DescriptorNotifier::setEnabled(bool enable)
{
    execute([=]() {
        enabled_ = enable;
        for (auto notifier: notifiers_)
            notifier->setEnabled(enable);
    });
}

void DescriptorNotifier::clear()
{
    execute([=]() {
        for (auto notifier: notifiers_)
            delete notifier;
        notifiers_.clear();
    });
}

void DescriptorNotifier::execute(function<void()> f)
{
    if (thread() != QThread::currentThread()) {
        // See descriptor_notifier.hh for information about std_function_void_void
        QMetaObject::invokeMethod(this, "executeAsync", Qt::BlockingQueuedConnection,
                                  Q_ARG(std_function_void_void, f));
    } else {
        f();
    }
}

void DescriptorNotifier::executeAsync(function<void()> f)
{
    f();
}
