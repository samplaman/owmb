// OWMB Landing Page - JavaScript Engine

document.addEventListener('DOMContentLoaded', () => {
  initScreenshotTabs();
  initCopyButton();
});

// ==========================================
// 1. Screenshot Gallery Tabs Switcher
// ==========================================
function initScreenshotTabs() {
  const tabBtns = document.querySelectorAll('.tab-btn');
  const showcaseImgs = document.querySelectorAll('.showcase-img');

  if (!tabBtns.length || !showcaseImgs.length) return;

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const targetId = btn.getAttribute('data-target');

      // Update Tab Buttons Active State
      tabBtns.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      // Update Screenshot Images Display
      showcaseImgs.forEach(img => {
        if (img.id === targetId) {
          img.classList.add('active');
        } else {
          img.classList.remove('active');
        }
      });
    });
  });
}

// ==========================================
// 2. Copy Terminal Code Snippet
// ==========================================
function initCopyButton() {
  const copyBtn = document.getElementById('copyTerminalBtn');
  if (!copyBtn) return;

  copyBtn.addEventListener('click', () => {
    const codeText = `git clone https://github.com/samplaman/owmb.git\ncd owmb\ncmake -B build -DCMAKE_BUILD_TYPE=Release\ncmake --build build --config Release`;
    
    navigator.clipboard.writeText(codeText).then(() => {
      const originalHTML = copyBtn.innerHTML;
      copyBtn.innerHTML = `<i class="fa-solid fa-check" style="color: var(--accent-green);"></i> Copied!`;
      setTimeout(() => {
        copyBtn.innerHTML = originalHTML;
      }, 2000);
    }).catch(err => {
      console.error('Copy failed', err);
    });
  });
}
