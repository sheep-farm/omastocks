#include "YahooFinance.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

#include <cstring>
#include <mutex>

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

static const int REQUEST_TIMEOUT = 30;
static const int BATCH_SIZE = 50;
}

YahooFinance::YahooFinance(QObject *parent)
    : QObject(parent)
{
}

YahooFinance::~YahooFinance()
{
    if (m_share)
        curl_share_cleanup(m_share);
    curl_global_cleanup();
}

void YahooFinance::init()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    m_share = curl_share_init();
    curl_share_setopt(m_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);

    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/omastocks"));
    cacheDir.mkpath(QStringLiteral("."));
    m_cookieJar = cacheDir.absoluteFilePath(QStringLiteral("cookies.txt"));
}

void YahooFinance::refresh(const QStringList &symbols)
{
    QStringList pending;
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

    const CurlResponse crumb = request(QStringLiteral("https://query1.finance.yahoo.com/v1/test/getcrumb"), QStringLiteral("*/*"));
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

CurlResponse YahooFinance::request(const QString &urlString, const QString &accept)
{
    CurlResponse response;

    CURL *curl = curl_easy_init();
    if (!curl) {
        response.error = QStringLiteral("Failed to initialize curl");
        return response;
    }

    const QByteArray url = urlString.toUtf8();
    const QByteArray acceptHeader = (QStringLiteral("Accept: ") + accept).toUtf8();
    struct curl_slist *headers = nullptr;
    if (!accept.isEmpty())
        headers = curl_slist_append(headers, acceptHeader.constData());

    curl_easy_setopt(curl, CURLOPT_URL, url.constData());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long) CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) REQUEST_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, CIPHER_LIST);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_cookieJar.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_cookieJar.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_SHARE, m_share);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    else
        response.error = QString::fromUtf8(curl_easy_strerror(res));

    if (headers)
        curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return response;

    if (response.status != 200)
        response.error = QStringLiteral("HTTP %1").arg(response.status);

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

        const CurlResponse response = request(url.toString());
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

    const CurlResponse response = request(url.toString());
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

size_t YahooFinance::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *buffer = static_cast<QByteArray *>(userdata);
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
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
