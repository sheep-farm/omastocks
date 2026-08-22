#include "Backend.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRect>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

namespace {
static const int CACHE_TTL_SECONDS = 300;
static const int REFRESH_INTERVAL_SECONDS = 60;

const QString windowGeometrySetting = QStringLiteral("window/geometry");
const QString windowMaximizedSetting = QStringLiteral("window/maximized");

static QString defaultWatchlist[] = {
    QStringLiteral("AAPL"),
    QStringLiteral("MSFT"),
    QStringLiteral("GOOGL"),
    QStringLiteral("AMZN"),
    QStringLiteral("TSLA"),
    QStringLiteral("NVDA"),
    QStringLiteral("BTC-USD")
};
}

Backend::Backend(QObject *parent)
    : QObject(parent)
{
    m_model = new StockListModel(this);

    m_yahooThread = new QThread(this);
    m_yahoo = new YahooFinance();
    m_yahoo->moveToThread(m_yahooThread);
    connect(m_yahooThread, &QThread::started, m_yahoo, &YahooFinance::init);
    connect(m_yahoo, &YahooFinance::quoteUpdated, this, &Backend::onQuoteUpdated, Qt::QueuedConnection);
    connect(m_yahoo, &YahooFinance::fetchFinished, this, &Backend::onFetchFinished, Qt::QueuedConnection);
    connect(m_yahoo, &YahooFinance::chartReady, this, &Backend::onChartReady, Qt::QueuedConnection);
    connect(m_yahoo, &YahooFinance::busyChanged, this, &Backend::setBusy, Qt::QueuedConnection);
    m_yahooThread->start();

    loadSettings();
    loadOmarchyTheme();
    watchOmarchyTheme();
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });

    loadWatchlist();
    loadCache();
    m_model->sortStocks(m_sortMode);

    m_autoRefresh = new QTimer(this);
    m_autoRefresh->setInterval(REFRESH_INTERVAL_SECONDS * 1000);
    connect(m_autoRefresh, &QTimer::timeout, this, &Backend::refresh);
    startAutoRefresh();

    if (!m_model->symbols().isEmpty())
        QTimer::singleShot(0, this, &Backend::refresh);
}

Backend::~Backend()
{
    stopAutoRefresh();
    m_yahoo->deleteLater();
    m_yahooThread->quit();
    m_yahooThread->wait();
}

void Backend::addSymbol(const QString &symbol)
{
    const QString input = symbol.trimmed().toUpper();
    if (input.isEmpty())
        return;

    const QStringList parts = input.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList added;

    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (trimmed.isEmpty())
            continue;

        if (m_model->symbols().contains(trimmed)) {
            setError(QStringLiteral("%1 is already in the watchlist").arg(trimmed));
            continue;
        }

        clearError();
        Stock placeholder;
        placeholder.symbol = trimmed;
        m_model->updateStock(placeholder);
        added.append(trimmed);
    }

    if (!added.isEmpty()) {
        saveWatchlist();
        refresh();
    }
}

void Backend::removeSymbol(int index)
{
    if (index < 0 || index >= m_model->rowCount())
        return;

    m_model->remove(index);
    saveWatchlist();
    saveCache();
    clearError();
}

void Backend::removeSymbol(const QString &symbol)
{
    const QString trimmed = symbol.trimmed().toUpper();
    const QStringList symbols = m_model->symbols();
    const int index = symbols.indexOf(trimmed);
    if (index >= 0)
        removeSymbol(index);
}

void Backend::refresh()
{
    clearError();
    const QStringList symbols = m_model->symbols();
    if (symbols.isEmpty())
        return;
    QMetaObject::invokeMethod(m_yahoo, "refresh", Qt::QueuedConnection,
                              Q_ARG(QStringList, symbols));
}

void Backend::selectStock(int index)
{
    if (index < 0 || index >= m_model->rowCount())
        return;

    m_selectedStock = m_model->stockAt(index);
    emit selectedStockChanged();

    const QString symbol = m_selectedStock.value(QStringLiteral("symbol")).toString();
    if (!symbol.isEmpty())
        fetchChart(symbol, m_chartRange.isEmpty() ? QStringLiteral("5d") : m_chartRange);
}

void Backend::setChartRange(const QString &range)
{
    const QString normalized = chartRangeLabel(range);
    if (m_chartRange == normalized)
        return;
    m_chartRange = normalized;
    emit chartRangeChanged();
}

