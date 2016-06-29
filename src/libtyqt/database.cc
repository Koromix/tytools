/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <QSettings>

#include "tyqt/database.hpp"

using namespace std;

void SettingsDatabase::put(const QString &key, const QVariant &value)
{
    settings_->setValue(key, value);
}

void SettingsDatabase::remove(const QString &key)
{
    settings_->remove(key);
}

QVariant SettingsDatabase::get(const QString &key, const QVariant &default_value) const
{
    return settings_->value(key, default_value);
}

void SettingsDatabase::clear()
{
    settings_->clear();
}

void DatabaseInterface::setGroup(const QString &group)
{
    group_ = group;
    if (!group_.endsWith("/"))
        group_ += "/";
}

void DatabaseInterface::put(const QString &key, const QVariant &value)
{
    if (db_)
        db_->put(compositeKey(key), value);
}

void DatabaseInterface::remove(const QString &key)
{
    if (db_)
        db_->remove(compositeKey(key));
}

QVariant DatabaseInterface::get(const QString &key, const QVariant &default_value) const
{
    if (db_)
        return db_->get(compositeKey(key), default_value);
    return default_value;
}

DatabaseInterface DatabaseInterface::subDatabase(const QString &prefix) const
{
    DatabaseInterface intf(*this);
    intf.setGroup(group_ + prefix);
    return intf;
}

QString DatabaseInterface::compositeKey(const QString &key) const
{
    return group_ + key;
}
