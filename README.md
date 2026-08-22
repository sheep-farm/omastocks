# Omastocks

A clean, Apple Stocks-inspired market watchlist for Omarchy, built with Qt Quick and C++.

## Install

Install via the Omarchy Package Repository with the `omastocks` package, or build from source:

```bash
bin/build          # compiles to build/omastocks
bin/install        # installs to ~/.local/bin/omastocks and registers a Hyprland window rule
```

To uninstall later, run `bin/install uninstall`.

## Multi-platform builds

Cross-platform build scripts and a GitHub Actions workflow are included:

- `bin/build` — Linux
- `bin/build-windows` — Windows (MinGW)
- `bin/build-macos` — macOS
- `bin/build-android` — Android

Releases are built automatically by the `Build` workflow in `.github/workflows/build.yml`, which packages artifacts for all four platforms and uploads them to a GitHub Release.

## Usage

Omastocks keeps a watchlist of ticker symbols and refreshes prices from Yahoo Finance.

- Type a ticker in the search bar and press `Enter` or click `+` to add it.
- Tap a stock to see a larger detail view with a 5-day chart.
- Click the `↻` button to refresh manually.
- Hover over a stock and click `×` to remove it.

Prices and percentage changes are color-coded green for gains and red for losses, just like the Apple Stocks app.

## Data source

Yahoo Finance is accessed following the same session/crumb/cookie flow used in **merkato**:

1. Visit `https://fc.yahoo.com/` to seed cookies.
2. Fetch a crumb from `https://query1.finance.yahoo.com/v1/test/getcrumb`.
3. Request batched quotes from `https://query1.finance.yahoo.com/v7/finance/quote`.
4. Request 5-day charts from `https://query1.finance.yahoo.com/v8/finance/chart`.

The session uses a Chrome user agent, a curated TLS cipher list, and a persistent cookie jar to avoid the fingerprint blocks that trip simple requests.

## Configuration

Watchlist and window geometry are persisted in `~/.config/Omacom/omastocks.conf`. Quotes are cached in `~/.cache/omastocks/quotes.json` for five minutes.

Colors follow the current Omarchy theme (`~/.local/state/omarchy/current/theme/colors.toml`) and update live when the theme changes.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`, `qt6-network`
- `xdg-desktop-portal` and a portal backend (Linux desktop)

The iA Writer Mono font is bundled under the SIL Open Font License 1.1; see `fonts/OFL.txt`.
