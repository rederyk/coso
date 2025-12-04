(function () {
  const NAV_ITEMS = [
    {
      id: 'assistant',
      label: 'Voice Assistant',
      href: '/index.html',
      icon: '🎙️',
      match: (path) => path === '/' || path.endsWith('/') || path.includes('index'),
    },
    {
      id: 'lua',
      label: 'Lua Console',
      href: '/lua-console.html',
      icon: '🧪',
      match: (path) => path.includes('lua-console'),
    },
    {
      id: 'files',
      label: 'File Manager',
      href: '/file-manager.html',
      icon: '📁',
      match: (path) => path.includes('file-manager'),
    },
    {
      id: 'settings',
      label: 'Settings',
      href: '#',
      icon: '⚙️',
      disabled: true,
      chip: 'soon',
      match: () => false,
    },
  ];

  function buildNav(activeId) {
    const nav = document.createElement('nav');
    nav.className = 'app-nav';
    nav.innerHTML = `
      <div class="app-nav__brand">
        <span>ESP32 Companion</span>
      </div>
      <ul class="app-nav__links"></ul>
    `;
    const list = nav.querySelector('.app-nav__links');
    NAV_ITEMS.forEach((item) => {
      const li = document.createElement('li');
      const link = document.createElement('a');
      link.className = 'app-nav__link';
      link.innerHTML = `<span>${item.icon}</span><span>${item.label}</span>`;
      link.href = item.href;
      if (item.disabled) {
        link.classList.add('disabled');
        link.href = 'javascript:void(0);';
      }
      if (item.chip) {
        const chip = document.createElement('span');
        chip.className = 'chip';
        chip.textContent = item.chip;
        link.appendChild(chip);
      }
      if (activeId === item.id) {
        link.classList.add('active');
      }
      li.appendChild(link);
      list.appendChild(li);
    });
    return nav;
  }

  function deduceActiveId() {
    const explicit = document.documentElement?.dataset?.page;
    if (explicit) return explicit;
    const path = window.location.pathname || '/';
    const match = NAV_ITEMS.find((item) => item.match(path));
    return match ? match.id : null;
  }

  function mountNav() {
    const target = document.querySelector('[data-app-nav]') || document.querySelector('main');
    if (!target) return;
    const nav = buildNav(deduceActiveId());
    if (target.hasAttribute('data-app-nav')) {
      target.appendChild(nav);
    } else {
      target.insertBefore(nav, target.firstChild);
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', mountNav);
  } else {
    mountNav();
  }
}());
