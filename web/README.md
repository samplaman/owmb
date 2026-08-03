# OWMB Website — GitHub Pages Packaging Guide

This folder contains the packaged static website for **OWMB (OpenWav Media Browser)** ready for deployment to **GitHub Pages**.

---

## 📦 Package Contents

```text
web/
├── index.html           # Main HTML structure
├── styles.css           # Minimalist Swiss typography stylesheet
├── app.js               # Gallery tab switcher & UI logic
├── owmblogo.png         # Official OWMB Logo
├── owmbico.png          # OWMB Icon
├── ss1.png              # 3D Sample Constellation Screenshot
├── ss2.png              # Library & Tag Filtering Screenshot
├── .nojekyll            # Prevents Jekyll processing on GitHub Pages
└── .github/workflows/
    └── static.yml       # GitHub Actions workflow for auto-deployment
```

---

## 🚀 How to Deploy to GitHub Pages

### Method 1: Automatic Deployment with GitHub Actions (Recommended)

1. Commit and push the `web/` folder and `.github/` folder to your main GitHub repository:
   ```bash
   git add web .github
   git commit -m "Add OWMB website for GitHub Pages"
   git push origin main
   ```
2. On GitHub, navigate to **Settings** > **Pages**.
3. Under **Build and deployment**, set **Source** to **GitHub Actions**.
4. GitHub Actions will automatically build and publish your site at `https://samplaman.github.io/owmb/`.

---

### Method 2: Deploy from `gh-pages` Branch or Repository Subfolder

1. If deploying directly from a branch:
   - On GitHub, go to **Settings** > **Pages**.
   - Under **Source**, select **Deploy from a branch**.
   - Choose your branch (e.g. `main`) and folder (`/web` or `/docs` or `/root`).
   - Click **Save**.

Your OWMB website will be live in 1-2 minutes!
