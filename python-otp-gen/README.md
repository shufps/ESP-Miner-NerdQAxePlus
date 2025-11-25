TOTP Generator with QR Code
===========================

This small Python tool creates:

- a random TOTP secret (Base32),
- a matching `otpauth://` URI,
- a QR code in a small window for scanning with an authenticator app (Google Authenticator, Aegis, etc.).

Requirements
------------

- Python 3 (recommended: 3.10 or newer)

Create and activate a virtual environment
-----------------------------------------

In the project directory (where `generate_totp_qr.py` and `requirements.txt` live):

    python3 -m venv .venv

Activate on Linux / macOS:

    source .venv/bin/activate

Activate on Windows (PowerShell):

    .\.venv\Scripts\Activate.ps1

Once the environment is active, your shell prompt will usually show something like `(.venv)` in front.

Install dependencies
--------------------

    pip install --upgrade pip
    pip install -r requirements.txt

Run the script
--------------

Interactive mode (prompts for issuer & account):

    python generate_totp_qr.py

Non-interactive (no prompts):

    python generate_totp_qr.py --issuer "NerdQX" --account "nerdqaxeplus2-B43A"

The script prints to the console:

- the Base32 TOTP secret
- the full `otpauth://` URI

At the same time, a small window opens with the QR code so you can scan it with your TOTP app.

Make the script directly executable on Linux (optional)
-------------------------------------------------------

    chmod +x generate_totp_qr.py
    ./generate_totp_qr.py --issuer "NerdQX" --account "nerdqaxeplus2-B43A"

Leave the virtual environment
-----------------------------

    deactivate

