#pragma once

#include "Stock.h"
#include "StockListModel.h"
#include "SystemTheme.h"
#include "YahooFinance.h"

#include <QFileSystemWatcher>
#include <QMutex>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QThread;

class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(StockListModel *stocks READ stocks CONSTANT)
    Q_PROPERTY(QVariantMap selectedStock READ selectedStock NOTIFY selectedStockChanged)
    Q_PROPERTY(QVariantList chartValues READ chartValues NOTIFY chartValuesChanged)
    Q_PROPERTY(QString chartSymbol READ chartSymbol NOTIFY chartValuesChanged)
    Q_PROPERTY(QString chartRange READ chartRange WRITE setChartRange NOTIFY chartRangeChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeColorsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

public:
    enum SortMode {
        DefaultSort = 0,
        AlphabeticalSort,
        GainsSort,
        LossesSort
    };
    Q_ENUM(SortMode)
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    StockListModel *stocks() const { return m_model; }
    QVariantMap selectedStock() const { return m_selectedStock; }
    QVariantList chartValues() const { return m_chartValues; }
    QString chartSymbol() const { return m_chartSymbol; }
    QString chartRange() const { return m_chartRange; }
    void setChartRange(const QString &range);
    bool busy() const { return m_busy; }
    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);
    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal textScale);
    QString themeBackground() const { return m_themeBackground; }
    QString themeForeground() const { return m_themeForeground; }
    QString themeAccent() const { return m_themeAccent; }
    QString themeSelection() const { return m_themeSelection; }
    QString error() const { return m_error; }
    QString lastUpdated() const { return m_lastUpdated; }
    void setLastUpdated(const QString &lastUpdated);

    Q_INVOKABLE void addSymbol(const QString &symbol);
    Q_INVOKABLE void removeSymbol(int index);
    Q_INVOKABLE void removeSymbol(const QString &symbol);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectStock(int index);
    Q_INVOKABLE void fetchChart(const QString &symbol, const QString &range = QStringLiteral("5d"));
    Q_INVOKABLE QString chartRangeLabel(const QString &range) const;
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);
    Q_INVOKABLE void sort(int mode);
    Q_INVOKABLE QString sortModeLabel(int mode) const;
    Q_INVOKABLE int sortModeCount() const { return 4; }

    int sortMode() const { return m_sortMode; }
    void setSortMode(int mode);

signals:
    void selectedStockChanged();
    void chartValuesChanged();
    void chartRangeChanged();
    void busyChanged();
    void darkModeChanged();
    void textScaleChanged();
    void themeColorsChanged();
    void errorChanged();
    void sortModeChanged();
    void lastUpdatedChanged();

private:
    void onQuoteUpdated(const Stock &stock);
    void onFetchFinished(bool success, const QString &error);
    void onChartReady(const QString &symbol, const QVariantList &values);
    void setBusy(bool busy);
    void setError(const QString &error);

    void loadWatchlist();
    void saveWatchlist();
    void loadCache();
    void saveCache();
    void startAutoRefresh();
    void stopAutoRefresh();

    void loadOmarchyTheme();
    void watchOmarchyTheme();
    void loadSettings();

    static QString cacheFilePath();
    static QString configPath();
    static bool isCacheFresh(const QJsonObject &stockObject);
    static QVector<Stock> stocksFromJson(const QJsonObject &root);
    static QJsonObject stockToJson(const Stock &stock);
    static QJsonObject cacheRootFromStocks(const QVector<Stock> &stocks);

    StockListModel *m_model;
    YahooFinance *m_yahoo;
    QThread *m_yahooThread;
    QTimer *m_autoRefresh;

    bool m_busy = false;
    bool m_initialRefresh = true;

    bool m_darkMode = true;
    qreal m_textScale = 1.0;
    QString m_themeBackground;
    QString m_themeForeground;
    QString m_themeAccent;
    QString m_themeSelection;
    QFileSystemWatcher m_themeWatcher;

    QVariantMap m_selectedStock;
    QVariantList m_chartValues;
    QString m_chartSymbol;
    QString m_chartRange;
    QString m_error;
    QString m_lastUpdated;
    int m_sortMode = DefaultSort;
};
