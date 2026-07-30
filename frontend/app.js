(function () {
  'use strict';

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------
  var API_BASE = 'http://localhost:8080';

  // ---------------------------------------------------------------------------
  // Map Initialization (Req 6.1)
  // ---------------------------------------------------------------------------
  var map = L.map('map').setView([43.26, -2.93], 13);

  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
  }).addTo(map);

  // ---------------------------------------------------------------------------
  // State
  // ---------------------------------------------------------------------------
  var activeLayers = {};         // { layerName: L.GeoJSON } — multiple layers on the map
  var queryResultsLayer = null;  // L.GeoJSON layer for spatial query results

  // ---------------------------------------------------------------------------
  // Notification System (Req 6.6, 8.5)
  // ---------------------------------------------------------------------------
  var notificationsEl = document.getElementById('notifications');

  function showNotification(message, type) {
    var el = document.createElement('div');
    el.className = 'notification' + (type ? ' notification--' + type : '');
    el.textContent = message;
    notificationsEl.appendChild(el);

    setTimeout(function () {
      el.classList.add('notification--fade-out');
      el.addEventListener('animationend', function () {
        if (el.parentNode) {
          el.parentNode.removeChild(el);
        }
      });
    }, 3000);
  }

  // ---------------------------------------------------------------------------
  // Popup Generation (Req 7.1, 7.2)
  // ---------------------------------------------------------------------------
  function buildPopupContent(feature) {
    var props = feature.properties || {};
    var id = feature.id;
    var html = '';

    if (id) {
      html += '<h3 class="feature-popup__heading">' + escapeHtml(String(id)) + '</h3>';
    }

    var keys = Object.keys(props);
    if (keys.length > 0) {
      html += '<table class="feature-popup__table"><tbody>';
      for (var i = 0; i < keys.length; i++) {
        var key = keys[i];
        var value = props[key];
        html += '<tr><th>' + escapeHtml(key) + '</th><td>' + escapeHtml(String(value)) + '</td></tr>';
      }
      html += '</tbody></table>';
    }

    return html || '<em>No properties</em>';
  }

  function escapeHtml(str) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(str));
    return div.innerHTML;
  }

  // ---------------------------------------------------------------------------
  // GeoJSON Rendering Helpers (Req 6.3, 6.4, 7.1, 7.3)
  // ---------------------------------------------------------------------------
  var DEFAULT_ROUTE_COLOR = '#2563eb';

  function isStopsLayer(name) {
    return /stops/i.test(name);
  }

  function isRoutesLayer(name) {
    return /routes/i.test(name);
  }

  function getRouteColor(feature) {
    var props = feature.properties || {};
    var raw = props.route_color;
    if (raw && /^[0-9A-Fa-f]{6}$/.test(raw)) {
      return '#' + raw;
    }
    return DEFAULT_ROUTE_COLOR;
  }

  function getStopStyle(feature) {
    var props = feature.properties || {};
    var isMain = props.stop_type === 'parent_station';
    var color = isMain ? '#000000' : '#888888';
    var radius = isMain ? 8 : 5;
    return {
      radius: radius,
      color: color,
      weight: isMain ? 2 : 1,
      opacity: 0.9,
      fillColor: color,
      fillOpacity: isMain ? 0.7 : 0.4
    };
  }

  function createGeoJsonLayer(geojsonData, options) {
    options = options || {};
    var layerType = options.layerType || 'generic'; // 'routes', 'stops', or 'generic'
    var fallbackStyle = options.style || {
      color: '#2563eb',
      weight: 2,
      opacity: 0.8,
      fillColor: '#2563eb',
      fillOpacity: 0.3
    };

    return L.geoJSON(geojsonData, {
      pointToLayer: function (feature, latlng) {
        if (layerType === 'stops') {
          var stopStyle = getStopStyle(feature);
          return L.circleMarker(latlng, stopStyle);
        }
        return L.circleMarker(latlng, {
          radius: 7,
          color: fallbackStyle.color,
          weight: fallbackStyle.weight,
          opacity: fallbackStyle.opacity,
          fillColor: fallbackStyle.fillColor,
          fillOpacity: fallbackStyle.fillOpacity
        });
      },
      style: function (feature) {
        if (layerType === 'routes') {
          var routeColor = getRouteColor(feature);
          return {
            color: routeColor,
            weight: 3,
            opacity: 0.85,
            fillColor: routeColor,
            fillOpacity: 0.3
          };
        }
        return fallbackStyle;
      },
      onEachFeature: function (feature, layer) {
        layer.on('click', function (e) {
          L.DomEvent.stopPropagation(e);
          var popup = L.popup()
            .setLatLng(e.latlng)
            .setContent(buildPopupContent(feature))
            .openOn(map);
        });
      }
    });
  }

  // ---------------------------------------------------------------------------
  // Close popup on map click (Req 7.3)
  // ---------------------------------------------------------------------------
  map.on('click', function () {
    map.closePopup();
  });

  // ---------------------------------------------------------------------------
  // Layer Fetching and Selection (Req 6.2, 6.3, 6.5)
  // ---------------------------------------------------------------------------
  var layerListEl = document.getElementById('layer-list');

  function fetchLayers() {
    fetch(API_BASE + '/api/layers')
      .then(function (response) {
        if (!response.ok) {
          throw new Error('Failed to fetch layers: ' + response.status);
        }
        return response.json();
      })
      .then(function (layerNames) {
        populateLayerList(layerNames);
      })
      .catch(function (err) {
        showNotification('Error loading layers: ' + err.message, 'error');
      });
  }

  function populateLayerList(layerNames) {
    layerListEl.innerHTML = '';
    for (var i = 0; i < layerNames.length; i++) {
      var li = document.createElement('li');
      li.className = 'layer-control__item';

      var label = document.createElement('label');
      label.className = 'layer-control__label';

      var checkbox = document.createElement('input');
      checkbox.type = 'checkbox';
      checkbox.className = 'layer-control__checkbox';
      checkbox.dataset.layerName = layerNames[i];
      checkbox.checked = !!activeLayers[layerNames[i]];
      checkbox.setAttribute('aria-label', 'Toggle layer ' + layerNames[i]);

      checkbox.addEventListener('change', onLayerToggle);

      var span = document.createElement('span');
      span.className = 'layer-control__name';
      span.textContent = layerNames[i];

      label.appendChild(checkbox);
      label.appendChild(span);
      li.appendChild(label);
      layerListEl.appendChild(li);
    }
  }

  function onLayerToggle(e) {
    var name = e.target.dataset.layerName;
    if (e.target.checked) {
      addLayer(name);
    } else {
      removeLayer(name);
    }
  }

  // Layer default style for generic layers without data-driven color
  var DEFAULT_LAYER_STYLE = {
    color: '#2563eb',
    weight: 2,
    opacity: 0.8,
    fillColor: '#2563eb',
    fillOpacity: 0.3
  };

  function addLayer(name) {
    // Already loaded
    if (activeLayers[name]) return;

    fetch(API_BASE + '/api/layers/' + encodeURIComponent(name))
      .then(function (response) {
        if (!response.ok) {
          throw new Error('Failed to fetch layer "' + name + '": ' + response.status);
        }
        return response.json();
      })
      .then(function (geojsonData) {
        var layerType = 'generic';
        if (isStopsLayer(name)) {
          layerType = 'stops';
        } else if (isRoutesLayer(name)) {
          layerType = 'routes';
        }

        var layer = createGeoJsonLayer(geojsonData, {
          layerType: layerType,
          style: DEFAULT_LAYER_STYLE
        });
        layer.addTo(map);
        activeLayers[name] = layer;

        // Fit bounds (Req 6.5)
        var bounds = layer.getBounds();
        if (bounds.isValid()) {
          map.fitBounds(bounds, { padding: [30, 30] });
        }
      })
      .catch(function (err) {
        showNotification('Error loading layer: ' + err.message, 'error');
        // Uncheck the checkbox on failure
        var cb = layerListEl.querySelector('input[data-layer-name="' + name + '"]');
        if (cb) cb.checked = false;
      });
  }

  function removeLayer(name) {
    if (activeLayers[name]) {
      map.removeLayer(activeLayers[name]);
      delete activeLayers[name];
    }
  }

  // ---------------------------------------------------------------------------
  // Leaflet.draw Rectangle Control (Req 8.1, 8.2, 8.3, 8.4, 8.5)
  // ---------------------------------------------------------------------------
  var drawnItems = new L.FeatureGroup();
  map.addLayer(drawnItems);

  var drawControl = new L.Control.Draw({
    draw: {
      rectangle: true,
      polygon: false,
      polyline: false,
      circle: false,
      circlemarker: false,
      marker: false
    },
    edit: {
      featureGroup: drawnItems
    }
  });
  map.addControl(drawControl);

  map.on(L.Draw.Event.CREATED, function (event) {
    var layer = event.layer;
    var bounds = layer.getBounds();

    var minLat = bounds.getSouthWest().lat;
    var minLng = bounds.getSouthWest().lng;
    var maxLat = bounds.getNorthEast().lat;
    var maxLng = bounds.getNorthEast().lng;

    performSpatialQuery(minLat, minLng, maxLat, maxLng);
  });

  function performSpatialQuery(minLat, minLng, maxLat, maxLng) {
    var url = API_BASE + '/api/query' +
      '?min_lat=' + minLat +
      '&min_lng=' + minLng +
      '&max_lat=' + maxLat +
      '&max_lng=' + maxLng;

    fetch(url)
      .then(function (response) {
        if (!response.ok) {
          throw new Error('Spatial query failed: ' + response.status);
        }
        return response.json();
      })
      .then(function (geojsonData) {
        renderQueryResults(geojsonData);
      })
      .catch(function (err) {
        showNotification('Query error: ' + err.message, 'error');
      });
  }

  function renderQueryResults(geojsonData) {
    // Remove previous query results layer (Req 8.4)
    if (queryResultsLayer) {
      map.removeLayer(queryResultsLayer);
      queryResultsLayer = null;
    }

    // Check for zero results (Req 8.5)
    if (!geojsonData.features || geojsonData.features.length === 0) {
      showNotification('No features found in the selected area.', 'info');
      return;
    }

    // Render with distinct styling (Req 8.3)
    var queryStyle = {
      color: '#dc2626',
      weight: 3,
      opacity: 0.9,
      fillColor: '#f59e0b',
      fillOpacity: 0.4
    };

    queryResultsLayer = createGeoJsonLayer(geojsonData, { style: queryStyle });
    queryResultsLayer.addTo(map);
  }

  // ---------------------------------------------------------------------------
  // Layer List Refresh (Req 4.1, 4.2, 4.3, 4.4, 4.5)
  // ---------------------------------------------------------------------------
  function refreshLayerList() {
    fetch(API_BASE + '/api/layers')
      .then(function (response) {
        if (!response.ok) throw new Error('Failed to refresh');
        return response.json();
      })
      .then(function (layerNames) {
        // Remove layers that no longer exist on the server
        var activeNames = Object.keys(activeLayers);
        for (var i = 0; i < activeNames.length; i++) {
          if (layerNames.indexOf(activeNames[i]) === -1) {
            removeLayer(activeNames[i]);
          }
        }
        populateLayerList(layerNames);
      })
      .catch(function (err) {
        showNotification('Could not refresh layer list', 'error');
      });
  }

  // ---------------------------------------------------------------------------
  // GTFS Upload Logic (Req 1.3, 1.4, 1.5, 1.7, 1.8, 1.9, 5.1–5.6, 6.1, 6.2)
  // ---------------------------------------------------------------------------
  var fileInput = document.getElementById('gtfs-file-input');
  var submitBtn = document.getElementById('gtfs-submit-btn');
  var statusEl = document.getElementById('gtfs-status');

  fileInput.addEventListener('change', function() {
    submitBtn.disabled = !fileInput.files.length;
  });

  submitBtn.addEventListener('click', function() {
    var file = fileInput.files[0];
    if (!file) return;

    // Extension validation (case-insensitive)
    if (!file.name.match(/\.zip$/i)) {
      showNotification('Please select a .zip file', 'error');
      return;
    }

    // Size validation (50 MB)
    if (file.size > 50 * 1024 * 1024) {
      showNotification('File exceeds the 50 MB size limit', 'error');
      return;
    }

    // Disable controls, show loading
    submitBtn.disabled = true;
    fileInput.disabled = true;
    statusEl.textContent = 'Importing...';

    var formData = new FormData();
    formData.append('file', file);

    var controller = new AbortController();
    var timeoutId = setTimeout(function() { controller.abort(); }, 30000);

    fetch(API_BASE + '/api/ingest/gtfs', {
      method: 'POST',
      body: formData,
      signal: controller.signal
    })
    .then(function(response) {
      clearTimeout(timeoutId);
      return response.json().then(function(data) {
        return { status: response.status, body: data };
      });
    })
    .then(function(result) {
      if (result.status === 200) {
        showNotification('GTFS import complete', 'success');
        refreshLayerList();
      } else {
        showNotification(result.body.error || 'Upload failed', 'error');
      }
    })
    .catch(function(err) {
      clearTimeout(timeoutId);
      showNotification('Import failed: network error', 'error');
    })
    .finally(function() {
      statusEl.textContent = '';
      submitBtn.disabled = false;
      fileInput.disabled = false;
    });
  });

  // ---------------------------------------------------------------------------
  // Initialize on Load (Req 6.2)
  // ---------------------------------------------------------------------------
  fetchLayers();

})();
