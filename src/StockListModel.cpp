#include "StockListModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <algorithm>

StockListModel::StockListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int StockListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_stocks.size();
}

QVariant StockListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_stocks.size())
        return QVariant();

    const Stock &stock = m_stocks.at(index.row());
    switch (role) {
    case SymbolRole:
        return stock.symbol;
    case NameRole:
        return stock.name;
    case PriceRole:
        return stock.price;
    case FormattedPriceRole:
        return formatPrice(stock);
    case ChangeRole:
        return stock.change;
    case FormattedChangeRole:
        return formatChange(stock);
    case ChangePercentRole:
        return stock.changePercent;
    case FormattedChangePercentRole:
        return formatChangePercent(stock);
    case CurrencyRole:
        return stock.currency;
    case CurrencySymbolRole:
        return currencySymbolFor(stock.currency);
    case MarketStateRole:
        return stock.marketState;
    case IsGainingRole:
        return stock.change > 0.0;
    case IsLosingRole:
        return stock.change < 0.0;
    case QuoteTypeRole:
        return stock.quoteType;
    case SectorRole:
        return stock.sector;
    case IndustryRole:
        return stock.industry;
    }

    return QVariant();
}

QHash<int, QByteArray> StockListModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[SymbolRole] = "symbol";
    names[NameRole] = "name";
    names[PriceRole] = "price";
    names[FormattedPriceRole] = "formattedPrice";
    names[ChangeRole] = "change";
    names[FormattedChangeRole] = "formattedChange";
    names[ChangePercentRole] = "changePercent";
    names[FormattedChangePercentRole] = "formattedChangePercent";
    names[CurrencyRole] = "currency";
    names[CurrencySymbolRole] = "currencySymbol";
    names[MarketStateRole] = "marketState";
    names[IsGainingRole] = "isGaining";
    names[IsLosingRole] = "isLosing";
    names[QuoteTypeRole] = "quoteType";
    names[SectorRole] = "sector";
    names[IndustryRole] = "industry";
    return names;
}

QStringList StockListModel::symbols() const
{
    return m_symbols;
}

int StockListModel::findStockIndex(const QString &symbol) const
{
    for (int i = 0; i < m_stocks.size(); ++i) {
        if (m_stocks[i].symbol == symbol)
            return i;
    }
    return -1;
}

void StockListModel::updateStock(const Stock &stock)
{
    const int indexInStocks = findStockIndex(stock.symbol);
    if (indexInStocks >= 0) {
        m_stocks[indexInStocks] = stock;
        emit dataChanged(index(indexInStocks), index(indexInStocks));
        return;
    }

    if (!m_symbols.contains(stock.symbol))
        m_symbols.append(stock.symbol);

    beginInsertRows(QModelIndex(), m_stocks.size(), m_stocks.size());
    m_stocks.append(stock);
    endInsertRows();
    applySort();
}

void StockListModel::remove(int row)
{
    if (row < 0 || row >= m_stocks.size())
        return;

    const QString symbol = m_stocks.at(row).symbol;
    m_symbols.removeAll(symbol);

    beginRemoveRows(QModelIndex(), row, row);
    m_stocks.removeAt(row);
    endRemoveRows();
}

void StockListModel::clear()
{
    if (m_stocks.isEmpty())
        return;
    beginResetModel();
    m_stocks.clear();
    endResetModel();
}

QVariantMap StockListModel::stockAt(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_stocks.size())
        return map;

    const Stock &stock = m_stocks.at(row);
    map[QStringLiteral("symbol")] = stock.symbol;
    map[QStringLiteral("name")] = stock.name;
    map[QStringLiteral("price")] = stock.price;
    map[QStringLiteral("formattedPrice")] = formatPrice(stock);
    map[QStringLiteral("change")] = stock.change;
    map[QStringLiteral("formattedChange")] = formatChange(stock);
    map[QStringLiteral("changePercent")] = stock.changePercent;
    map[QStringLiteral("formattedChangePercent")] = formatChangePercent(stock);
    map[QStringLiteral("currency")] = stock.currency;
    map[QStringLiteral("currencySymbol")] = currencySymbolFor(stock.currency);
    map[QStringLiteral("marketState")] = stock.marketState;
    map[QStringLiteral("isGaining")] = (stock.change > 0.0);
    map[QStringLiteral("isLosing")] = (stock.change < 0.0);
    map[QStringLiteral("quoteType")] = stock.quoteType;
    map[QStringLiteral("sector")] = stock.sector;
    map[QStringLiteral("industry")] = stock.industry;
    return map;
}

void StockListModel::setStocks(const QVector<Stock> &stocks)
{
    beginResetModel();
    m_stocks = stocks;
    m_symbols.clear();
    m_symbols.reserve(stocks.size());
    for (const Stock &stock : stocks)
        m_symbols.append(stock.symbol);
    endResetModel();
    applySort();
}

void StockListModel::sortStocks(int mode)
{
    m_sortMode = static_cast<SortMode>(mode);
    applySort();
}

void StockListModel::applySort()
{
    if (m_stocks.size() < 2)
        return;

    switch (m_sortMode) {
    case DefaultSort:
        std::sort(m_stocks.begin(), m_stocks.end(), [this](const Stock &a, const Stock &b) {
            return m_symbols.indexOf(a.symbol) < m_symbols.indexOf(b.symbol);
        });
        break;
    case AlphabeticalSort:
        std::sort(m_stocks.begin(), m_stocks.end(), [](const Stock &a, const Stock &b) {
            return a.symbol.compare(b.symbol, Qt::CaseInsensitive) < 0;
        });
        break;
    case GainsSort:
        std::sort(m_stocks.begin(), m_stocks.end(), [](const Stock &a, const Stock &b) {
            if (a.changePercent == b.changePercent)
                return a.symbol.compare(b.symbol, Qt::CaseInsensitive) < 0;
            return a.changePercent > b.changePercent;
        });
        break;
    case LossesSort:
        std::sort(m_stocks.begin(), m_stocks.end(), [](const Stock &a, const Stock &b) {
            if (a.changePercent == b.changePercent)
                return a.symbol.compare(b.symbol, Qt::CaseInsensitive) < 0;
            return a.changePercent < b.changePercent;
        });
        break;
    }

    emit dataChanged(index(0), index(m_stocks.size() - 1));
}

QString StockListModel::formatPrice(const Stock &stock) const
{
    const QString sym = stock.symbol.startsWith(QLatin1Char('^')) ? QString() : currencySymbolFor(stock.currency);
    int decimals = 2;
    if (stock.price > 0.0 && stock.price < 1.0)
        decimals = qBound(2, qRound(-std::log10(stock.price)) + 3, 8);
    return sym + QLocale::c().toString(stock.price, 'f', decimals);
}

QString StockListModel::formatChange(const Stock &stock) const
{
    const QString sym = stock.symbol.startsWith(QLatin1Char('^')) ? QString() : currencySymbolFor(stock.currency);
    const QString sign = stock.change >= 0.0 ? QStringLiteral("+") : QString();
    return sym + sign + QLocale::c().toString(stock.change, 'f', 2);
}

QString StockListModel::formatChangePercent(const Stock &stock) const
{
    const double pct = stock.changePercent * 100.0;
    const QString sign = pct >= 0.0 ? QStringLiteral("+") : QString();
    return sign + QLocale::c().toString(pct, 'f', 2) + QStringLiteral("%");
}

QString StockListModel::currencySymbolFor(const QString &currency)
{
    if (currency.isEmpty())
        return QString();
    return currency + QStringLiteral(" ");
}
