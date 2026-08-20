// OWMB Landing Page - Interactive JavaScript Engine

document.addEventListener('DOMContentLoaded', () => {
  initGallery();
  initLightbox();
  initCopyButton();
  initKeyboardNav();
});

// ==========================================================================
// 1. Gallery State & Tab Switcher
// ==========================================================================
const galleryState = {
  currentIndex: 0,
  slides: [],
  tabs: [],
  total: 0
};

function initGallery() {
  galleryState.tabs = Array.from(document.querySelectorAll('.tab-btn'));
  galleryState.slides = Array.from(document.querySelectorAll('.gallery-slide'));
  galleryState.total = galleryState.slides.length;

  if (!galleryState.tabs.length || !galleryState.slides.length) return;

  // Tab click listeners
  galleryState.tabs.forEach((tab, index) => {
    tab.addEventListener('click', () => {
      goToSlide(index);
    });
  });

  // Next / Previous buttons
  const prevBtn = document.getElementById('prevSlideBtn');
  const nextBtn = document.getElementById('nextSlideBtn');

  if (prevBtn) {
    prevBtn.addEventListener('click', () => {
      const newIndex = (galleryState.currentIndex - 1 + galleryState.total) % galleryState.total;
      goToSlide(newIndex);
    });
  }

  if (nextBtn) {
    nextBtn.addEventListener('click', () => {
      const newIndex = (galleryState.currentIndex + 1) % galleryState.total;
      goToSlide(newIndex);
    });
  }
}

function goToSlide(index) {
  if (index < 0 || index >= galleryState.total) return;

  galleryState.currentIndex = index;

  // Update tabs
  galleryState.tabs.forEach((tab, i) => {
    const isActive = i === index;
    tab.classList.toggle('active', isActive);
    tab.setAttribute('aria-selected', isActive ? 'true' : 'false');
  });

  // Update slides
  galleryState.slides.forEach((slide, i) => {
    slide.classList.toggle('active', i === index);
  });

  // Update Window title and Captions
  const activeSlide = galleryState.slides[index];
  if (activeSlide) {
    const title = activeSlide.getAttribute('data-title') || 'OWMB View';
    const desc = activeSlide.getAttribute('data-desc') || '';

    const windowTitleEl = document.getElementById('galleryWindowTitle');
    const captionTitleEl = document.getElementById('galleryCaptionTitle');
    const captionTextEl = document.getElementById('galleryCaptionText');
    const counterEl = document.getElementById('galleryCounter');

    if (windowTitleEl) windowTitleEl.textContent = title;
    if (captionTitleEl) {
      // Extract short name from title
      const shortTitle = title.replace('OWMB — ', '');
      captionTitleEl.textContent = shortTitle;
    }
    if (captionTextEl) captionTextEl.textContent = desc;
    if (counterEl) counterEl.textContent = `${index + 1} / ${galleryState.total}`;
  }

  // Update Lightbox if open
  if (lightboxState.isOpen) {
    updateLightboxContent(index);
  }
}

// ==========================================================================
// 2. Lightbox Fullscreen Modal
// ==========================================================================
const lightboxState = {
  isOpen: false
};

function initLightbox() {
  const modal = document.getElementById('lightboxModal');
  const backdrop = document.getElementById('lightboxBackdrop');
  const closeBtn = document.getElementById('lightboxCloseBtn');
  const openFullscreenBtn = document.getElementById('openLightboxBtn');
  const prevBtn = document.getElementById('lightboxPrevBtn');
  const nextBtn = document.getElementById('lightboxNextBtn');

  if (!modal) return;

  // Open triggers
  if (openFullscreenBtn) {
    openFullscreenBtn.addEventListener('click', () => openLightbox(galleryState.currentIndex));
  }

  document.querySelectorAll('.image-zoom-trigger').forEach((trigger, idx) => {
    trigger.addEventListener('click', () => openLightbox(idx));
  });

  // Close triggers
  if (closeBtn) closeBtn.addEventListener('click', closeLightbox);
  if (backdrop) backdrop.addEventListener('click', closeLightbox);

  // Prev / Next inside Lightbox
  if (prevBtn) {
    prevBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const newIndex = (galleryState.currentIndex - 1 + galleryState.total) % galleryState.total;
      goToSlide(newIndex);
    });
  }

  if (nextBtn) {
    nextBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const newIndex = (galleryState.currentIndex + 1) % galleryState.total;
      goToSlide(newIndex);
    });
  }
}

function openLightbox(index) {
  const modal = document.getElementById('lightboxModal');
  if (!modal) return;

  lightboxState.isOpen = true;
  modal.classList.add('open');
  modal.setAttribute('aria-hidden', 'false');
  document.body.style.overflow = 'hidden';

  goToSlide(index);
  updateLightboxContent(index);
}

function closeLightbox() {
  const modal = document.getElementById('lightboxModal');
  if (!modal) return;

  lightboxState.isOpen = false;
  modal.classList.remove('open');
  modal.setAttribute('aria-hidden', 'true');
  document.body.style.overflow = '';
}

function updateLightboxContent(index) {
  const activeSlide = galleryState.slides[index];
  if (!activeSlide) return;

  const imgEl = activeSlide.querySelector('.gallery-img');
  const lightboxImg = document.getElementById('lightboxImg');
  const lightboxTitle = document.getElementById('lightboxTitle');
  const lightboxCaption = document.getElementById('lightboxCaption');

  if (imgEl && lightboxImg) {
    lightboxImg.src = imgEl.src;
    lightboxImg.alt = imgEl.alt;
  }

  const title = activeSlide.getAttribute('data-title') || 'OWMB View';
  const desc = activeSlide.getAttribute('data-desc') || '';

  if (lightboxTitle) lightboxTitle.textContent = `${title} (${index + 1}/${galleryState.total})`;
  if (lightboxCaption) lightboxCaption.textContent = desc;
}

// ==========================================================================
// 3. Keyboard Navigation
// ==========================================================================
function initKeyboardNav() {
  window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && lightboxState.isOpen) {
      closeLightbox();
    } else if (e.key === 'ArrowLeft') {
      const newIndex = (galleryState.currentIndex - 1 + galleryState.total) % galleryState.total;
      goToSlide(newIndex);
    } else if (e.key === 'ArrowRight') {
      const newIndex = (galleryState.currentIndex + 1) % galleryState.total;
      goToSlide(newIndex);
    }
  });
}

// ==========================================================================
// 4. Copy Terminal Code Snippet
// ==========================================================================
function initCopyButton() {
  const copyBtn = document.getElementById('copyTerminalBtn');
  const snippetEl = document.getElementById('terminalSnippet');
  if (!copyBtn) return;

  copyBtn.addEventListener('click', () => {
    const codeText = snippetEl ? snippetEl.textContent.trim() : `git clone https://github.com/samplaman/owmb.git\ncd owmb\ncmake -B build -DCMAKE_BUILD_TYPE=Release\ncmake --build build --config Release -j 4`;

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
