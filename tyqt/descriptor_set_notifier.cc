/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "descriptor_set_notifier.hh"

using namespace std;

DescriptorSetNotifier::DescriptorSetNotifier(ty_descriptor_set *set, QObject *parent)
    : QObject(parent)
{
    if (set)
        addDescriptorSet(set);

    interval_timer_.setSingleShot(true);
    connect(&interval_timer_, &QTimer::timeout, this, &DescriptorSetNotifier::restoreNotifiers);
}

void DescriptorSetNotifier::setDescriptorSet(ty_descriptor_set *set)
{
    clear();
    addDescriptorSet(set);
}

void DescriptorSetNotifier::addDescriptorSet(ty_descriptor_set *set)
{
    for (unsigned int i = 0; i < set->count; i++) {
#ifdef _WIN32
        auto notifier = unique_ptr<QWinEventNotifier>(new QWinEventNotifier(set->desc[i]));
        connect(notifier.get(), &QWinEventNotifier::activated, this, &DescriptorSetNotifier::activatedDesc);
#else
        auto notifier = unique_ptr<QSocketNotifier>(new QSocketNotifier(set->desc[i], QSocketNotifier::Read));
        connect(notifier.get(), &QSocketNotifier::activated, this, &DescriptorSetNotifier::activatedDesc);
#endif

        notifier->setEnabled(enabled_);

        notifiers_.push_back(move(notifier));
    }
}

void DescriptorSetNotifier::setMinInterval(int interval)
{
    if (interval < 0)
        interval = 0;

    interval_timer_.setInterval(interval);
}

bool DescriptorSetNotifier::isEnabled() const
{
    return enabled_;
}

int DescriptorSetNotifier::minInterval() const
{
    return interval_timer_.interval();
}

void DescriptorSetNotifier::setEnabled(bool enable)
{
    enabled_ = enable;

    for (auto &notifier: notifiers_)
        notifier->setEnabled(enable);
}

void DescriptorSetNotifier::clear()
{
    notifiers_.clear();
}

void DescriptorSetNotifier::activatedDesc(ty_descriptor desc)
{
    if (!enabled_ )
        return;

    if (interval_timer_.interval() > 0) {
        setEnabled(false);
        enabled_ = true;

        interval_timer_.start();
    }

    emit activated(desc);
}

void DescriptorSetNotifier::restoreNotifiers()
{
    if (!enabled_)
        return;

    setEnabled(true);
}
