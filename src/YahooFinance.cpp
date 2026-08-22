#include "YahooFinance.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QMutexLocker>
#include <QNetworkCookieJar>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

#include <cstring>

namespace {
static const char USER_AGENT[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36";

static const char CIPHER_LIST[] =
    "ECDHE-ECDSA-AES128-GCM-SHA256:"
    "ECDHE-RSA-AES128-GCM-SHA256:"
    "ECDHE-ECDSA-AES256-GCM-SHA384:"
    "ECDHE-RSA-AES256-GCM-SHA384:"
    "ECDHE-ECDSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-CHACHA20-POLY1305:"
    "ECDHE-RSA-AES128-SHA:"
    "ECDHE-RSA-AES256-SHA:"
    "AES128-GCM-SHA256:"
    "AES256-GCM-SHA384:"
    "AES128-SHA:"
    "AES256-SHA";

static const int REQUEST_TIMEOUT_MS = 30 * 1000;
static const int BATCH_SIZE = 50;

QNetworkRequest buildRequest(const QUrl &url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", USER_AGENT);
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);

    // Use the curated cipher list whenever the backend actually supports the
    // named ciphers. If none match (common on Android), leave the default
    // system list so the handshake still works.
    if (QSslSocket::supportsSsl()) {
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        const QList<QSslCipher> backendCiphers = QSslConfiguration::supportedCiphers();

        QStringList backendNames;
        for (const QSslCipher &cipher : backendCiphers)
            backendNames.append(cipher.name());

        const QStringList wantedCiphers = QString::fromLatin1(CIPHER_LIST).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        QStringList filteredCiphers;
        for (const QString &name : wantedCiphers) {
            if (backendNames.contains(name))
                filteredCiphers.append(name);
        }

        if (!filteredCiphers.isEmpty()) {
            sslConfig.setCiphers(filteredCiphers.join(QLatin1Char(':')));
            request.setSslConfiguration(sslConfig);
        }
    }

    return request;
}
}

YahooFinance::YahooFinance(QObject *parent)
    : QObject(parent)
{
}

YahooFinance::~YahooFinance() = default;

void YahooFinance::init()
{
    m_manager = new QNetworkAccessManager(this);
    m_manager->setCookieJar(new QNetworkCookieJar(this));
    m_manager->setRedirectPolicy(QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy);

    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/omastocks"));
    cacheDir.mkpath(QStringLiteral("."));
}

void YahooFinance::refresh(const QStringList &symbols)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_busy) {
            m_pendingSymbols = symbols;
            return;
        }
        if (symbols.isEmpty()) {
            lock.unlock();
            emit fetchFinished(true, QString());
            return;
        }
        m_busy = true;
        m_pendingSymbols.clear();
    }
    emit busyChanged(true);
    doRefresh(symbols);
}

void YahooFinance::fetchChart(const QString &symbol, const QString &range)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_busy) {
            m_pendingChartSymbol = symbol;
            m_pendingChartRange = range;
            return;
        }
        m_busy = true;
        m_pendingChartSymbol.clear();
        m_pendingChartRange.clear();
    }
    emit busyChanged(true);
    doFetchChart(symbol, range);
}

bool YahooFinance::primeSession()
{
    if (m_sessionPrimed)
        return true;

    // Seed cookies; the 404 response still sets the session cookie.
    request(QStringLiteral("https://fc.yahoo.com/"), QStringLiteral("*/*"));

    const NetworkResponse crumb = request(QStringLiteral("https://query1.finance.yahoo.com/v1/test/getcrumb"), QStringLiteral("*/*"));
    if (crumb.status != 200 || crumb.body.isEmpty()) {
        m_sessionPrimed = false;
        return false;
    }

    m_crumb = QString::fromUtf8(crumb.body).trimmed();
    if (m_crumb.isEmpty() || m_crumb.length() > 64) {
        m_crumb.clear();
        m_sessionPrimed = false;
        return false;
    }

    m_sessionPrimed = true;
    return true;
}

