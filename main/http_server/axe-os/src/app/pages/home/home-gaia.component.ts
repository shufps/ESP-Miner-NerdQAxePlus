import { ChangeDetectionStrategy, Component, inject } from '@angular/core';
import { Observable } from 'rxjs';

import { ISystemV2 } from '../../models/ISystemV2';
import { SystemService } from '../../services/system.service';
import { HOME_CFG } from './home.cfg';
import { HomeComponent } from './home.component';
import { toPct } from './tiles/utils';

/**
 * Dashboard for the "Gaia" theme (NerdAxeGaia).
 *
 * Inherits all data handling (polling, chart, history, lifecycle) from
 * HomeComponent and only brings its own template, styles and the helpers its
 * layout needs. HomeShellComponent decides which of the two dashboards is shown,
 * so the default dashboard stays free of Gaia-specific code.
 */
@Component({
  selector: 'app-home-gaia',
  templateUrl: './home-gaia.component.html',
  styleUrls: ['./home.component.scss', './home-gaia.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class HomeGaiaComponent extends HomeComponent {
  // Gaia gold instead of the default purple for the hashrate chart and bars
  protected override get hashrateColor(): string {
    return '#E8B23C';
  }

  private readonly sysService = inject(SystemService);

  // Wi-Fi details, device model and firmware version for the uptime card and footer
  public sysInfo$: Observable<ISystemV2> = this.sysService.getSystemV2();

  /**
   * stroke-dasharray for a circular gauge ring (r=42 => circumference ~263.9).
   * Returns "<filled> <circumference>" so the colored arc fills `pct` of the ring.
   */
  public gaugeArc(value: number, min: number, max: number): string {
    const C = 263.9;
    const SPAN = C * 0.75; // 270° arc (a quarter is left open at the bottom)
    const pct = Math.max(0, Math.min(100, toPct(value, min, max)));
    return `${((pct / 100) * SPAN).toFixed(1)} ${C}`;
  }

  /**
   * Color level for a gauge based on how FULL it is (its fill %):
   *   < 70%  -> 'ok'   (green)
   *   70-90% -> 'warn' (yellow)
   *   >= 90% -> 'crit' (red)
   */
  public gaugeLevel(value: number, min: number, max: number): 'ok' | 'warn' | 'crit' {
    const pct = Math.max(0, Math.min(100, toPct(value, min, max)));
    if (pct >= 90) return 'crit';
    if (pct >= 70) return 'warn';
    return 'ok';
  }

  // ── NEROQ+ dashboard helpers (gaia) ────────────────────────────────────────
  public clamp100(n: number): number { return Math.max(0, Math.min(100, Number(n) || 0)); }
  public pctOf(a: number, b: number): number { const d = Number(b) || 0; return d ? (Number(a) / d) * 100 : 0; }
  public effLabel(j: number): string { const v = Number(j) || 0; if (v <= 0) return '—'; if (v < 25) return 'Good'; if (v < 40) return 'Fair'; return 'High'; }

  // SVG sparklines: deterministic gentle waves (no jitter on change-detection).
  // `seed` varies the shape per metric; width/height match the SVG viewBox.
  private static readonly SPARK_W = 90;
  private static readonly SPARK_H = 28;
  public get sparkW(): number { return HomeGaiaComponent.SPARK_W; }
  public get sparkH(): number { return HomeGaiaComponent.SPARK_H; }

  private sparkSeries(seed: number): number[] {
    const N = 26;
    const out: number[] = [];
    for (let i = 0; i < N; i++) {
      const v = 0.5 + 0.30 * Math.sin(i * 0.72 + seed) + 0.12 * Math.sin(i * 1.9 + seed * 1.7);
      out.push(Math.max(0.08, Math.min(0.92, v)));
    }
    return out;
  }

  /** Polyline points for a sparkline. */
  public sparkPoints(seed: number): string {
    const w = HomeGaiaComponent.SPARK_W, h = HomeGaiaComponent.SPARK_H, pad = 2;
    const s = this.sparkSeries(seed);
    return s.map((v, i) => {
      const x = (i / (s.length - 1)) * w;
      const y = h - (v * (h - pad * 2) + pad);
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    }).join(' ');
  }

  /** Closed polygon points for the soft area fill under a sparkline. */
  public sparkArea(seed: number): string {
    const w = HomeGaiaComponent.SPARK_W, h = HomeGaiaComponent.SPARK_H;
    return `0,${h} ${this.sparkPoints(seed)} ${w},${h}`;
  }

  /**
   * stroke-dasharray for a SEMICIRCULAR mini gauge (180° arc, r=43 in the SVG).
   * Fills the arc from the min end up to the value's position in [min,max].
   */
  public halfGaugeArc(value: number, min: number, max: number): string {
    const C = Math.PI * 43; // semicircle arc length (≈ 135.09)
    const pct = Math.max(0, Math.min(100, toPct(value, min, max)));
    return `${((pct / 100) * C).toFixed(1)} ${C.toFixed(1)}`;
  }

  // Chart time-range buttons (NEROQ+ header). Only ranges inside the chart zoom
  // limits are offered: the firmware keeps at most 3 h of dashboard history, and
  // longer windows would be clamped to that while the button still showed e.g. "1D".
  private static readonly RANGE_MS: Record<string, number> = {
    '1H': 3_600_000, '2H': 7_200_000, '3H': 10_800_000,
  };
  public chartRanges: string[] = Object.keys(HomeGaiaComponent.RANGE_MS).filter((r) => {
    const ms = HomeGaiaComponent.RANGE_MS[r];
    return ms >= HOME_CFG.xAxis.minWindowMs && ms <= HOME_CFG.xAxis.maxWindowMs;
  });
  public chartRange: string = '1H';
  public setChartRange(label: string): void {
    this.chartRange = label;
    const ms = HomeGaiaComponent.RANGE_MS[label];
    if (ms) { try { this.setChartWindowMs(ms); } catch { /* clamp / ignore */ } }
  }
}
