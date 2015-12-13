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

bool DescriptorSetNotifier::isEnabled() const
{
    return enabled_;
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

    emit activated(desc);
}