NetworkResponse YahooFinance::request(const QString &urlString, const QString &accept)
{
    NetworkResponse response;

    if (!m_manager) {
        response.error = QStringLiteral("Network manager not initialized");
        return response;
    }

    QUrl url(urlString);
    QNetworkRequest req = buildRequest(url);
    if (!accept.isEmpty())
        req.setRawHeader("Accept", accept.toUtf8());

    QNetworkReply *reply = m_manager->get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(REQUEST_TIMEOUT_MS);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();

    loop.exec();

    if (!timer.isActive()) {
        reply->abort();
        response.error = QStringLiteral("Request timed out");
        reply->deleteLater();
        return response;
    }

    timer.stop();

    response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        if (response.error.isEmpty())
            response.error = reply->errorString();
    } else if (response.status != 200) {
        response.error = QStringLiteral("HTTP %1").arg(response.status);
    }

    reply->deleteLater();
    return response;
}

void YahooFinance::doRefresh(const QStringList &symbols)
{
    if (!primeSession()) {
        finishWithError(QStringLiteral("Failed to prime Yahoo session"));
        return;
    }

    const QStringList chunks = chunkSymbols(symbols, BATCH_SIZE);
    for (const QString &chunk : chunks) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("symbols"), chunk);
        query.addQueryItem(QStringLiteral("crumb"), m_crumb);

        QUrl url(QStringLiteral("https://query1.finance.yahoo.com/v7/finance/quote"));
        url.setQuery(query);

        const NetworkResponse response = request(url.toString());
        if (response.status != 200) {
            finishWithError(response.error.isEmpty() ? QStringLiteral("Yahoo request failed") : response.error);
            return;
        }

        const QVector<Stock> stocks = parseQuotes(response.body);
        for (const Stock &stock : stocks)
            emit quoteUpdated(stock);
    }

    finishSuccess();
}

void YahooFinance::doFetchChart(const QString &symbol, const QString &range)
{
    if (!primeSession()) {
        finishWithError(QStringLiteral("Failed to prime Yahoo session"));
        return;
    }

    QString yahooRange = range.toLower();
    QString interval = QStringLiteral("1d");

    if (range == QStringLiteral("1d")) {
        yahooRange = QStringLiteral("1d");
        interval = QStringLiteral("15m");
    } else if (range == QStringLiteral("5d")) {
        yahooRange = QStringLiteral("5d");
    } else if (range == QStringLiteral("1m")) {
        yahooRange = QStringLiteral("1mo");
    } else if (range == QStringLiteral("6m")) {
        yahooRange = QStringLiteral("6mo");
    } else if (range == QStringLiteral("ytd")) {
        yahooRange = QStringLiteral("ytd");
    } else if (range == QStringLiteral("1y")) {
        yahooRange = QStringLiteral("1y");
    } else if (range == QStringLiteral("5y")) {
        yahooRange = QStringLiteral("5y");
        interval = QStringLiteral("1wk");
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("interval"), interval);
    query.addQueryItem(QStringLiteral("range"), yahooRange);
    query.addQueryItem(QStringLiteral("crumb"), m_crumb);

    QUrl url(QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/%1").arg(symbol));
    url.setQuery(query);

    const NetworkResponse response = request(url.toString());
    if (response.status != 200) {
        finishWithError(response.error.isEmpty() ? QStringLiteral("Yahoo chart request failed") : response.error);
        return;
    }

    QVariantList values = parseChart(response.body);

    emit chartReady(symbol, values);
    finishSuccess();
}

QVector<Stock> YahooFinance::parseQuotes(const QByteArray &data)
{
    QVector<Stock> result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return result;

    const QJsonObject root = document.object();
    const QJsonObject quoteResponse = root.value(QStringLiteral("quoteResponse")).toObject();
    const QJsonArray quotes = quoteResponse.value(QStringLiteral("result")).toArray();

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const QJsonValue &value : quotes) {
        const QJsonObject quote = value.toObject();
        const QString symbol = quote.value(QStringLiteral("symbol")).toString();
        if (symbol.isEmpty())
            continue;

        const double price = quote.value(QStringLiteral("regularMarketPrice")).toDouble();
        if (price <= 0.0)
            continue;

        Stock stock;
        stock.symbol = symbol;
        stock.price = price;
        stock.change = quote.value(QStringLiteral("regularMarketChange")).toDouble();
        stock.changePercent = quote.value(QStringLiteral("regularMarketChangePercent")).toDouble() / 100.0;
        stock.name = quote.value(QStringLiteral("longName")).toString();
        if (stock.name.isEmpty())
            stock.name = quote.value(QStringLiteral("shortName")).toString();
        stock.currency = quote.value(QStringLiteral("currency")).toString();
        stock.marketState = quote.value(QStringLiteral("marketState")).toString();
        stock.quoteType = quote.value(QStringLiteral("quoteType")).toString();
        stock.sector = quote.value(QStringLiteral("sector")).toString();
        stock.industry = quote.value(QStringLiteral("industry")).toString();
        stock.fetchedAt = now;

        result.append(stock);
    }

    return result;
}

