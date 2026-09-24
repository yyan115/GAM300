(() => {
  'use strict';

  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  const lightbox = document.getElementById('lightbox');
  const hero = document.querySelector('.hero');
  const heroVideo = document.getElementById('hero-video');
  const saveData = Boolean(navigator.connection?.saveData);
  let heroVisible = false;
  let heroEpoch = 0;

  function syncHero() {
    const epoch = ++heroEpoch;
    if (heroVisible && !document.hidden && !lightbox.open && !reducedMotion.matches && !saveData) {
      if (!heroVideo.getAttribute('src')) heroVideo.src = heroVideo.dataset.src;
      heroVideo.play().then(() => {
        if (epoch === heroEpoch) hero.classList.add('is-playing');
      }).catch(() => {
        if (epoch === heroEpoch) hero.classList.remove('is-playing');
      });
    } else {
      heroVideo.pause();
    }
  }
  heroVideo.addEventListener('error', () => hero.classList.remove('is-playing'));

  const story = document.querySelector('.story-art');
  const storyRegion = document.querySelector('.story-media');
  const storySlides = [...story.querySelectorAll('img')];
  const storyCount = document.querySelector('.story-count');
  let storyVisible = false;
  let storyIndex = 0;
  let storyTimer;
  let storyTransition;
  document.querySelector('.story-controls').hidden = false;

  function showStory(index) {
    const nextIndex = (index + storySlides.length) % storySlides.length;
    const next = storySlides[nextIndex];
    if (nextIndex !== storyIndex && next.complete && next.naturalWidth) {
      clearTimeout(storyTransition);
      storySlides.forEach(slide => slide.classList.remove('is-previous'));
      const previous = storySlides[storyIndex];
      previous.classList.add('is-previous');
      previous.classList.remove('is-current');
      previous.setAttribute('aria-hidden', 'true');
      next.classList.add('is-current');
      next.setAttribute('aria-hidden', 'false');
      storyIndex = nextIndex;
      storyCount.firstChild.textContent = `${String(storyIndex + 1).padStart(2, '0')} `;
      storyCount.setAttribute('aria-label', `Image ${storyIndex + 1} of ${storySlides.length}`);
      storyTransition = setTimeout(() => previous.classList.remove('is-previous'), 700);
    }
    syncStory();
  }

  function syncStory() {
    clearTimeout(storyTimer);
    storyRegion.classList.remove('is-advancing');
    if (!storyVisible || document.hidden || lightbox.open || reducedMotion.matches || saveData) return;
    // Restart the progress line with the next full interval.
    void storyRegion.offsetWidth;
    storyRegion.classList.add('is-advancing');
    storyTimer = setTimeout(() => showStory(storyIndex + 1), 4000);
  }
  document.getElementById('story-previous').addEventListener('click', () => showStory(storyIndex - 1));
  document.getElementById('story-next').addEventListener('click', () => showStory(storyIndex + 1));
  storyRegion.addEventListener('keydown', event => {
    if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
    event.preventDefault();
    showStory(storyIndex + (event.key === 'ArrowLeft' ? -1 : 1));
  });
  const music = document.getElementById('site-music');
  const musicToggle = document.getElementById('music-toggle');
  let musicEnabled = true;
  let musicRequest = 0;
  // The audio file carries the playback level so it also applies on iOS.
  musicToggle.hidden = false;

  function renderMusic() {
    const playing = !music.paused && !music.error;
    const label = music.error ? 'Music unavailable' : playing ? 'Mute music' : 'Play music';
    musicToggle.disabled = Boolean(music.error);
    musicToggle.classList.toggle('is-playing', playing);
    musicToggle.setAttribute('aria-pressed', String(playing));
    musicToggle.setAttribute('aria-label', label);
    musicToggle.title = label;
  }

  function syncMusic() {
    const request = ++musicRequest;
    if (!musicEnabled || document.hidden || lightbox.open || music.error) {
      music.pause();
      renderMusic();
      return;
    }
    if (!music.getAttribute('src')) music.src = music.dataset.src;
    music.play().then(() => {
      if (request === musicRequest) renderMusic();
    }).catch(error => {
      if (request !== musicRequest) return;
      // Keep the default preference when the browser requires a user gesture.
      if (error.name !== 'NotAllowedError' && error.name !== 'AbortError') musicEnabled = false;
      renderMusic();
    });
  }
  musicToggle.addEventListener('click', () => {
    musicEnabled = music.paused;
    syncMusic();
  });
  music.addEventListener('playing', renderMusic);
  music.addEventListener('pause', renderMusic);
  music.addEventListener('error', () => {
    musicRequest++;
    musicEnabled = false;
    music.pause();
    renderMusic();
  });
  function resumeMusicOnGesture(event) {
    if (!event.isTrusted || musicToggle.contains(event.target) || !musicEnabled || !music.paused) return;
    syncMusic();
  }
  document.addEventListener('click', resumeMusicOnGesture, {capture: true});
  document.addEventListener('touchend', resumeMusicOnGesture, {capture: true, passive: true});
  document.addEventListener('keydown', event => {
    if (event.key === 'Enter' || event.key === ' ') resumeMusicOnGesture(event);
  }, {capture: true});
  syncMusic();

  const items = [...document.querySelectorAll('[data-media]')];
  const media = items.map(link => ({
    src: link.getAttribute('href'),
    kind: link.dataset.kind || 'image',
    poster: link.dataset.poster,
    caption: link.dataset.caption,
    alt: link.dataset.alt,
  }));
  const galleryVideos = new Map([...document.querySelectorAll('.gallery-grid video')].map(video => [video, false]));
  const largeImage = document.getElementById('lightbox-image');
  const largeVideo = document.getElementById('lightbox-video');
  let current = 0;
  function syncGallery() {
    for (const [video, visible] of galleryVideos) {
      if (visible && !document.hidden && !lightbox.open && !reducedMotion.matches && !saveData) {
        video.play().catch(() => {});
      } else {
        video.pause();
      }
    }
  }

  function showMedia(index) {
    current = (index + media.length) % media.length;
    const item = media[current];
    largeVideo.pause();
    largeVideo.hidden = item.kind !== 'video';
    largeImage.hidden = item.kind === 'video';
    if (item.kind === 'video') {
      largeVideo.src = item.src;
      largeVideo.poster = item.poster;
      largeVideo.setAttribute('aria-label', item.alt);
      if (!reducedMotion.matches) largeVideo.play().catch(() => {});
    } else {
      largeImage.src = item.src;
      largeImage.alt = item.alt;
    }
    document.getElementById('lightbox-caption').textContent = `${current + 1} / ${media.length} · ${item.caption}`;
  }

  items.forEach((link, index) => link.addEventListener('click', event => {
    if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey || typeof lightbox.showModal !== 'function') return;
    event.preventDefault();
    lightbox.showModal();
    syncHero();
    syncGallery();
    syncStory();
    syncMusic();
    showMedia(index);
  }));
  document.getElementById('lightbox-previous').addEventListener('click', () => showMedia(current - 1));
  document.getElementById('lightbox-next').addEventListener('click', () => showMedia(current + 1));
  lightbox.querySelector('.lightbox-close').addEventListener('click', () => lightbox.close());
  lightbox.addEventListener('click', event => {
    if (event.target === lightbox || event.target.classList.contains('lightbox-stage')) lightbox.close();
  });
  lightbox.addEventListener('close', () => {
    largeVideo.pause();
    syncHero();
    syncGallery();
    syncStory();
    syncMusic();
  });
  lightbox.addEventListener('keydown', event => {
    if (event.target === largeVideo) return;
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
      event.preventDefault();
      showMedia(current + (event.key === 'ArrowLeft' ? -1 : 1));
    }
  });
  if ('IntersectionObserver' in window) {
    const observer = new IntersectionObserver(entries => {
      for (const entry of entries) {
        if (entry.target === hero) {
          heroVisible = entry.isIntersecting;
          syncHero();
        } else if (galleryVideos.has(entry.target)) {
          galleryVideos.set(entry.target, entry.isIntersecting);
          syncGallery();
        } else {
          storyVisible = entry.isIntersecting;
          syncStory();
        }
      }
    }, {threshold: .1});
    observer.observe(hero);
    galleryVideos.forEach((_, video) => observer.observe(video));
    observer.observe(story);
  } else {
    heroVisible = true;
    galleryVideos.forEach((_, video) => galleryVideos.set(video, true));
    storyVisible = true;
    syncHero();
    syncGallery();
    syncStory();
  }
  document.addEventListener('visibilitychange', () => {
    syncHero();
    syncGallery();
    syncStory();
    syncMusic();
    if (document.hidden) largeVideo.pause();
  });
  reducedMotion.addEventListener('change', () => {
    if (reducedMotion.matches) {
      hero.classList.remove('is-playing');
      largeVideo.pause();
    }
    syncHero();
    syncGallery();
    syncStory();
  });
  const releasePage = 'https://github.com/yyan115/GAM300/releases/latest';
  const extensions = {windows: '.exe', linux: '.AppImage', android: '.apk'};
  const sizeLabel = bytes => {
    if (!Number.isFinite(bytes) || bytes <= 0) return '';
    const gb = bytes / 1024 ** 3;
    return gb >= 1 ? `${gb.toFixed(2)} GB` : `${(bytes / 1024 ** 2).toFixed(0)} MB`;
  };

  async function loadRelease() {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 7000);
    try {
      const response = await fetch('https://api.github.com/repos/yyan115/GAM300/releases/latest', {signal: controller.signal});
      if (!response.ok) return;
      const release = await response.json();
      if (!Array.isArray(release.assets)) return;
      document.querySelectorAll('[data-platform]').forEach(row => {
        const asset = release.assets.find(file => typeof file.name === 'string' && file.name.endsWith(extensions[row.dataset.platform]));
        if (!asset) {
          row.href = releasePage;
          row.querySelector('.download-action').firstChild.textContent = 'View releases ';
          row.querySelector('.download-name small').textContent = 'Not included in this release';
          row.setAttribute('aria-label', `View releases for ${row.querySelector('.download-name').firstChild.textContent}`);
          return;
        }
        const url = new URL(asset.browser_download_url);
        if (url.origin !== 'https://github.com' || !url.pathname.startsWith('/yyan115/GAM300/releases/download/')) return;
        row.href = url.href;
        const size = sizeLabel(asset.size);
        row.querySelector('[data-size]').textContent = size ? `· ${size}` : '';
      });
    } catch {
      // Stable download links remain usable if release metadata is unavailable.
    } finally {
      clearTimeout(timeout);
    }
  }
  loadRelease();
})();
