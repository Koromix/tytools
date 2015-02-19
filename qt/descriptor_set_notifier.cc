/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
    for (size_t i = 0; i < set->count; i++) {
#ifdef _WIN32
        auto notifier = make_unique<QWinEventNotifier>(set->desc[i]);
        connect(notifier.get(), &QWinEventNotifier::activated, this, &DescriptorSetNotifier::activatedDesc);
#else
        auto notifier = make_unique<QSocketNotifier>(set->desc[i], QSocketNotifier::Read);
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
    if (!enabled_)
        return;

    emit activated(desc);
}