void Backend::fetchChart(const QString &symbol, const QString &range)
{
    if (symbol.isEmpty())
        return;
    setChartRange(range);
    m_chartSymbol = symbol;
    m_chartValues.clear();
    emit chartValuesChanged();
    QMetaObject::invokeMethod(m_yahoo, "fetchChart", Qt::QueuedConnection,
                              Q_ARG(QString, symbol),
                              Q_ARG(QString, m_chartRange));
}

QString Backend::chartRangeLabel(const QString &range) const
{
    const QString r = range.toLower();
    if (r == QStringLiteral("1d")) return QStringLiteral("1d");
    if (r == QStringLiteral("5d")) return QStringLiteral("5d");
    if (r == QStringLiteral("1m") || r == QStringLiteral("1mo")) return QStringLiteral("1m");
    if (r == QStringLiteral("6m") || r == QStringLiteral("6mo")) return QStringLiteral("6m");
    if (r == QStringLiteral("ytd")) return QStringLiteral("ytd");
    if (r == QStringLiteral("1y") || r == QStringLiteral("1yr")) return QStringLiteral("1y");
    if (r == QStringLiteral("5y") || r == QStringLiteral("5yr")) return QStringLiteral("5y");
    return QStringLiteral("5d");
}

void Backend::clearError()
{
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }
}

void Backend::onQuoteUpdated(const Stock &stock)
{
    m_model->updateStock(stock);
}

void Backend::onFetchFinished(bool success, const QString &error)
{
    if (success) {
        m_initialRefresh = false;
        clearError();
        saveCache();
        setLastUpdated(QLocale::system().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    } else {
        setError(error);
    }
}

void Backend::onChartReady(const QString &symbol, const QVariantList &values)
{
    m_chartSymbol = symbol;
    m_chartValues = values;
    emit chartValuesChanged();
}

void Backend::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Backend::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void Backend::setLastUpdated(const QString &lastUpdated)
{
    if (m_lastUpdated == lastUpdated)
        return;
    m_lastUpdated = lastUpdated;
    emit lastUpdatedChanged();
}

void Backend::setDarkMode(bool darkMode)
{
    if (m_darkMode == darkMode)
        return;
    m_darkMode = darkMode;
    loadOmarchyTheme();
    emit darkModeChanged();
}

void Backend::setTextScale(qreal textScale)
{
    if (qFuzzyCompare(m_textScale, textScale))
        return;
    m_textScale = textScale;
    emit textScaleChanged();
}

QVariantMap Backend::windowGeometry() const
{
    const QSettings settings("Omacom", "omastocks");
    const QRect geometry = settings.value(windowGeometrySetting).toRect();
    QVariantMap map;
    map[QStringLiteral("valid")] = geometry.isValid();
    map[QStringLiteral("x")] = geometry.x();
    map[QStringLiteral("y")] = geometry.y();
    map[QStringLiteral("width")] = geometry.width();
    map[QStringLiteral("height")] = geometry.height();
    map[QStringLiteral("maximized")] = settings.value(windowMaximizedSetting, false).toBool();
    return map;
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized)
{
    QSettings settings("Omacom", "omastocks");
    settings.setValue(windowGeometrySetting, QRect(x, y, width, height));
    settings.setValue(windowMaximizedSetting, maximized);
}

void Backend::loadWatchlist()
{
    QSettings settings("Omacom", "omastocks");
    QStringList symbols = settings.value(QStringLiteral("watchlist/symbols")).toStringList();
    if (symbols.isEmpty()) {
        for (const QString &symbol : defaultWatchlist)
            symbols.append(symbol);
    }

    for (const QString &symbol : symbols) {
        const QString trimmed = symbol.trimmed().toUpper();
        if (trimmed.isEmpty())
            continue;

        Stock placeholder;
        placeholder.symbol = trimmed;
        m_model->updateStock(placeholder);
    }
}

void Backend::saveWatchlist()
{
    QSettings settings("Omacom", "omastocks");
    settings.setValue(QStringLiteral("watchlist/symbols"), m_model->symbols());
}

void Backend::setSortMode(int mode)
{
    mode = qBound(0, mode, 3);
    if (m_sortMode == mode)
        return;
    m_sortMode = mode;
    m_model->sortStocks(mode);
    QSettings settings("Omacom", "omastocks");
    settings.setValue(QStringLiteral("watchlist/sortMode"), m_sortMode);
    emit sortModeChanged();
}

void Backend::sort(int mode)
{
    setSortMode(mode);
}

QString Backend::sortModeLabel(int mode) const
{
    switch (qBound(0, mode, 3)) {
    case AlphabeticalSort:
        return QStringLiteral("A-Z");
    case GainsSort:
        return QStringLiteral("Gains");
    case LossesSort:
        return QStringLiteral("Losses");
    default:
        return QStringLiteral("Order");
    }
}

void Backend::loadCache()
{
    const QString path = cacheFilePath();
    if (!QFile::exists(path))
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;

    const QJsonObject root = document.object();
    const QJsonObject entries = root.value(QStringLiteral("entries")).toObject();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 latestFetchedAt = 0;

    const QStringList watchedSymbols = m_model->symbols();

    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const QJsonObject stockObject = entry.value(QStringLiteral("stock")).toObject();
        const qint64 fetchedAt = entry.value(QStringLiteral("fetched_at")).toDouble();
        if (now - fetchedAt > CACHE_TTL_SECONDS)
            continue;

        const QString symbol = stockObject.value(QStringLiteral("symbol")).toString();
        if (!watchedSymbols.contains(symbol))
            continue;

        if (fetchedAt > latestFetchedAt)
            latestFetchedAt = fetchedAt;

        Stock stock;
        stock.symbol = stockObject.value(QStringLiteral("symbol")).toString();
        stock.name = stockObject.value(QStringLiteral("name")).toString();
        stock.price = stockObject.value(QStringLiteral("price")).toDouble();
        stock.change = stockObject.value(QStringLiteral("change")).toDouble();
        stock.changePercent = stockObject.value(QStringLiteral("change_percent")).toDouble();
        stock.currency = stockObject.value(QStringLiteral("currency")).toString();
        stock.marketState = stockObject.value(QStringLiteral("market_state")).toString();
        stock.quoteType = stockObject.value(QStringLiteral("quote_type")).toString();
        stock.sector = stockObject.value(QStringLiteral("sector")).toString();
        stock.industry = stockObject.value(QStringLiteral("industry")).toString();
        stock.fetchedAt = fetchedAt;

        m_model->updateStock(stock);
    }

    if (latestFetchedAt > 0)
        setLastUpdated(QLocale::system().toString(QDateTime::fromSecsSinceEpoch(latestFetchedAt), QLocale::ShortFormat));
}

