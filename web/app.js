// OWMB Landing Page - Interactive JavaScript Engine

document.addEventListener('DOMContentLoaded', () => {
  initShowcase();
  initLightbox();
  initCopyButton();
  initKeyboardNav();
  initTouchSwipe();
});

// ==========================================================================
// 1. Interactive Showcase State & Tab Switcher
// ==========================================================================
const showcaseState = {
  currentIndex: 0,
  slides: [],
  tabs: [],
  total: 0
};

function initShowcase() {
  showcaseState.tabs = Array.from(document.querySelectorAll('.tab-btn'));
  showcaseState.slides = Array.from(document.querySelectorAll('.showcase-slide'));
  showcaseState.total = showcaseState.slides.length;

  if (!showcaseState.tabs.length || !showcaseState.slides.length) return;

  // Tab click listeners
  showcaseState.tabs.forEach((tab, index) => {
    tab.addEventListener('click', (e) => {
      e.preventDefault();
      goToShowcaseSlide(index);
    });
  });

  // Next / Previous buttons
  const prevBtn = document.getElementById('prevSlideBtn');
  const nextBtn = document.getElementById('nextSlideBtn');

  if (prevBtn) {
    prevBtn.addEventListener('click', (e) => {
      e.preventDefault();
      const newIndex = (showcaseState.currentIndex - 1 + showcaseState.total) % showcaseState.total;
      goToShowcaseSlide(newIndex);
    });
  }

  if (nextBtn) {
    nextBtn.addEventListener('click', (e) => {
      e.preventDefault();
      const newIndex = (showcaseState.currentIndex + 1) % showcaseState.total;
      goToShowcaseSlide(newIndex);
    });
  }
}

function goToShowcaseSlide(index) {
  if (index < 0 || index >= showcaseState.total) return;

  showcaseState.currentIndex = index;

  // Update tab active states
  showcaseState.tabs.forEach((tab, i) => {
    const isActive = i === index;
    tab.classList.toggle('active', isActive);
    tab.setAttribute('aria-selected', isActive ? 'true' : 'false');
  });

  // Auto-scroll active tab into view horizontally on smaller screens
  if (showcaseState.tabs[index]) {
    showcaseState.tabs[index].scrollIntoView({
      behavior: 'smooth',
      inline: 'center',
      block: 'nearest'
    });
  }

  // Update slide display
  showcaseState.slides.forEach((slide, i) => {
    slide.classList.toggle('active', i === index);
  });

  // Update captions & counter
  const activeSlide = showcaseState.slides[index];
  if (activeSlide) {
    const title = activeSlide.getAttribute('data-title') || 'OWMB View';
    const desc = activeSlide.getAttribute('data-desc') || '';

    const captionTitleEl = document.getElementById('galleryCaptionTitle');
    const captionTextEl = document.getElementById('galleryCaptionText');
    const counterEl = document.getElementById('galleryCounter');

    if (captionTitleEl) captionTitleEl.textContent = title;
    if (captionTextEl) captionTextEl.textContent = desc;
    if (counterEl) counterEl.textContent = `${index + 1} / ${showcaseState.total}`;
  }

  // Update Lightbox if currently open
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
    openFullscreenBtn.addEventListener('click', (e) => {
      e.preventDefault();
      openLightbox(showcaseState.currentIndex);
    });
  }

  document.querySelectorAll('.image-zoom-trigger').forEach((trigger, idx) => {
    trigger.addEventListener('click', (e) => {
      e.preventDefault();
      openLightbox(idx);
    });
  });

  // Close triggers
  if (closeBtn) closeBtn.addEventListener('click', closeLightbox);
  if (backdrop) backdrop.addEventListener('click', closeLightbox);

  // Prev / Next inside Lightbox
  if (prevBtn) {
    prevBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const newIndex = (showcaseState.currentIndex - 1 + showcaseState.total) % showcaseState.total;
      goToShowcaseSlide(newIndex);
    });
  }

  if (nextBtn) {
    nextBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const newIndex = (showcaseState.currentIndex + 1) % showcaseState.total;
      goToShowcaseSlide(newIndex);
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

  goToShowcaseSlide(index);
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
  const activeSlide = showcaseState.slides[index];
  if (!activeSlide) return;

  const imgEl = activeSlide.querySelector('.showcase-img');
  const lightboxImg = document.getElementById('lightboxImg');
  const lightboxTitle = document.getElementById('lightboxTitle');
  const lightboxCaption = document.getElementById('lightboxCaption');

  if (imgEl && lightboxImg) {
    lightboxImg.src = imgEl.src;
    lightboxImg.alt = imgEl.alt;
  }

  const title = activeSlide.getAttribute('data-title') || 'OWMB View';
  const desc = activeSlide.getAttribute('data-desc') || '';

  if (lightboxTitle) lightboxTitle.textContent = `${title} (${index + 1} / ${showcaseState.total})`;
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
      const newIndex = (showcaseState.currentIndex - 1 + showcaseState.total) % showcaseState.total;
      goToShowcaseSlide(newIndex);
    } else if (e.key === 'ArrowRight') {
      const newIndex = (showcaseState.currentIndex + 1) % showcaseState.total;
      goToShowcaseSlide(newIndex);
    }
  });
}

// ==========================================================================
// 4. Touch Swipe Support for Mobile & Tablets
// ==========================================================================
function initTouchSwipe() {
  const container = document.querySelector('.showcase-viewport');
  if (!container) return;

  let touchStartX = 0;
  let touchEndX = 0;

  container.addEventListener('touchstart', (e) => {
    touchStartX = e.changedTouches[0].screenX;
  }, { passive: true });

  container.addEventListener('touchend', (e) => {
    touchEndX = e.changedTouches[0].screenX;
    handleSwipe();
  }, { passive: true });

  function handleSwipe() {
    const diff = touchEndX - touchStartX;
    if (Math.abs(diff) > 45) {
      if (diff < 0) {
        // Swipe Left -> Next Slide
        const newIndex = (showcaseState.currentIndex + 1) % showcaseState.total;
        goToShowcaseSlide(newIndex);
      } else {
        // Swipe Right -> Prev Slide
        const newIndex = (showcaseState.currentIndex - 1 + showcaseState.total) % showcaseState.total;
        goToShowcaseSlide(newIndex);
      }
    }
  }
}

// ==========================================================================
// 5. Copy Terminal Code Snippet
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
