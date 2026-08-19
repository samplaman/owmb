# Apple macOS Code Signing & Notarization Guide for OWMB

This guide walks through code signing and Apple Notarization for the **OWMB** audio plugin (VST3, AU) and Standalone macOS application.

---

## 1. What You Need from Apple Developer Account

To distribute outside the Mac App Store without Gatekeeper warnings ("cannot be opened because Apple cannot check it for malicious software"), you need:
1. **Apple Developer Account** (Individual or Organization).
2. **Developer ID Application Certificate**: Used to sign `.app`, `.vst3`, and `.component` bundles.
3. **App-Specific Password** (or App Store Connect API Key): Used for submitting releases to Apple's automated Notarization service (`notarytool`).
4. **Apple Team ID**: 10-character alphanumeric ID (found in [Apple Developer Account Membership](https://developer.apple.com/account)).

---

## 2. Generating Your Developer ID Application Certificate

### Option A: Using Xcode (Easiest)
1. Open **Xcode** -> **Settings...** (`Cmd + ,`) -> **Accounts**.
2. Sign in with your Apple ID.
3. Select your Team and click **Manage Certificates...**.
4. Click the **+** (plus button) in the lower left corner and choose **Developer ID Application**.
5. Xcode will create and install the certificate into your macOS Keychain.

### Option B: Using Apple Developer Portal
1. Go to [developer.apple.com/account/resources/certificates/list](https://developer.apple.com/account/resources/certificates/list).
2. Click **+** to add a new certificate.
3. Select **Developer ID Application** and click **Continue**.
4. Follow instructions to upload a Certificate Signing Request (CSR) from **Keychain Access** (`Keychain Access -> Certificate Assistant -> Request a Certificate from a Certificate Authority`).
5. Download the `.cer` file and double-click it to install into your Keychain.

---

## 3. Verify Certificate in Keychain

Open Terminal and run:
```bash
security find-identity -v -p codesigning
```

You should see an identity like:
```text
1) 1234567890ABCDEF1234567890ABCDEF12345678 "Developer ID Application: Your Name (TEAM_ID)"
```

---

## 4. Local Signing & Building

We have automated code signing with Apple Hardened Runtime in [`build-macos.sh`](../build-macos.sh).

### A. Build and Sign with Auto-Detected Certificate:
```bash
./build-macos.sh
```
*(If a Developer ID Application certificate is present in your Keychain, it will automatically sign the `.vst3`, `.component`, and `.app` bundles).*

### B. Build, Sign, and Notarize with Apple:
Generate an App-Specific Password at [appleid.apple.com](https://appleid.apple.com) (under Sign-In and Security -> App-Specific Passwords).

```bash
./build-macos.sh \
  --sign "Developer ID Application: Your Name (TEAM_ID)" \
  --notarize \
  --apple-id "your-apple-id@example.com" \
  --password "xxxx-xxxx-xxxx-xxxx" \
  --team-id "YOUR_TEAM_ID"
```

---

## 5. Setting Up GitHub Actions CI/CD for Automated Releases

To have GitHub Actions automatically sign and notarize every release tag (`v*`), configure the following repository secrets under **GitHub Repository -> Settings -> Secrets and variables -> Actions**:

### Exporting your `.p12` Certificate
1. Open **Keychain Access** app on your Mac.
2. Select **login** keychain and **My Certificates**.
3. Right-click your **Developer ID Application: ...** certificate and choose **Export "Developer ID Application: ..."**.
4. Save as `DeveloperID.p12` and enter a strong password.
5. In Terminal, encode the `.p12` file to Base64:
   ```bash
   base64 -i DeveloperID.p12 | pbcopy
   ```
   *(The base64 string is now in your clipboard).*

### Add GitHub Secrets:
| Secret Name | Description | Example |
| :--- | :--- | :--- |
| `MACOS_CERTIFICATE` | Base64-encoded `.p12` certificate file | `MIIKogIBAzCCCm8GCSq...` |
| `MACOS_CERTIFICATE_PWD` | Password chosen when exporting `.p12` | `MySecretPassword123!` |
| `MACOS_CERTIFICATE_NAME` | Full identity name | `Developer ID Application: Your Name (TEAM_ID)` |
| `APPLE_ID` | Your Apple Developer email address | `developer@example.com` |
| `APPLE_APP_SPECIFIC_PASSWORD`| App-Specific Password from appleid.apple.com | `abcd-efgh-ijkl-mnop` |
| `APPLE_TEAM_ID` | 10-character Apple Team ID | `ABCDE12345` |

---

## 6. Hardened Runtime Entitlements

Audio plugins (AU & VST3) and DAW standalone applications require specific entitlements when signed with Apple's Hardened Runtime. These are defined in [`entitlements.plist`](../entitlements.plist):

- `com.apple.security.cs.allow-jit`: Permits runtime JIT compilation.
- `com.apple.security.cs.allow-unsigned-executable-memory`: Allows memory allocation required by audio engines and DSP.
- `com.apple.security.cs.disable-library-validation`: Critical for Audio Units and VST3 plugins so third-party DAWs (Logic Pro, Ableton Live, Reaper, Cubase) can load the plugin bundles.
- `com.apple.security.device.audio-input`: Enables microphone / audio device recording permissions.

---

## 7. Troubleshooting: "App is Damaged and Can't Be Opened"

### Why does this error happen?
On recent macOS versions (**macOS 14 Sonoma** and **macOS 15 Sequoia**), Apple's Gatekeeper security policy treats any downloaded application or plugin that has the `com.apple.quarantine` extended attribute as "damaged" if:
1. It is **unsigned** or **ad-hoc signed**.
2. It is signed with a Developer ID but has **not been notarized** by Apple's Notarization Service (`notarytool`), or the notarization ticket is not stapled.
3. The user downloads an Intel-only build onto Apple Silicon without proper Rosetta/quarantine clearance.

### Immediate Workaround for Users:
End users who encounter this error on an existing downloaded release can immediately clear the quarantine flag using Terminal:

```bash
# For Standalone App:
xattr -cr /path/to/OWMB.app

# For extracted Release Folder:
xattr -cr ~/Downloads/OWMB-macOS-Universal

# For Audio Plugins:
sudo xattr -cr /Library/Audio/Plug-Ins/VST3/OWMB.vst3
sudo xattr -cr /Library/Audio/Plug-Ins/Components/OWMB.component
```

Or via macOS Settings:
1. Open **System Settings** -> **Privacy & Security**.
2. Scroll to the **Security** section.
3. Click **"Open Anyway"** next to `OWMB`.

### Permanent Fix:
Every release must be signed with Apple Developer ID and submitted to Apple Notary Service (`xcrun notarytool submit ... --wait`), and stapled (`xcrun stapler staple ...`). Distributing via a signed & stapled `.dmg` (Disk Image) is strongly recommended over `.zip` alone for macOS audio plugins.

