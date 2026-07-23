// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

// The digital-signature backend (Poppler-Qt6 over its NSS crypto backend).
// Signs a document with a certificate from the user's NSS database and verifies
// the signatures already in a document.
class Signer {
public:
    // Point the signing stack at an NSS certificate database directory. The app
    // calls this once at start-up with the shared user store (~/.pki/nssdb) so
    // certificate listing, signing, and security-device management all agree.
    static void useNssDatabase(const QString& dir);

    // Display names (NSS nicknames) of certificates available for signing. Empty
    // if none are set up in the NSS database.
    static QStringList availableCertificates();

    // ── Hardware tokens (PKCS#11) ───────────────────────────────────────────
    // True if the NSS tooling (modutil) for managing security devices is present.
    static bool hasSecurityDeviceTools();
    // Register a PKCS#11 module (e.g. /usr/lib64/opensc-pkcs11.so for smartcards
    // and YubiKeys) under `name` in the NSS database, so certificates on an
    // inserted token show up in availableCertificates(). The token PIN is then
    // entered as the certificate password when signing. Returns false + *error.
    static bool addSecurityDevice(const QString& name, const QString& modulePath, QString* error);
    // Remove a previously registered module by name.
    static bool removeSecurityDevice(const QString& name, QString* error);
    // Names of the external security modules currently registered (the NSS
    // internal module is excluded).
    static QStringList securityDevices();

    // Sign `inputPath` with the named certificate, writing `outputPath`. `rect`
    // is the signature's position on `page` (0-based) in page points (origin
    // bottom-left). `password` unlocks the key (often empty for a local NSS DB).
    // `imagePath` is an optional PNG/JPG drawn as the signature's appearance (a
    // graphical/handwritten signature); empty means the default text appearance.
    // Returns true on success; on failure fills *error.
    static bool sign(const QString& inputPath, const QString& outputPath,
                     const QString& certName, const QString& password, const QString& reason,
                     const QString& location, int page, const QRectF& rect,
                     const QString& imagePath, QString* error);

    // Request an RFC 3161 trusted timestamp over `filePath` from the timestamp
    // authority at `tsaUrl`, writing the DER reply to `outTokenPath` (a detached
    // `.tsr` sidecar — proof the file existed at the stamped time). Uses the
    // `openssl` and `curl` CLIs; returns false with a human message in *error
    // when the tools are missing, the TSA is unreachable, or the reply is invalid.
    static bool timestamp(const QString& filePath, const QString& tsaUrl,
                          const QString& outTokenPath, QString* error);

    struct SignatureInfo {
        QString signer;
        QString status;   // human-readable validity
        bool valid = false;
        QString time;     // signing time, localized
        QString reason;
        QString location;
    };

    // The signatures present in `path`, each validated.
    static QList<SignatureInfo> verify(const QString& path);
};
