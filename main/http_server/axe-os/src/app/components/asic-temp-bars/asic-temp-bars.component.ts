import { ChangeDetectionStrategy, Component, Input } from '@angular/core';

@Component({
    selector: 'app-asic-temp-bars',
    changeDetection: ChangeDetectionStrategy.OnPush,
    template: `
    <div class="bars-container" [style.width.px]="width" [style.height.px]="height">
      <svg class="bars-svg" viewBox="0 0 100 90" preserveAspectRatio="xMidYMid meet" aria-label="ASIC temperatures">
        <!-- Background rail -->
        <g *ngFor="let x of barXs; let i = index">
          <title>ASIC {{ i }}: {{ safeTemp(i) | number: hoverFormat }}{{ unit }}</title>

          <!-- Background column -->
          <rect
            class="bar-bg"
            [attr.x]="x"
            [attr.y]="topPadding"
            [attr.width]="barWidth"
            [attr.height]="plotHeight"
            rx="2" ry="2"
          ></rect>

          <!-- Value label above each bar -->
          <text
            class="value-text"
            [attr.x]="x + barWidth/2"
            [attr.y]="valueTextY(i)"
            text-anchor="middle"
          >
            {{ safeTemp(i) | number: valueFormat }}
          </text>

          <!-- Foreground fill (the actual value) -->
          <rect
            class="bar-fg"
            [attr.x]="x"
            [attr.width]="barWidth"
            [attr.y]="barY(i)"
            [attr.height]="barH(i)"
            rx="2" ry="2"
            [attr.fill]="barColor(i)"
          ></rect>

          <!-- Chip label under each bar -->
          <text
            class="chip-label"
            [attr.x]="x + barWidth/2"
            [attr.y]="76"
            text-anchor="middle"
          >
            {{ chipLabel(i + 1) }}
          </text>
        </g>
      </svg><!--<span class="container-label">Chip Temps {{ unit }}</span>-->
      <span class="container-label">ASIC Temp</span>
    </div>
  `,
    styles: [`
    /* Container keeps things compact and theme-friendly */
    .bars-container {
      display: inline-block;
      position: relative;
      top: -5px;
      font-size: 14px;
      /* Nebular-ish surface */
      color: var(--card-text-color, #cfd8dc);
      height: 100px !important;
      margin: 0px;
    }

    .bars-svg {
      width: 100%;
      height: 100%;
      display: block;
    }

    /* Background rail for each bar */
    .bar-bg {
      fill: var(--gauge-bg, #304562);
      opacity: 0.45;
    }

    /* Foreground (value) */
    .bar-fg {
      /* Animates height/y when values change (SVG attribute transitions are supported in modern browsers) */
      transition: y 300ms ease, height 300ms ease;
    }

    /* Numbers above bars */
    .value-text {
      font-size: 8px;
      fill: var(--card-text-color, #e0e0e0);
      dominant-baseline: central;
      /* Small background halo for readability on dark themes */
      paint-order: stroke;
      stroke: rgba(0,0,0,0.35);
      stroke-width: 0.75px;
    }

    /* Label below bars */
    .chip-label {
      font-size: 7px;
      dominant-baseline: central;
      fill:  var(--card-text-color, #666);
    }

    .container-label {
        position: relative;
        top: -9px;
        /*border: 1px solid;*/
    }
  `],
})
export class AsicTempBarsComponent {
    /** Temperatures for the 4 ASICs. If fewer provided, missing are treated as 0. */
    @Input() temps: number[] = [0, 0, 0, 0];

    /** Expected min/max for scale mapping. */
    @Input() min = 0;
    @Input() max = 120;

    /** Unit to display with the numeric values. */
    @Input() unit: string = '°C';

    /** Thresholds for color changes (warn/critical). */
    @Input() warn = 75;
    @Input() crit = 90;

    /** Compact size (approx 100x100). You can tweak a bit wider if needed. */
    @Input() width = 110;
    @Input() height = 100;

    /** Number formatting for value text. */
    @Input() valueFormat: string = '1.1-1';
    @Input() hoverFormat: string = '1.2-2'; // Tooltip-Format

    // --- Layout constants in the 0..100 SVG space ---
    // Padding from top/bottom for labels & breathing room.
    topPadding = 14;   // leaves room for value text
    bottomPadding = 18; // leaves room for chip labels
    gap = 4;           // horizontal gap between bars
    leftPad = 6;
    rightPad = 6;

    /** Computed plotting height (rail length) in SVG units. */
    get plotHeight(): number {
        return 100 - this.topPadding - this.bottomPadding;
    }

    /** Bar width so that 4 bars + gaps fit into 100 - left/right padding. */
    get barWidth(): number {
        const available = 100 - this.leftPad - this.rightPad;
        const totalGaps = this.gap * 3;
        return (available - totalGaps) / 4;
    }

    /** X positions for the 4 bars. */
    get barXs(): number[] {
        const xs: number[] = [];
        for (let i = 0; i < 4; i++) {
            xs.push(this.leftPad + i * (this.barWidth + this.gap));
        }
        return xs;
    }

    /** Clamp + normalize value to 0..1 (bottom..top). */
    private perc(i: number): number {
        const v = this.safeTemp(i);
        if (this.max === this.min) return 0;
        const p = (v - this.min) / (this.max - this.min);
        return Math.max(0, Math.min(1, p));
    }

    /** Y position of the top of the filled bar for chip i. */
    barY(i: number): number {
        const p = this.perc(i);
        // Higher temp => taller bar; y moves upwards
        // y = topPadding + (1 - p) * plotHeight
        return this.topPadding + (1 - p) * this.plotHeight;
    }

    /** Height of the filled bar for chip i. */
    barH(i: number): number {
        return this.perc(i) * this.plotHeight;
    }

    /** Position the value text slightly above the current bar top, but keep inside the canvas. */
    valueTextY(i: number): number {
        const y = this.barY(i) - 4;
        return Math.max(8, y);
    }

    /** Get temp safely even if array shorter than 4. */
    safeTemp(i: number): number {
        return (this.temps && this.temps[i] != null) ? this.temps[i] : 0;
    }

    /** Pick a color based on thresholds, falling back to Nebular-style CSS vars. */
    barColor(i: number): string {
        const v = this.safeTemp(i);
        if (v >= this.crit) {
            // critical
            return getComputedStyle(document.documentElement)
                .getPropertyValue('--status-danger-color')?.trim() || '#ff5252';
        }
        if (v >= this.warn) {
            // warning
            return getComputedStyle(document.documentElement)
                .getPropertyValue('--status-warning-color')?.trim() || '#ffc107';
        }
        // normal
        return getComputedStyle(document.documentElement)
            .getPropertyValue('--gauge-value-color')?.trim() || '#2d8ad7';
    }

    /** Lower caption under each bar; falls back to A0.. */
    chipLabel(i: number): string {
        return `${i}`;
    }
}
