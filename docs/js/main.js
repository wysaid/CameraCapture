/**
 * ccap Documentation Site
 * Main JavaScript for navigation and dynamic content loading
 */

(function() {
    'use strict';

    // Language management
    function toggleLanguage() {
        const html = document.documentElement;
        const currentLang = html.getAttribute('lang');
        const newLang = currentLang === 'en' ? 'zh' : 'en';
        html.setAttribute('lang', newLang);
        localStorage.setItem('ccap-lang', newLang);
        updateLanguageButton();
    }

    function updateLanguageButton() {
        const btn = document.querySelector('.lang-switch');
        if (btn) {
            const currentLang = document.documentElement.getAttribute('lang');
            btn.textContent = currentLang === 'en' ? '🌐 中文' : '🌐 English';
        }
    }

    function initLanguage() {
        const savedLang = localStorage.getItem('ccap-lang');
        const browserLang = navigator.language.startsWith('zh') ? 'zh' : 'en';
        const lang = savedLang || browserLang;
        document.documentElement.setAttribute('lang', lang);
        updateLanguageButton();
    }

    // Code tabs
    function showCode(lang) {
        document.querySelectorAll('.code-content').forEach(function(el) {
            el.classList.remove('active');
        });
        document.querySelectorAll('.code-tab').forEach(function(el) {
            el.classList.remove('active');
        });
        var codeElement = document.getElementById('code-' + lang);
        if (codeElement) {
            codeElement.classList.add('active');
        }
        if (event && event.target) {
            event.target.classList.add('active');
        }
    }

    // GitHub Release API
    var GITHUB_REPO = 'wysaid/CameraCapture';
    var cachedRelease = null;

    function fetchLatestRelease(callback) {
        if (cachedRelease) {
            callback(cachedRelease);
            return;
        }

        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'https://api.github.com/repos/' + GITHUB_REPO + '/releases/latest', true);
        xhr.setRequestHeader('Accept', 'application/vnd.github.v3+json');
        xhr.onload = function() {
            if (xhr.status >= 200 && xhr.status < 300) {
                try {
                    cachedRelease = JSON.parse(xhr.responseText);
                    callback(cachedRelease);
                } catch (e) {
                    callback(null);
                }
            } else {
                callback(null);
            }
        };
        xhr.onerror = function() {
            callback(null);
        };
        xhr.send();
    }

    function updateDownloadSection() {
        var versionElements = document.querySelectorAll('.latest-version');
        var downloadContainer = document.getElementById('download-links');

        fetchLatestRelease(function(release) {
            if (release) {
                // Update version display
                versionElements.forEach(function(el) {
                    el.textContent = release.tag_name;
                });

                // Build download links
                if (downloadContainer) {
                    var assets = release.assets || [];
                    var html = '';

                    // Categorize assets by platform
                    var platforms = {
                        windows: [],
                        macos: [],
                        linux: [],
                        ios: [],
                        other: []
                    };

                    assets.forEach(function(asset) {
                        var name = asset.name.toLowerCase();
                        if (name.indexOf('windows') !== -1 || name.indexOf('win') !== -1) {
                            platforms.windows.push(asset);
                        } else if (name.indexOf('macos') !== -1 || name.indexOf('darwin') !== -1 || name.indexOf('mac') !== -1) {
                            platforms.macos.push(asset);
                        } else if (name.indexOf('linux') !== -1) {
                            platforms.linux.push(asset);
                        } else if (name.indexOf('ios') !== -1) {
                            platforms.ios.push(asset);
                        } else {
                            platforms.other.push(asset);
                        }
                    });

                    html += '<div class="download-grid">';

                    if (platforms.windows.length > 0) {
                        html += '<div class="download-platform">';
                        html += '<div class="platform-icon">🪟</div>';
                        html += '<h4>Windows</h4>';
                        platforms.windows.forEach(function(asset) {
                            html += '<a href="' + asset.browser_download_url + '" class="download-link">' + asset.name + '</a>';
                        });
                        html += '</div>';
                    }

                    if (platforms.macos.length > 0) {
                        html += '<div class="download-platform">';
                        html += '<div class="platform-icon">🍎</div>';
                        html += '<h4>macOS</h4>';
                        platforms.macos.forEach(function(asset) {
                            html += '<a href="' + asset.browser_download_url + '" class="download-link">' + asset.name + '</a>';
                        });
                        html += '</div>';
                    }

                    if (platforms.linux.length > 0) {
                        html += '<div class="download-platform">';
                        html += '<div class="platform-icon">🐧</div>';
                        html += '<h4>Linux</h4>';
                        platforms.linux.forEach(function(asset) {
                            html += '<a href="' + asset.browser_download_url + '" class="download-link">' + asset.name + '</a>';
                        });
                        html += '</div>';
                    }

                    if (platforms.ios.length > 0) {
                        html += '<div class="download-platform">';
                        html += '<div class="platform-icon">📱</div>';
                        html += '<h4>iOS</h4>';
                        platforms.ios.forEach(function(asset) {
                            html += '<a href="' + asset.browser_download_url + '" class="download-link">' + asset.name + '</a>';
                        });
                        html += '</div>';
                    }

                    if (platforms.other.length > 0) {
                        html += '<div class="download-platform">';
                        html += '<div class="platform-icon">📦</div>';
                        html += '<h4 class="lang-en">Other</h4><h4 class="lang-zh">其他</h4>';
                        platforms.other.forEach(function(asset) {
                            html += '<a href="' + asset.browser_download_url + '" class="download-link">' + asset.name + '</a>';
                        });
                        html += '</div>';
                    }

                    html += '</div>';

                    html += '<div class="download-footer">';
                    html += '<a href="https://github.com/' + GITHUB_REPO + '/releases/tag/' + release.tag_name + '" class="btn btn-outline">';
                    html += '<span class="lang-en">View Release Notes</span><span class="lang-zh">查看版本说明</span>';
                    html += '</a>';
                    html += '<a href="https://github.com/' + GITHUB_REPO + '/releases" class="btn btn-outline">';
                    html += '<span class="lang-en">All Releases</span><span class="lang-zh">所有版本</span>';
                    html += '</a>';
                    html += '</div>';

                    downloadContainer.innerHTML = html;
                    
                    // Re-apply language after updating content
                    initLanguage();
                }
            } else {
                // Fallback if API fails
                versionElements.forEach(function(el) {
                    el.textContent = 'Latest';
                });
                if (downloadContainer) {
                    downloadContainer.innerHTML = '<p class="lang-en">Unable to fetch release info. Please visit <a href="https://github.com/' + GITHUB_REPO + '/releases">GitHub Releases</a>.</p>' +
                        '<p class="lang-zh">无法获取版本信息，请访问 <a href="https://github.com/' + GITHUB_REPO + '/releases">GitHub Releases</a>。</p>';
                    initLanguage();
                }
            }
        });
    }

    // Navigation handling
    function initNavigation() {
        var navLinks = document.querySelectorAll('.nav-links a[data-page]');
        navLinks.forEach(function(link) {
            link.addEventListener('click', function(e) {
                var page = this.getAttribute('data-page');
                if (page) {
                    e.preventDefault();
                    loadPage(page);
                }
            });
        });
    }

    function loadPage(pageName) {
        // For now, we'll use hash-based navigation
        // This can be expanded to use fetch() for AJAX loading
        window.location.hash = pageName;
    }

    // Mobile menu toggle
    function initMobileMenu() {
        var menuToggle = document.querySelector('.mobile-menu-toggle');
        var navLinks = document.querySelector('.nav-links');
        
        if (menuToggle && navLinks) {
            menuToggle.addEventListener('click', function() {
                navLinks.classList.toggle('active');
                menuToggle.classList.toggle('active');
            });
        }
    }

    // Smooth scrolling for anchor links
    function initSmoothScroll() {
        document.querySelectorAll('a[href^="#"]').forEach(function(anchor) {
            anchor.addEventListener('click', function(e) {
                var href = this.getAttribute('href');
                if (href.length > 1) {
                    var target = document.querySelector(href);
                    if (target) {
                        e.preventDefault();
                        target.scrollIntoView({
                            behavior: 'smooth',
                            block: 'start'
                        });
                    }
                }
            });
        });
    }

    // Initialize everything when DOM is ready
    function init() {
        initLanguage();
        initNavigation();
        initMobileMenu();
        initSmoothScroll();
        updateDownloadSection();
    }

    // Expose functions globally
    window.ccap = {
        toggleLanguage: toggleLanguage,
        showCode: showCode,
        updateDownloadSection: updateDownloadSection
    };

    // Run init when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
