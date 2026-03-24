// Injects Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers
// into every response, enabling SharedArrayBuffer on GitHub Pages.
// Technique: https://github.com/gzuidhof/coi-serviceworker

'use strict';

self.addEventListener('install', function () {
  self.skipWaiting();
});

self.addEventListener('activate', function (event) {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', function (event) {
  // Skip cross-origin requests that would fail CORS preflight
  if (event.request.cache === 'only-if-cached' && event.request.mode !== 'same-origin') {
    return;
  }

  event.respondWith(
    fetch(event.request).then(function (response) {
      if (response.status === 0) return response;

      var headers = new Headers(response.headers);
      headers.set('Cross-Origin-Opener-Policy', 'same-origin');
      headers.set('Cross-Origin-Embedder-Policy', 'require-corp');

      return new Response(response.body, {
        status: response.status,
        statusText: response.statusText,
        headers: headers,
      });
    }).catch(function () {
      return fetch(event.request);
    })
  );
});
