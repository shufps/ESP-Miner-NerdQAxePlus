# Experimental – Structure Refactor

**Target image:** `HomeExperimentalComponent` is essentially just glue code:
- Start and stop polling/history drain
- Fill `chartState` with new data
- Trigger `chart.update()`
- UI interaction (view mode, legend click, tooltip wiring)

## Activate Experimental Mode

The switch between Legacy and Experimental is deliberately done **outside** the home templates so that:
- Legacy is not affected by Experimental bindings (template type check)
- Experimental only runs when it is actually enabled (no background subscriptions)

Routing:
- `pages-routing.module.ts` routes `/pages/home` to `HomeShellComponent`.
- `HomeShellComponent` decides whether to render `app-home-experimental` or `app-home` based on `ExperimentalDashboardService.enabled$`.

File: `home/home-shell.component.html`
```html
<ng-container *ngIf=“(experimental.enabled$ | async); else normal”>
  <app-home-experimental></app-home-experimental>
</ng-container>
<ng-template #normal>
  <app-home></app-home>
</ng-template>
```

## Folder structure

```
home/
  home.component.ts                 # “Legacy”
  home.component.experimental.ts    # Experimental (Glue)
  home.component.html               # Shared Template (neutral, without experimental-specific bindings)
  home.component.scss
  home-shell.component.ts
  home-shell.component.html
  
chart/
    index.ts                        # Barrel exports
    math.ts                         # Small helpers (median, clamp, findLastFinite)
    home.chart-state.ts             # Pure data storage + sanitize/trim/persist mapping
    home.chart-storage.ts           # LocalStorage wrapper (incl. versioning/migration)
    home.history-drain.ts           # Drain loop + throttled render
    home.info-polling.ts            # Polling pipeline (RxJS helper)
    home.chart.factory.ts           # Chart.js setup (create data/options)
    home.chart.theme.ts             # Theme/CSS variables -> Chart options
    home.chart.datasets.ts          # Dataset definitions (colors, order, styles)
    home.debug-hooks.ts             # __nerdCharts wiring
    home.graph-guard.ts             # Heuristics/Spike Guard
    home.axis-scale.ts              # Axis bounds + “nice ticks” + apply helper

plugins/
    index.ts                        # registerHomeChartPlugins()
    value-pills.plugin.ts           # Chart.js Plugin: Value Pills
```

## Where do you change what?

- **Datasets / Look & Feel**: `chart/home.chart.datasets.ts`
  - Colors, lines, dash pattern, order, which series exist.
- **Theme**: `chart/home.chart.theme.ts`
  - Reading CSS variables, grid/text colors, theme updates.
- **Chart options & rendering**: `chart/home.chart.factory.ts`
  - Tooltip, legend handling, general Chart.js options.
- **Value Pills**: `plugins/value-pills.plugin.ts`
  - Plugin can be tested completely separately.
- **Data Storage/Transformation**: `chart/home.chart-state.ts`
  - Arrays, append/trim/validate, toPersisted/fromPersisted.
- **Persistence**: `chart/home.chart-storage.ts`
  - Keys, versioning, migration, normalization.
- **Heuristics**: `chart/home.graph-guard.ts`
  - Spike/step filter (encapsulates state + live-ref stability).
- **Axis Scaling**: `chart/home.axis-scale.ts`
  - min/max from window, padding, nice steps, apply helper.
- **Flow control**:
  - `chart/home.history-drain.ts` (hasMore drain, throttled render)
  - `chart/home.info-polling.ts` (polling observable)
- **Debug**: `chart/home.debug-hooks.ts`
  - Global commands (`__nerdCharts`) and storage keys.

## Persistence schema (important for debugging)

`chart/home.chart-storage.ts` defines the keys (default):
- `chartData_exp`
- `lastTimestamp_exp`
- `tempViewMode_exp`
- `chartLegendVisibility_exp`
- `minHistoryTimestampMs_exp`

`chartData_exp` is **versioned** (Envelope v2):
- `v: 2` (Envelope version)
- `ts: <persisted at>`
- `state: <PersistedHomeChartStateV1>`

When loading, best-effort migration is performed (v0/v1 shapes are accepted) and capped at a maximum of 20,000 points.

## Debug Hooks

`chart/home.debug-hooks.ts` installs `globalThis.__nerdCharts`.

### Enable hooks
Some builds only install the lightweight bootstrap. In that case, run:

```js
__nerdCharts.enable(true)  // persist across reloads
// or:
__nerdCharts.enable()
```

You can inspect available commands via:

```js
__nerdCharts.list()
__nerdCharts.help()
```

### Useful commands

```js
// Clear history (in-memory, immediate)
__nerdCharts.clearChartHistoryNow()

// Clear history once (persists a flag + seeds timestamps; reload to apply)
__nerdCharts.clearChartHistoryOnce(); location.reload()

// Force a render tick of the history-drain loop
__nerdCharts.flushHistoryDrainRender()

// Restart device (optional hook; may require a TOTP depending on backend configuration)
__nerdCharts.restart()
__nerdCharts.restart("123456")
```

