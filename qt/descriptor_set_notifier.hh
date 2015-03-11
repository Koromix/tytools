/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DESCRIPTOR_SET_NOTIFIER_HH
#define DESCRIPTOR_SET_NOTIFIER_HH

#ifdef _WIN32
#include <QWinEventNotifier>
#else
#include <QSocketNotifier>
#endif
#include <QTimer>

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
    QTimer interval_timer_;

public:
    DescriptorSetNotifier(ty_descriptor_set *set, QObject *parent = nullptr);
    DescriptorSetNotifier(QObject *parent = nullptr)
        : DescriptorSetNotifier(nullptr, parent) {}

    void setDescriptorSet(ty_descriptor_set *set);
    void addDescriptorSet(ty_descriptor_set *set);

    void setMinInterval(int interval);

    bool isEnabled() const;
    int minInterval() const;

signals:
    void activated(ty_descriptor desc);

public slots:
    void setEnabled(bool enable);
    void clear();

private slots:
    void activatedDesc(ty_descriptor desc);
    void restoreNotifiers();
};

#endif
