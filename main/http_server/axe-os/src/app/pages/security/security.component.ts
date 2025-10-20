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
    otpStatus$: Observable<{enabled: boolean}>;

    enrollmentActive = false;
    pending = false;

    // only used by inline forms (you can remove if you strictly use the modal)
    totpEnable = '';
    totpDisable = '';

    constructor(
        private system: SystemService,
        private toast: NbToastrService,
        private translate: TranslateService,
        private otpAuth: OtpAuthService,
    ) {
        this.otpStatus$ = this.system.getOTPStatus();
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
                this.toast.danger(this.translate.instant('SECURITY.ENROLL_FAIL'), this.translate.instant('COMMON.ERROR'));
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
                this.toast.success(this.translate.instant('SECURITY.ENABLED'), this.translate.instant('COMMON.SUCCESS'));
                this.enrollmentActive = false;
                this.totpEnable = '';
            }),
            switchMap(() => this.refreshInfo()),
            catchError(err => {
                this.toast.danger(this.translate.instant('SECURITY.ENABLE_FAIL'), this.translate.instant('COMMON.ERROR'));
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
                this.toast.success(this.translate.instant('SECURITY.DISABLED'), this.translate.instant('COMMON.SUCCESS'));
                this.enrollmentActive = false;
                this.totpDisable = '';
            }),
            switchMap(() => this.refreshInfo()),
            catchError(err => {
                this.toast.danger(this.translate.instant('SECURITY.DISABLE_FAIL'), this.translate.instant('COMMON.ERROR'));
                return of(null);
            }),
            tap(() => this.pending = false),
        ).subscribe();
    }

    askForOtpAndEnable() {
        this.otpAuth
            .promptForCode$(
                this.translate.instant('SECURITY.OTP_TITLE'),
                this.translate.instant('SECURITY.OTP_ENABLE_HINT')
            )
            .pipe(
                tap(() => (this.pending = true)),
                switchMap(code =>
                    this.system.updateOtp(true, code).pipe(
                        tap(() => {
                            this.toast.success(this.translate.instant('SECURITY.ENABLED'), this.translate.instant('COMMON.SUCCESS'));
                            this.enrollmentActive = false;
                        }),
                        switchMap(() => this.refreshInfo()),
                        catchError(() => {
                            this.toast.danger(this.translate.instant('SECURITY.ENABLE_FAIL'), this.translate.instant('COMMON.ERROR'));
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
            .promptForCode$(
                this.translate.instant('SECURITY.OTP_TITLE'),
                this.translate.instant('SECURITY.OTP_DISABLE_HINT')
            )
            .pipe(
                tap(() => (this.pending = true, this.otpAuth.clearSession())),
                switchMap(code =>
                    // totp ist evtl. undefined, wenn eine gültige Session existiert (Header kommt via Interceptor)
                    this.system.updateOtp(false, code ?? '').pipe(
                        tap(() => {
                            this.toast.success(this.translate.instant('SECURITY.DISABLED'), this.translate.instant('COMMON.SUCCESS'));
                            this.enrollmentActive = false;
                        }),
                        switchMap(() => this.refreshInfo()),
                        catchError((err: HttpErrorResponse) => {
                            this.toast.danger(this.translate.instant('SECURITY.DISABLE_FAIL'), this.translate.instant('COMMON.ERROR'));
                            return of(null);
                        })
                    )
                ),
                tap(() => (this.pending = false))
            )
            .subscribe();
    }

    private refreshInfo(): Observable<{enabled: boolean}> {
        this.otpStatus$ = this.system.getOTPStatus();
        return this.otpStatus$;
    }
}
