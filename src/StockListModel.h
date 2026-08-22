#pragma once

#include "Stock.h"

#include <QAbstractListModel>
#include <QJsonObject>
#include <QVariantMap>
#include <QVector>

class StockListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit StockListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<Stock> &stocks() const { return m_stocks; }
    QStringList symbols() const;

    Q_INVOKABLE void updateStock(const Stock &stock);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap stockAt(int row) const;
    Q_INVOKABLE void sortStocks(int mode);

    void setStocks(const QVector<Stock> &stocks);

    enum SortMode {
        DefaultSort = 0,
        AlphabeticalSort,
        GainsSort,
        LossesSort
    };
    Q_ENUM(SortMode)

    enum Roles {
        SymbolRole = Qt::UserRole + 1,
        NameRole,
        PriceRole,
        FormattedPriceRole,
        ChangeRole,
        FormattedChangeRole,
        ChangePercentRole,
        FormattedChangePercentRole,
        CurrencyRole,
        CurrencySymbolRole,
        MarketStateRole,
        IsGainingRole,
        IsLosingRole,
        QuoteTypeRole,
        SectorRole,
        IndustryRole
    };
    Q_ENUM(Roles)

private:
    QString formatPrice(const Stock &stock) const;
    QString formatChange(const Stock &stock) const;
    QString formatChangePercent(const Stock &stock) const;
    static QString currencySymbolFor(const QString &currency);
    void applySort();
    int findStockIndex(const QString &symbol) const;

    QVector<Stock> m_stocks;
    QStringList m_symbols;
    SortMode m_sortMode = DefaultSort;
};
