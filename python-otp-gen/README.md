TOTP Generator with QR Code
===========================

> ⚠️ Security disclaimer
>
> This script is intentionally a **convenience hack** for larger fleets where
> maintaining a unique TOTP secret per device would be too painful in practice,
> and where the likely result would be that TOTP gets disabled entirely.
>
> All devices in such a fleet can share the **same TOTP secret**, which makes
> rollout and recovery much easier, but also means that if this secret is ever
> compromised, **every device using it is affected**.
>
> For environments with higher security requirements, you should use
> **per-device TOTP secrets** and a more rigorous provisioning process instead
> of this shared-secret approach.


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

    python generate_totp_qr.py --issuer "NerdQAxe" --account "nerdqaxeplus2-B43A"

The script prints to the console:

- the Base32 TOTP secret
- the full `otpauth://` URI

At the same time, a small window opens with the QR code so you can scan it with your TOTP app.

<img width="383" height="422" alt="image" src="https://github.com/user-attachments/assets/e8b23181-6094-4ec0-ba70-fe1870959116" />


Example Output
--------------

```
=== TOTP Secret (Base32) ===
Z24R33BWOPGD6JQOJP7B5F2NWEEKRDFA

=== otpauth:// URI ===
otpauth://totp/NerdQAxe:nerdqaxeplus2-B43A?secret=Z24R33BWOPGD6JQOJP7B5F2NWEEKRDFA&issuer=NerdQAxe

```

The Base32 TOTP secret can be used in the `config.cvs` like:
```
# TOTP secret
otp_secret,data,string,Z24R33BWOPGD6JQOJP7B5F2NWEEKRDFA

# Enable TOTP
otp_enabled,data,u16,1
```


Make the script directly executable on Linux (optional)
-------------------------------------------------------

    chmod +x generate_totp_qr.py
    ./generate_totp_qr.py --issuer "NerdQX" --account "nerdqaxeplus2-B43A"

Leave the virtual environment
-----------------------------

    deactivate

