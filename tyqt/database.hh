/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef DATABASE_HH
#define DATABASE_HH

#include <QString>
#include <QVariant>

class QSettings;

class Database {
public:
    virtual ~Database() {}

    virtual void put(const QString &key, const QVariant &value) = 0;
    virtual void remove(const QString &key) = 0;
    virtual QVariant get(const QString &key, const QVariant &default_value = QVariant()) const = 0;

    virtual void clear() = 0;
};

class SettingsDatabase : public Database {
    QSettings *settings_;

public:
    SettingsDatabase(QSettings *settings = nullptr)
        : settings_(settings) {}

    void setSettings(QSettings *settings) { settings_ = settings; }
    QSettings *settings() const { return settings_; }

    void put(const QString &key, const QVariant &value) override;
    void remove(const QString &key) override;
    QVariant get(const QString &key, const QVariant &default_value) const override;

    void clear() override;
};

class DatabaseInterface {
    Database *db_;
    QString group_;

public:
    DatabaseInterface(Database *db = nullptr)
        : db_(db) {}

    void setDatabase(Database *db) { db_ = db; }
    Database *database() const { return db_; }

    void setGroup(const QString &group);
    QString group() const { return group_; }

    void put(const QString &key, const QVariant &value);
    void remove(const QString &key);
    QVariant get(const QString &key, const QVariant &default_value = QVariant()) const;

    DatabaseInterface subDatabase(const QString &prefix) const;

private:
    QString compositeKey(const QString &key) const;
};

#endif
