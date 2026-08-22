#pragma once

#include "Stock.h"

#include <QJsonDocument>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QStringList>
#include <QVariantList>

struct NetworkResponse {
    QByteArray body;
    int status = 0;
    QString error;
};

class YahooFinance : public QObject {
    Q_OBJECT

public:
    explicit YahooFinance(QObject *parent = nullptr);
    ~YahooFinance() override;

public slots:
    void init();
    void refresh(const QStringList &symbols);
    void fetchChart(const QString &symbol, const QString &range = QStringLiteral("5d"));

signals:
    void quoteUpdated(const Stock &stock);
    void fetchFinished(bool success, const QString &error);
    void chartReady(const QString &symbol, const QVariantList &values);
    void busyChanged(bool busy);

private:
    bool primeSession();
    NetworkResponse request(const QString &url, const QString &accept = QStringLiteral("application/json"));
    void doRefresh(const QStringList &symbols);
    void doFetchChart(const QString &symbol, const QString &range);
    QVector<Stock> parseQuotes(const QByteArray &data);
    QVariantList parseChart(const QByteArray &data);
    void finishWithError(const QString &error);
    void finishSuccess();

    static QStringList chunkSymbols(const QStringList &symbols, int chunkSize);

    QNetworkAccessManager *m_manager = nullptr;
    QString m_crumb;
    bool m_sessionPrimed = false;
    bool m_busy = false;
    QMutex m_mutex;
    QStringList m_pendingSymbols;
    QString m_pendingChartSymbol;
    QString m_pendingChartRange;
};
