/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QSettings>

#include "database.hh"

using namespace std;

void SettingsDatabase::put(const QString &key, const QVariant &value)
{
    if (!settings_)
        return;

    settings_->setValue(key, value);
}

void SettingsDatabase::remove(const QString &key)
{
    if (!settings_)
        return;

    settings_->remove(key);
}

QVariant SettingsDatabase::get(const QString &key, const QVariant &default_value) const
{
    if (!settings_)
        return default_value;

    return settings_->value(key, default_value);
}

void SettingsDatabase::clear()
{
    if (!settings_)
        return;

    settings_->clear();
}