### Storage keys (typical)
- `__nerdCharts_debugMode` (debug active)
- `__nerdCharts_axisPaddingOverrideEnabled` / `__nerdCharts_axisPadding` (axis padding overrides)
- `__nerdCharts_clearChartHistoryOnce` (delete once)

## Central tuning point: `home.cfg.ts`

The file is located in the home folder (typically in the repo: `src/app/pages/home/home.cfg.ts`).

`home.cfg.ts` is the **single source of truth** for all settings that only affect the **Experimental Dashboard**.

**Important:** `home.cfg.ts` does **not** affect whether Experimental is active. The **Legacy vs Experimental** switch is controlled exclusively via **Settings/UI** (via `ExperimentalDashboardService.enabled$` in `HomeShell`).

What you will find centrally in `home.cfg.ts`:

- **Engineering Defaults**: heuristics, axes/padding, ticks, smoothing, drain behavior
- **UI Defaults**: start values for ViewMode/Legend, *if nothing is saved yet*
- **Storage Keys**: all LocalStorage keys in one place (so that renaming/migrations do not happen in a scattered manner)

The goal is: Changes to behavior are **not** distributed in `home.component.experimental.ts`, but controlled via `HOME_CFG`.

### What can be configured in `home.cfg.ts`?

The defaults are logically grouped in `HOME_CFG`. Here is a practical overview (including “where is it used?”):

**Axis / Visual**
- `HOME_CFG.axisPadding.hashrate` / `HOME_CFG.axisPadding.temp`

- adaptive padding rules so that the lines don't “stick” to the frame
  - used in: `chart/home.axis-scale.ts`
- `HOME_CFG.yAxis.hashrateTickCountClamp`
- clamp for debug/inputs (previously hardcoded `2..30`)

- used in: `home.component.experimental.ts` (tick count setter/debug)
- `HOME_CFG.yAxis.minTickSteps`
- minimum tick steps (more stable axis for small ranges)
- used in: `chart/home.axis-scale.ts`

**Axis / X viewport**
- `HOME_CFG.xAxis.fixedWindowMs`
- Implemented time window for the X axis (milliseconds). Keeps the viewport stable (e.g. always show the last 1h), even if there are only a few points.
- Used in: `home.component.experimental.ts` (sets `scales.x.min/max`), `chart/home.axis-scale.ts` (bounds computed for the active window), `chart/home.chart-state.ts` (trim history to the same window)

- `HOME_CFG.xAxis.tickStepMs`
- Tick spacing for the time axis (milliseconds). Controls the generated tick marks/labels (e.g. 15-minute labels `:00, :15, :30, :45`) without shifting the viewport end.
- Used in: `chart/home.chart.factory.ts`

- `HOME_CFG.colors.chartGridColor`
- Gridline color of the chart. Default: `#80808040`.
- Used in: `chart/home.chart.factory.ts`, `chart/home.chart.theme.ts`

- `HOME_CFG.colors.textFallback`
- Fallback text color if the CSS variable `--card-text-color` is not yet available (e.g., during early boot).
- Used in: `chart/home.chart.theme.ts`

- `HOME_CFG.colors.hashrateBase`
- Base color for the hashrate series (1m/10m/1h).
- Used in: `chart/home.chart.datasets.ts`

- `HOME_CFG.colors.vregTemp` / `HOME_CFG.colors.asicTemp`
- Line/fill colors for the temperature series.
- Used in: `chart/home.chart.datasets.ts`

- `HOME_CFG.colors.pillsText` / `HOME_CFG.colors.pillsDebugStroke`
- Default text color and debug outline for value pills.
- Used in: `plugins/value-pills.plugin.ts`

**Axis / “latest” views**
- `HOME_CFG.tempScale.latestPadC`
- Temperature axis around +/- X°C around the latest values (previously hardcoded `3`)
- Used in: `home.component.experimental.ts` (latest temp min/max)

**GraphGuard (spike/step heuristics)**
- `HOME_CFG.graphGuard.cfg`
- Confirm samples, live ref tolerance, “big steps,” live ref stability
- Used in: `chart/home.graph-guard.ts` (configure)
- `HOME_CFG.graphGuard.thresholds`
- `relThreshold` per series (previously hardcoded `0.01/0.02/0.08/0.10/0.35`)
  - used in: `home.component.experimental.ts` (apply per series)
- `HOME_CFG.graphGuard.enableHashrateSpikeGuard`
- global kill switch for hashrate guard
- used in: `home.component.experimental.ts`

**History Drain**
- `HOME_CFG.historyDrain.renderThrottleMs`
- `HOME_CFG.historyDrain.useThrottledRender`
- `HOME_CFG.historyDrain.suppressChartUpdatesDuringDrain`
- `HOME_CFG.historyDrain.chunkSize`
- used in: `chart/home.history-drain.ts` + wiring in `home.component.experimental.ts`

**Smoothing (rendering only)**
- `HOME_CFG.smoothing.hashrate1m`
- Curve tension based on point density (no data change)
- used in: `home.component.experimental.ts` (dataset tension/cubicInterpolation)

