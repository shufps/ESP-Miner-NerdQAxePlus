import { Component } from '@angular/core';
import { Observable, of, switchMap, tap, catchError } from 'rxjs';
import { NbToastrService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { SystemService } from '../../services/system.service';
import { ISystemInfo } from '../../models/ISystemInfo';
import { OtpAuthService } from '../../services/otp-auth.service';
import { HttpErrorResponse } from '@angular/common/http';

@Component({
    selector: 'app-security',
    templateUrl: './security.component.html',
    styleUrls: ['./security.component.scss'],
})
export class SecurityComponent {
    info$: Observable<ISystemInfo>;

    enrollmentActive = false;
    pending = false;

    // only used by inline forms (you can remove if you strictly use the modal)
    totpEnable = '';
    totpDisable = '';

    constructor(
        private system: SystemService,
        private toast: NbToastrService,
        private i18n: TranslateService,
        private otpAuth: OtpAuthService,
    ) {
        this.info$ = this.system.getInfo(0);
    }

    /** Start OTP enrollment: POST /api/otp -> device shows QR, then prompt for code */
    startEnrollment() {
        // Guard: avoid double-click
        if (this.pending) return;

        this.pending = true;
        this.system.startOtpEnrollment().pipe(
            tap(() => {
                this.enrollmentActive = true;
                // Immediately open the OTP dialog to enter the code
                this.askForOtpAndEnable();
            }),
            switchMap(() => this.refreshInfo()),
            catchError(err => {
                this.toast.danger(this.t('SECURITY.ENROLL_FAIL', 'Enrollment failed'), this.t('COMMON.ERROR', 'Error'));
                return of(null);
            }),
            tap(() => this.pending = false),
        ).subscribe();
    }


    /** Inline enable (kept for completeness) */
    enableOtp() {
        const code = (this.totpEnable || '').trim();
        if (code.length !== 6) return;

        this.pending = true;
        this.system.updateOtp(true, code).pipe(
            tap(() => {
                this.toast.success(this.t('SECURITY.ENABLED', 'OTP enabled'), this.t('COMMON.SUCCESS', 'Success'));
                this.enrollmentActive = false;
                this.totpEnable = '';
            }),
            switchMap(() => this.refreshInfo()),
            catchError(err => {
                this.toast.danger(this.t('SECURITY.ENABLE_FAIL', 'Invalid or expired code'), this.t('COMMON.ERROR', 'Error'));
                return of(null);
            }),
            tap(() => this.pending = false),
        ).subscribe();
    }

    /** Inline disable (kept for completeness) */
    disableOtp() {
        const code = (this.totpDisable || '').trim();
        if (code.length !== 6) return;

        this.pending = true;
        this.system.updateOtp(false, code).pipe(
            tap(() => {
                this.toast.success(this.t('SECURITY.DISABLED', 'OTP disabled'), this.t('COMMON.SUCCESS', 'Success'));
                this.enrollmentActive = false;
                this.totpDisable = '';
            }),
            switchMap(() => this.refreshInfo()),
            catchError(err => {
                this.toast.danger(this.t('SECURITY.DISABLE_FAIL', 'Code invalid'), this.t('COMMON.ERROR', 'Error'));
                return of(null);
            }),
            tap(() => this.pending = false),
        ).subscribe();
    }

    /** Modal → Enable (erzwingt Codeeingabe, weil OTP noch aus ist) */
    askForOtpAndEnable() {
        this.otpAuth
            .promptForCode$(
                this.t('SECURITY.OTP_TITLE', 'Confirm with OTP'),
                this.t('SECURITY.OTP_ENABLE_HINT', 'Scan the QR on the device, then enter the 6-digit code.')
            )
            .pipe(
                tap(() => (this.pending = true)),
                switchMap(code =>
                    this.system.updateOtp(true, code).pipe(
                        tap(() => {
                            this.toast.success(this.t('SECURITY.ENABLED', 'OTP enabled'), this.t('COMMON.SUCCESS', 'Success'));
                            this.enrollmentActive = false;
                        }),
                        switchMap(() => this.refreshInfo()),
                        catchError(() => {
                            this.toast.danger(this.t('SECURITY.ENABLE_FAIL', 'Invalid or expired code'), this.t('COMMON.ERROR', 'Error'));
                            return of(null);
                        })
                    )
                ),
                tap(() => (this.pending = false))
            )
            .subscribe();
    }

    /** Modal → Disable (nutzt ensureOtp$: Session oder Code, je nach Zustand) */
    askForOtpAndDisable() {
        this.otpAuth
            .ensureOtp$(
                "",
                this.t('SECURITY.OTP_TITLE', 'Confirm with OTP'),
                this.t('SECURITY.OTP_DISABLE_HINT', 'Enter current 6-digit code to disable OTP.')
            )
            .pipe(
                tap(() => (this.pending = true)),
                switchMap(({ totp }) =>
                    // totp ist evtl. undefined, wenn eine gültige Session existiert (Header kommt via Interceptor)
                    this.system.updateOtp(false, totp ?? '').pipe(
                        tap(() => {
                            this.toast.success(this.t('SECURITY.DISABLED', 'OTP disabled'), this.t('COMMON.SUCCESS', 'Success'));
                            this.enrollmentActive = false;
                        }),
                        switchMap(() => this.refreshInfo()),
                        catchError((err: HttpErrorResponse) => {
                            this.toast.danger(this.t('SECURITY.DISABLE_FAIL', 'Code invalid'), this.t('COMMON.ERROR', 'Error'));
                            return of(null);
                        })
                    )
                ),
                tap(() => (this.pending = false))
            )
            .subscribe();
    }

    private refreshInfo(): Observable<ISystemInfo> {
        this.info$ = this.system.getInfo(0);
        return this.info$;
    }

    private t(key: string, fallback: string) {
        const v = this.i18n.instant(key);
        return (v && v !== key) ? v : fallback;
    }
}