void Backend::saveCache()
{
    QDir().mkpath(QFileInfo(cacheFilePath()).path());

    const QJsonObject root = cacheRootFromStocks(m_model->stocks());
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(data);
}

void Backend::startAutoRefresh()
{
    if (m_autoRefresh)
        m_autoRefresh->start();
}

void Backend::stopAutoRefresh()
{
    if (m_autoRefresh)
        m_autoRefresh->stop();
}

void Backend::loadSettings()
{
    QSettings settings("Omacom", "omastocks");
    m_textScale = settings.value(QStringLiteral("display/textScale"), 1.0).toReal();
    m_sortMode = qBound(0, settings.value(QStringLiteral("watchlist/sortMode"), DefaultSort).toInt(), 3);
    m_chartRange = chartRangeLabel(settings.value(QStringLiteral("chart/range"), QStringLiteral("5d")).toString());
}

void Backend::loadOmarchyTheme()
{
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;
    QFile file(colorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_themeBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = value;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = value;
            else if (key == QStringLiteral("selection"))
                m_themeSelection = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }

    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    emit themeColorsChanged();
}

void Backend::watchOmarchyTheme()
{
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString currentDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_themeWatcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_themeWatcher.addPath(colorsPath);
}

QString Backend::cacheFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/omastocks/quotes.json");
}

QJsonObject Backend::stockToJson(const Stock &stock)
{
    QJsonObject object;
    object[QStringLiteral("symbol")] = stock.symbol;
    object[QStringLiteral("name")] = stock.name;
    object[QStringLiteral("price")] = stock.price;
    object[QStringLiteral("change")] = stock.change;
    object[QStringLiteral("change_percent")] = stock.changePercent;
    object[QStringLiteral("currency")] = stock.currency;
    object[QStringLiteral("market_state")] = stock.marketState;
    object[QStringLiteral("quote_type")] = stock.quoteType;
    object[QStringLiteral("sector")] = stock.sector;
    object[QStringLiteral("industry")] = stock.industry;
    return object;
}

QJsonObject Backend::cacheRootFromStocks(const QVector<Stock> &stocks)
{
    QJsonObject entries;
    for (const Stock &stock : stocks) {
        QJsonObject stockObject = stockToJson(stock);
        QJsonObject entry;
        entry[QStringLiteral("fetched_at")] = stock.fetchedAt;
        entry[QStringLiteral("stock")] = stockObject;
        entries[stock.symbol] = entry;
    }

    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("1.0");
    root[QStringLiteral("entries")] = entries;
    return root;
}
