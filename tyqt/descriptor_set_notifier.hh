/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef DESCRIPTOR_SET_NOTIFIER_HH
#define DESCRIPTOR_SET_NOTIFIER_HH

#ifdef _WIN32
    #include <QWinEventNotifier>
#else
    #include <QSocketNotifier>
#endif

#include <memory>
#include <vector>

#include "ty.h"

class DescriptorSetNotifier : public QObject {
    Q_OBJECT

#ifdef _WIN32
    std::vector<std::unique_ptr<QWinEventNotifier>> notifiers_;
#else
    std::vector<std::unique_ptr<QSocketNotifier>> notifiers_;
#endif

    bool enabled_ = true;

public:
    DescriptorSetNotifier(ty_descriptor_set *set, QObject *parent = nullptr);
    DescriptorSetNotifier(QObject *parent = nullptr)
        : DescriptorSetNotifier(nullptr, parent) {}

    void setDescriptorSet(ty_descriptor_set *set);
    void addDescriptorSet(ty_descriptor_set *set);

    bool isEnabled() const;

signals:
    void activated(ty_descriptor desc);

public slots:
    void setEnabled(bool enable);
    void clear();

private slots:
    void activatedDesc(ty_descriptor desc);
};

#endif
