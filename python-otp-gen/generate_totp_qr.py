#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import sys

import pyotp
import qrcode

try:
    import tkinter as tk
    from PIL import Image, ImageTk
except ImportError:
    tk = None
    Image = None
    ImageTk = None


def build_totp_uri(secret: str, issuer: str, account: str, period: int = 30, digits: int = 6) -> str:
    """Build an otpauth:// URI for the given parameters."""
    totp = pyotp.TOTP(secret, interval=period, digits=digits)
    return totp.provisioning_uri(name=account, issuer_name=issuer)


def show_qr_in_window(img):
    """Show the given PIL image in a small Tkinter window."""
    if tk is None or ImageTk is None:
        print("[WARN] Tkinter or Pillow is not available; cannot show GUI window.", file=sys.stderr)
        return

    root = tk.Tk()
    root.title("Scan TOTP QR")

    # Convert PIL image to Tkinter-compatible image
    tk_img = ImageTk.PhotoImage(img)

    label = tk.Label(root, image=tk_img)
    label.pack(padx=10, pady=10)

    # Keep a reference so it is not garbage-collected
    label.image = tk_img

    root.mainloop()


def main(argv=None):
    parser = argparse.ArgumentParser(description="Generate a TOTP secret and display it as a QR code.")
    parser.add_argument("--issuer", help="Issuer name (e.g. NerdQAxe)")
    parser.add_argument("--account", help="Account label / username for this TOTP")
    parser.add_argument("--period", type=int, default=30, help="TOTP period in seconds (default 30)")
    parser.add_argument("--digits", type=int, default=6, help="Number of TOTP digits (default 6)")
    parser.add_argument("--no-gui", action="store_true", help="Do not open a Tk window, only print data")

    args = parser.parse_args(argv)

    issuer = args.issuer or input("Issuer (z.B. NerdQAxe): ").strip() or "ExampleIssuer"
    account = args.account or input("Account / Label (z.B. user@example.com): ").strip() or "example@local"

    # Generate new random Base32 secret
    secret = pyotp.random_base32()

    # Build otpauth URI
    uri = build_totp_uri(secret, issuer, account, period=args.period, digits=args.digits)

    print("\n=== TOTP Secret (Base32) ===")
    print(secret)
    print("\n=== otpauth:// URI ===")
    print(uri)

    # Build QR code as PIL image
    qr = qrcode.QRCode(
        version=None,
        error_correction=qrcode.constants.ERROR_CORRECT_M,
        box_size=8,
        border=2,
    )
    qr.add_data(uri)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white").convert("RGB")

    if not args.no_gui:
        show_qr_in_window(img)


if __name__ == "__main__":
    main()
