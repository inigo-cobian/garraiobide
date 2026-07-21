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
  var currentLayer = null;       // L.GeoJSON layer currently on the map
  var queryResultsLayer = null;  // L.GeoJSON layer for spatial query results
  var activeLayerName = null;    // Name of the currently selected layer

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
  function createGeoJsonLayer(geojsonData, style) {
    var layerStyle = style || {
      color: '#2563eb',
      weight: 2,
      opacity: 0.8,
      fillColor: '#2563eb',
      fillOpacity: 0.3
    };

    return L.geoJSON(geojsonData, {
      pointToLayer: function (feature, latlng) {
        return L.circleMarker(latlng, {
          radius: 7,
          color: layerStyle.color,
          weight: layerStyle.weight,
          opacity: layerStyle.opacity,
          fillColor: layerStyle.fillColor,
          fillOpacity: layerStyle.fillOpacity
        });
      },
      style: function () {
        return layerStyle;
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
      li.textContent = layerNames[i];
      li.setAttribute('tabindex', '0');
      li.setAttribute('role', 'button');
      li.setAttribute('aria-pressed', 'false');
      li.dataset.layerName = layerNames[i];

      li.addEventListener('click', onLayerItemClick);
      li.addEventListener('keydown', function (e) {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          e.target.click();
        }
      });

      layerListEl.appendChild(li);
    }
  }

  function onLayerItemClick(e) {
    var name = e.target.dataset.layerName;
    selectLayer(name);
  }

  function selectLayer(name) {
    // Update active state in UI
    var items = layerListEl.querySelectorAll('li');
    for (var i = 0; i < items.length; i++) {
      if (items[i].dataset.layerName === name) {
        items[i].classList.add('active');
        items[i].setAttribute('aria-pressed', 'true');
      } else {
        items[i].classList.remove('active');
        items[i].setAttribute('aria-pressed', 'false');
      }
    }

    activeLayerName = name;

    // Fetch and render layer
    fetch(API_BASE + '/api/layers/' + encodeURIComponent(name))
      .then(function (response) {
        if (!response.ok) {
          throw new Error('Failed to fetch layer "' + name + '": ' + response.status);
        }
        return response.json();
      })
      .then(function (geojsonData) {
        renderLayer(geojsonData);
      })
      .catch(function (err) {
        showNotification('Error loading layer: ' + err.message, 'error');
      });
  }

  function renderLayer(geojsonData) {
    // Remove previous layer
    if (currentLayer) {
      map.removeLayer(currentLayer);
      currentLayer = null;
    }

    currentLayer = createGeoJsonLayer(geojsonData);
    currentLayer.addTo(map);

    // Fit bounds (Req 6.5)
    var bounds = currentLayer.getBounds();
    if (bounds.isValid()) {
      map.fitBounds(bounds, { padding: [30, 30] });
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

    queryResultsLayer = createGeoJsonLayer(geojsonData, queryStyle);
    queryResultsLayer.addTo(map);
  }

  // ---------------------------------------------------------------------------
  // Initialize on Load (Req 6.2)
  // ---------------------------------------------------------------------------
  fetchLayers();

})();
