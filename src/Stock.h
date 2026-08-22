#pragma once

#include <QMetaType>
#include <QString>

struct Stock {
    QString symbol;
    QString name;
    double price = 0.0;
    double change = 0.0;
    double changePercent = 0.0;
    QString currency;
    QString marketState;
    QString quoteType;
    QString sector;
    QString industry;
    qint64 fetchedAt = 0;

    Stock() = default;
};

Q_DECLARE_METATYPE(Stock)