QVariantList YahooFinance::parseChart(const QByteArray &data)
{
    QVariantList result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return result;

    const QJsonObject root = document.object();
    const QJsonObject chart = root.value(QStringLiteral("chart")).toObject();
    const QJsonArray results = chart.value(QStringLiteral("result")).toArray();
    if (results.isEmpty())
        return result;

    const QJsonObject resultObj = results.first().toObject();
    const QJsonObject indicators = resultObj.value(QStringLiteral("indicators")).toObject();

    const QJsonArray quoteArray = indicators.value(QStringLiteral("quote")).toArray();
    if (quoteArray.isEmpty())
        return result;

    const QJsonArray adjcloseArray = indicators.value(QStringLiteral("adjclose")).toArray();
    QJsonArray prices;
    if (!adjcloseArray.isEmpty()) {
        const QJsonObject adjcloseObj = adjcloseArray.first().toObject();
        prices = adjcloseObj.value(QStringLiteral("adjclose")).toArray();
    }

    if (prices.isEmpty()) {
        const QJsonObject quoteObj = quoteArray.first().toObject();
        prices = quoteObj.value(QStringLiteral("close")).toArray();
    }

    for (const QJsonValue &value : prices) {
        if (value.isNull() || !value.isDouble())
            continue;
        result.append(value.toDouble());
    }

    return result;
}

void YahooFinance::finishWithError(const QString &error)
{
    QStringList pending;
    QString pendingChart;
    QString pendingRange;
    {
        QMutexLocker lock(&m_mutex);
        m_busy = false;
        pending = m_pendingSymbols;
        pendingChart = m_pendingChartSymbol;
        pendingRange = m_pendingChartRange;
        m_pendingSymbols.clear();
        m_pendingChartSymbol.clear();
        m_pendingChartRange.clear();
    }

    emit busyChanged(false);
    emit fetchFinished(false, error);

    if (!pending.isEmpty())
        QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection, Q_ARG(QStringList, pending));
    if (!pendingChart.isEmpty())
        QMetaObject::invokeMethod(this, "fetchChart", Qt::QueuedConnection, Q_ARG(QString, pendingChart), Q_ARG(QString, pendingRange));
}

void YahooFinance::finishSuccess()
{
    QStringList pending;
    QString pendingChart;
    QString pendingRange;
    {
        QMutexLocker lock(&m_mutex);
        m_busy = false;
        pending = m_pendingSymbols;
        pendingChart = m_pendingChartSymbol;
        pendingRange = m_pendingChartRange;
        m_pendingSymbols.clear();
        m_pendingChartSymbol.clear();
        m_pendingChartRange.clear();
    }

    emit busyChanged(false);
    emit fetchFinished(true, QString());

    if (!pending.isEmpty())
        QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection, Q_ARG(QStringList, pending));
    if (!pendingChart.isEmpty())
        QMetaObject::invokeMethod(this, "fetchChart", Qt::QueuedConnection, Q_ARG(QString, pendingChart), Q_ARG(QString, pendingRange));
}

QStringList YahooFinance::chunkSymbols(const QStringList &symbols, int chunkSize)
{
    QStringList chunks;
    QStringList current;
    for (const QString &symbol : symbols) {
        current.append(symbol);
        if (current.size() >= chunkSize) {
            chunks.append(current.join(QLatin1Char(',')));
            current.clear();
        }
    }
    if (!current.isEmpty())
        chunks.append(current.join(QLatin1Char(',')));
    return chunks;
}