**Temp scale (“latest” zoom)**
The `latest` view deliberately uses a fixed band around the latest measurement value to keep small changes visible.
The band can be adjusted via `HOME_CFG.tempScale.latestPadC`.

**Warmup / Restart gating**
These knobs control when specific series are allowed to appear after a miner restart (to avoid “half-valid” ramps and visual chaos).
- `HOME_CFG.warmup.tempMinValidC` / `HOME_CFG.warmup.tempMaxValidC`
  - Plausibility bounds used to decide whether temperature samples count as “valid” for warmup sequencing.
  - Used in: `home.warmup.ts`
- `HOME_CFG.warmup.vregDelayMs` / `HOME_CFG.warmup.asicDelayMs` / `HOME_CFG.warmup.hash1mDelayMs`
  - Delays (ms) between warmup stages before enabling the next plot (VR temp → ASIC temp → HR 1m).
  - Used in: `home.warmup.ts`, `home.component.experimental.ts` (wiring)
- `HOME_CFG.warmup.restartDetectStreak`
  - How many consecutive “boot-like” polls are required before we treat the situation as a restart.
  - Used in: `home.warmup.ts`

**Sanitizing (raw sample → plot)**
Invalid samples become `NaN` which produces a gap instead of a misleading line.
- `HOME_CFG.sanitize.tempMinC` / `HOME_CFG.sanitize.tempMaxC`
- `HOME_CFG.sanitize.hashrateMinHs`
- Used in: `home.component.experimental.ts` (sanitizeLoadedHistory + live update path)

**Startup / GraphGuard behavior**
Tuning for how we treat hashrate steps/spikes right after a restart.
- `HOME_CFG.startup.bypassGuardSamples`
  - Number of initial samples per series that can bypass GraphGuard once startup is “unlocked”.
- `HOME_CFG.startup.expectedUnlockRatio`
  - Startup unlock ratio: live must reach `expected * ratio` before bypassing is allowed.
- `HOME_CFG.startup.hr1mSmoothWindowMs`
  - Length of the post-restart “super smooth” window for the 1m series (ms).
- `HOME_CFG.startup.hr1mConfirmStartup` / `HOME_CFG.startup.hr1mConfirmNormal`
  - GraphGuard confirmSamples used during the smooth window vs. normal running.
- `HOME_CFG.startup.hr1mReloadAfterSmooth`
  - Optional one-time auto-reload after the smooth window ends (only after a detected restart).
- `HOME_CFG.startup.hr1mReloadCooldownMs`
  - Session-scoped cooldown to prevent reload loops.
- Used in: `home.component.experimental.ts`, `chart/home.graph-guard.ts`, `home.warmup.ts`

**UI Defaults + Persistence (experimental only)**
- `HOME_CFG.uiDefaults.viewMode` / `HOME_CFG.uiDefaults.legendHidden`
- Default UI state when nothing has been saved yet
- Used in: `chart/home.chart-storage.ts` (load defaults)
- `HOME_CFG.storage.keys.*`
- Central LocalStorage keys (we deliberately keep the existing keys so as not to lose any data)
- Used in: `chart/home.chart-storage.ts`

### How do I add a new knob?

1) **Add it to `home.cfg.ts`** (including JSDoc: *What does it do? Which unit? Which range makes sense?*)
2) **Use it in the appropriate file** (Component/AxisScale/GraphGuard/Drain/etc.)
3) If it should be persisted:
- Add key under `HOME_CFG.storage.keys.*`
- Add load/save in `chart/home.chart-storage.ts`
4) Briefly add to README (this section is intended as an index)

### How do I use `home.cfg.ts` in practice?

**If you want to change “the behavior” (engineering):**
- Axes/padding: `HOME_CFG.axisPadding.*` and `HOME_CFG.yAxis.*`
- Spike filter reactivity: `HOME_CFG.graphGuard.cfg` + `HOME_CFG.graphGuard.thresholds.*`
- History loading feel: `HOME_CFG.historyDrain.*`
- “Smoother” graph (visual only): `HOME_CFG.smoothing.hashrate1m.*`

**If you want to change defaults for new installations (UI default):**
- `HOME_CFG.uiDefaults.viewMode`
- `HOME_CFG.uiDefaults.legendHidden`

**If you need to change keys:**
- `HOME_CFG.storage.keys.*`
  - Warning: Changing keys means that existing users will “lose” their stored data/views (it acts like a reset). If you rename keys, plan for migration.

### Guidelines (to keep it stable)

- **Tuning values** (thresholds, padding, steps) can change without migration.
- **Persisted schema** (e.g., `chartData_exp` format) can only be changed with a version bump + migration.
- Never change **keys** if possible (or migrate them deliberately).

### Why no toggle in the home template?

`home.component.html` is used by Legacy and Experimental. Angular checks bindings statically during the build.
If the template contains Experimental-specific bindings (`debugMode`, etc.), the Legacy build breaks.

Therefore:
- Switch happens in **HomeShell** (via Settings/Service)
- `home.cfg.ts` is the central location for Experimental tuning
