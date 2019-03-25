/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <QSettings>

#include "database.hpp"

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
