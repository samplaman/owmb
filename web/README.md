# OWMB Website — GitHub Pages Packaging Guide

This folder contains the packaged static website for **OWMB (OpenWav Media Browser)** ready for automatic deployment to **GitHub Pages**.

---

## 📦 Package Contents

```text
web/
├── index.html           # Main HTML structure & showcase interface
├── styles.css           # Studio Dark theme stylesheet
├── app.js               # Interactive gallery, Lightbox & keyboard switcher
├── owmblogo.png         # Official OWMB Logo
├── owmbico.png          # OWMB Icon
├── ss1.png              # 3D Sample Constellation Screenshot
├── ss2.png              # Library & Tag Filtering Screenshot
├── screenshots/         # Full High-Res Suite of OWMB Workspace Screenshots
│   ├── 01-list-browser.png        # List View & Waveform Editor
│   ├── 02-3d-cloud.png            # 3D Sample Cloud Constellation
│   ├── 03-online-library.png      # Pixeldrain Soundbanks Browser
│   ├── 04-recorder-eq.png         # Audio Recorder & 9-Band Parametric EQ
│   ├── 05-acoustic-analysis.png   # Timbral Acoustic Fingerprint
│   ├── 06-sample-map.png          # Multi-Sample Keyzone Map
│   └── 07-performance-grid.png    # 32-Pad Performance Mode
├── .nojekyll            # Prevents Jekyll processing on GitHub Pages
└── .github/workflows/
    └── deploy-web.yml   # GitHub Actions workflow for auto-deployment
```

---

## 🚀 Deployment

The site is automatically deployed via GitHub Actions whenever changes are pushed to `web/**` on `main`:

```bash
git add web docs
git commit -m "Update OWMB website with full high-res screenshots showcase"
git push origin main
```

Live Site URL: `https://samplaman.github.io/owmb/`
