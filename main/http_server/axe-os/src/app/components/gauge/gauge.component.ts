import { Component, Input } from '@angular/core';

@Component({
  selector: 'app-gauge',
  template: `
    <div class="gauge-container">
      <div class="gauge-wrap">
        <svg class="gauge" [attr.viewBox]="viewBox">
          <!-- Background Circle -->
          <circle
            class="gauge-bg"
            [attr.cx]="center"
            [attr.cy]="center"
            [attr.r]="radius"
            [attr.stroke-dasharray]="circumference"
            [attr.stroke-dashoffset]="offset2"
            transform="rotate(135, 50, 50)"
          ></circle>
          <!-- Foreground Circle (Value) -->
          <circle
            class="gauge-value"
            [attr.cx]="center"
            [attr.cy]="center"
            [attr.r]="radius"
            [attr.stroke-dasharray]="circumference"
            [attr.stroke-dashoffset]="offset"
            transform="rotate(135, 50, 50)"
          ></circle>
        </svg>
        <div class="gauge-value-container">
          <div class="gauge-value">{{ value | number: format }}</div>
          <div class="gauge-unit">{{ unit }}</div>
        </div>
      </div>
      <!-- Label Below -->
      <div class="gauge-label" *ngIf="label">
        {{ label }}
      </div>
    </div>
  `,
  styles: [
    `
      .gauge-container {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
      }

      .gauge-wrap {
        position: relative;
      }

      .gauge {
        width: 100px;
        height: auto;
      }

      .gauge-bg {
        fill: none;
        stroke: var(--gauge-bg, #304562);
        stroke-width: 14;
      }

      .gauge-value {
        fill: none;
        stroke: var(--gauge-value-color, #64B5F6);
        stroke-width: 14;
        transition: stroke-dashoffset 0.5s ease-in-out;
      }

      .gauge-value-container {
        position: absolute;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        text-align: center;
      }

      .gauge-value {
        font-size: 18px;
        color: var(--card-text-color);
      }

      .gauge-unit {
        font-size: 14px;
        color: var(--text-hint-color);
      }

      .gauge-label {
        margin-top: -14px;
        margin-bottom: 14px;
        font-size: 14px;
        color: var(--card-text-color, #666);
        text-align: center;
      }
    `,
  ],
})
export class GaugeComponent {
  @Input() value: number = 0; // Current value
  @Input() min: number = 0; // Minimum value
  @Input() max: number = 100; // Maximum value
  @Input() unit: string = ''; // Unit (e.g., %, °C)
  @Input() label: string = ''; // Label below the gauge
  @Input() format: string = '1.2-2'; // Default format with 2 decimal places

  radius: number = 40; // Radius of the circle
  center: number = 50; // Center of the circle
  viewBox: string = '0 0 100 100'; // SVG viewBox

  get circumference(): number {
    return 2 * Math.PI * this.radius;
  }

  get offset(): number {
    let progress = (this.value - this.min) / (this.max - this.min) * 0.75;

    if (progress > 0.75) {
      progress = 0.75;
    }
    return this.circumference * (1 - Math.min(Math.max(progress, 0), 1));
  }

  get offset2(): number {
    const progress = 0.75; // Full arc length for 270°
    return this.circumference * (1 - Math.min(Math.max(progress, 0), 1));
  }
}
