// src/app/services/otp-auth.service.ts
import { Injectable } from '@angular/core';
import { NbDialogService } from '@nebular/theme';
import { BehaviorSubject, Observable, of, EMPTY } from 'rxjs';
import { switchMap, take, map } from 'rxjs/operators';
import { SystemService } from './system.service';
import { OtpDialogComponent, OtpDialogResult } from '../components/otp-dialog/otp-dialog.component';

export type EnsureOtpResult = { totp?: string };

@Injectable({ providedIn: 'root' })
export class OtpAuthService {
    private readonly LS_TOKEN = 'otpSessionToken';
    private readonly LS_EXP = 'otpSessionExpiry';
    private readonly hasSessionSubject = new BehaviorSubject<boolean>(false);

    constructor(
        private system: SystemService,
        private dialog: NbDialogService,
    ) {
        // Initialize session state and keep it in sync across tabs
        this.hasSessionSubject.next(!!this.getStoredSession());
        window.addEventListener('storage', (e) => {
            if (e.key === this.LS_TOKEN || e.key === this.LS_EXP) {
                this.hasSessionSubject.next(!!this.getStoredSession());
            }
        });
    }

    private getStoredSession() {
        try {
            const token = localStorage.getItem(this.LS_TOKEN) || '';
            const exp = parseInt(localStorage.getItem(this.LS_EXP) || '0', 10) || 0;
            if (!token || Date.now() >= exp) return null;
            return { token, exp };
        } catch { return null; }
    }

    private saveSession(token: string, ttlMs: number) {
        localStorage.setItem(this.LS_TOKEN, token);
        localStorage.setItem(this.LS_EXP, String(Date.now() + ttlMs));
    }

    public getToken() {
        return localStorage.getItem('otpSessionToken');
    }

    public getExp() {
        return localStorage.getItem('otpSessionExpiry') || '0';
    }

    public clearSession(): void {
        try {
            localStorage.removeItem(this.LS_TOKEN);
            localStorage.removeItem(this.LS_EXP);
            this.hasSessionSubject.next(false);
        } catch { /* ignore */ }
    }

    /** Observable indicating whether a valid local session token exists. */
    public hasSession$(): Observable<boolean> {
        return this.hasSessionSubject.asObservable();
    }

    /** Force an OTP prompt even if OTP isn’t enabled (used for enabling OTP etc.). */
    promptForCode$(title: string, hint: string): Observable<string> {
        const ref = this.dialog.open(OtpDialogComponent, {
            closeOnBackdropClick: false,
            context: { dialogTitle: title, dialogHint: hint, otpEnabled: true, rememberCheckBox: false },
        });
        return ref.onClose.pipe(
            take(1),
            switchMap((res: OtpDialogResult | null) =>
                (res?.code && res.code.length === 6) ? of(res.code) : EMPTY
            ),
        );
    }

    /** Ensure we have either a valid session or a one-shot TOTP. */
    // otp-auth.service.ts
    ensureOtp$(uri: string, title: string, hint: string, ttlMs = 24 * 60 * 60 * 1000)
        : Observable<EnsureOtpResult> {
        return this.system.getInfo(0, uri).pipe(
            take(1),
            switchMap(info => {
                const otpEnabled = !!info?.otp;
                if (!otpEnabled) return of<EnsureOtpResult>({});

                const sess = this.getStoredSession();
                if (sess) return of<EnsureOtpResult>({});

                const ref = this.dialog.open(OtpDialogComponent, {
                    closeOnBackdropClick: false,
                    context: { dialogTitle: title, dialogHint: hint, ttlMs },
                });

                return ref.onClose.pipe(
                    take(1),
                    switchMap((res: OtpDialogResult | null) => {
                        if (!res) return EMPTY;

                        // Remember-for-24h path
                        if (res.remember24h) {
                            // A1: dialog already created a session -> just store it
                            if (res.session?.token) {
                                const effectiveTtl = (res.session.ttlMs ?? ttlMs);
                                // If the dialog provides absolute expiry, prefer it
                                if (res.session.expiresAt) {
                                    localStorage.setItem(this.LS_TOKEN, res.session.token);
                                    localStorage.setItem(this.LS_EXP, String(res.session.expiresAt));
                                    this.hasSessionSubject.next(true);
                                } else {
                                    this.saveSession(res.session.token, effectiveTtl);
                                } return of<EnsureOtpResult>({});
                            }
                            // A2: dialog only returned a code -> create the session here
                            if (res.code?.length === 6) {
                                return this.system.createOtpSession(res.code, ttlMs).pipe(
                                    map(({ token, ttlMs: serverTtl, expiresAt }) => {
                                        if (expiresAt) {
                                            localStorage.setItem(this.LS_TOKEN, token);
                                            localStorage.setItem(this.LS_EXP, String(expiresAt));
                                            this.hasSessionSubject.next(true);
                                        } else {
                                            this.saveSession(token, serverTtl ?? ttlMs);
                                        }
                                        return {} as EnsureOtpResult;
                                    })
                                );
                            }
                            return EMPTY;
                        }

                        // One-shot code path
                        if (res.code?.length === 6) return of<EnsureOtpResult>({ totp: res.code });

                        return EMPTY;
                    })
                );
            })
        );
    }
}
