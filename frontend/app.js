(function () {
  'use strict';

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------
  var API_BASE = 'http://localhost:8080';

  // Map Initialization
  var map = L.map('map').setView([43.26, -2.93], 13);

  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
  }).addTo(map);

  // State
  var activeLayers = {};         // { layerName: L.GeoJSON } — multiple layers on the map
  var secondaryStopsLayers = {}; // { layerName: L.LayerGroup } — child stops + connector lines
  var connectorLineLayers = {};  // { layerName: L.LayerGroup } — dashed lines only
  var stopsGeoJsonCache = {};    // { layerName: raw GeoJSON } — cached for toggling secondary
  var showSecondary = {};        // { layerName: bool }
  var queryResultsLayer = null;  // L.GeoJSON layer for spatial query results

  // Route filtering state
  var routesGeoJsonCache = {};   // { layerName: raw GeoJSON } — cached for route filtering
  var routeVisibility = {};      // { layerName: { routeId: bool } } — per-route visibility
  var routeSubLayers = {};       // { layerName: { routeId: L.GeoJSON } } — per-route map layers

  // Notification System
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

  // Popup Generation
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

  // GeoJSON Rendering Helpers
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

  function getRouteDisplayName(feature) {
    var props = feature.properties || {};
    return props.route_long_name || props.route_short_name || feature.id || 'Unknown route';
  }

  function getRouteId(feature) {
    return feature.id || (feature.properties && feature.properties.route_short_name) || getRouteDisplayName(feature);
  }

  function extractUniqueRoutes(geojsonData) {
    var routeMap = {};
    for (var i = 0; i < geojsonData.features.length; i++) {
      var f = geojsonData.features[i];
      var id = getRouteId(f);
      if (!routeMap[id]) {
        routeMap[id] = {
          id: id,
          displayName: getRouteDisplayName(f),
          color: getRouteColor(f)
        };
      }
    }
    var routes = Object.keys(routeMap).map(function (k) { return routeMap[k]; });
    routes.sort(function (a, b) { return a.displayName.localeCompare(b.displayName); });
    return routes;
  }

  function getStopRadius(zoom, isMain) {
    // Scale radius with zoom: base sizes at zoom 13, grow/shrink from there
    var base = isMain ? 7 : 4;
    var scale = Math.max(0.5, (zoom - 8) / 5);
    return Math.round(base * scale);
  }

  function getStopStyle(feature, zoom) {
    var props = feature.properties || {};
    var isMain = props.stop_type === 'parent_station';
    var borderColor = isMain ? '#000000' : '#888888';
    var radius = getStopRadius(zoom || map.getZoom(), isMain);
    return {
      radius: radius,
      color: borderColor,
      weight: isMain ? 2.5 : 1.5,
      opacity: 1,
      fillColor: '#ffffff',
      fillOpacity: 1
    };
  }

  // Resize stop markers on zoom change
  function updateStopRadii() {
    var zoom = map.getZoom();
    var names = Object.keys(activeLayers);
    for (var i = 0; i < names.length; i++) {
      if (!isStopsLayer(names[i])) continue;
      activeLayers[names[i]].eachLayer(function (layer) {
        if (layer.setRadius) {
          var props = (layer.feature && layer.feature.properties) || {};
          var isMain = props.stop_type === 'parent_station';
          layer.setRadius(getStopRadius(zoom, isMain));
        }
      });
    }
    // Also resize secondary stop markers
    var secNames = Object.keys(secondaryStopsLayers);
    for (var i = 0; i < secNames.length; i++) {
      secondaryStopsLayers[secNames[i]].eachLayer(function (layer) {
        if (layer.setRadius) {
          layer.setRadius(getStopRadius(zoom, false));
        }
      });
    }
  }

  map.on('zoomend', function () {
    updateStopRadii();
    updateConnectorVisibility();
  });

  function updateConnectorVisibility() {
    var zoom = map.getZoom();
    var names = Object.keys(connectorLineLayers);
    for (var i = 0; i < names.length; i++) {
      var lg = connectorLineLayers[names[i]];
      if (zoom >= 15) {
        if (!map.hasLayer(lg)) lg.addTo(map);
      } else {
        if (map.hasLayer(lg)) map.removeLayer(lg);
      }
    }
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
          var stopStyle = getStopStyle(feature, map.getZoom());
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
        if (layerType === 'stops') {
          return getStopStyle(feature, map.getZoom());
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

  // Close popup on map click
  map.on('click', function () {
    map.closePopup();
  });

  // Layer Fetching and Selection
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

      // Add secondary stops toggle for stops layers
      if (isStopsLayer(layerNames[i])) {
        var subLabel = document.createElement('label');
        subLabel.className = 'layer-control__label layer-control__label--sub';

        var subCheckbox = document.createElement('input');
        subCheckbox.type = 'checkbox';
        subCheckbox.className = 'layer-control__checkbox';
        subCheckbox.dataset.secondaryFor = layerNames[i];
        subCheckbox.checked = !!showSecondary[layerNames[i]];
        subCheckbox.setAttribute('aria-label', 'Show secondary stops for ' + layerNames[i]);
        subCheckbox.addEventListener('change', onSecondaryToggle);

        var subSpan = document.createElement('span');
        subSpan.className = 'layer-control__name layer-control__name--sub';
        subSpan.textContent = 'Secondary stops';

        subLabel.appendChild(subCheckbox);
        subLabel.appendChild(subSpan);
        li.appendChild(subLabel);
      }

      // Add per-route filter list for routes layers
      if (isRoutesLayer(layerNames[i])) {
        var routeContainer = document.createElement('div');
        routeContainer.className = 'route-filter-list';
        routeContainer.id = 'route-filter-' + layerNames[i];
        li.appendChild(routeContainer);
        if (routesGeoJsonCache[layerNames[i]]) {
          buildRouteFilterItems(layerNames[i], routeContainer);
        }
      }

      layerListEl.appendChild(li);
    }
  }

  function buildRouteFilterItems(layerName, container) {
    var geojsonData = routesGeoJsonCache[layerName];
    if (!geojsonData) return;
    var routes = extractUniqueRoutes(geojsonData);
    container.innerHTML = '';
    for (var i = 0; i < routes.length; i++) {
      var route = routes[i];
      var subLabel = document.createElement('label');
      subLabel.className = 'layer-control__label layer-control__label--route';

      var subCheckbox = document.createElement('input');
      subCheckbox.type = 'checkbox';
      subCheckbox.className = 'layer-control__checkbox';
      subCheckbox.dataset.routeLayer = layerName;
      subCheckbox.dataset.routeId = route.id;
      subCheckbox.checked = !routeVisibility[layerName] || routeVisibility[layerName][route.id] !== false;
      subCheckbox.setAttribute('aria-label', 'Toggle route ' + route.displayName);
      subCheckbox.addEventListener('change', onRouteToggle);

      var swatch = document.createElement('span');
      swatch.className = 'route-color-swatch';
      swatch.style.backgroundColor = route.color;

      var nameSpan = document.createElement('span');
      nameSpan.className = 'layer-control__name layer-control__name--route';
      nameSpan.textContent = route.displayName;

      subLabel.appendChild(subCheckbox);
      subLabel.appendChild(swatch);
      subLabel.appendChild(nameSpan);
      container.appendChild(subLabel);
    }
  }

  function onRouteToggle(e) {
    var layerName = e.target.dataset.routeLayer;
    var routeId = e.target.dataset.routeId;
    if (!routeVisibility[layerName]) routeVisibility[layerName] = {};
    routeVisibility[layerName][routeId] = e.target.checked;
    if (e.target.checked) {
      if (routeSubLayers[layerName] && routeSubLayers[layerName][routeId]) {
        routeSubLayers[layerName][routeId].addTo(map);
      }
    } else {
      if (routeSubLayers[layerName] && routeSubLayers[layerName][routeId]) {
        map.removeLayer(routeSubLayers[layerName][routeId]);
      }
    }
  }

  function onSecondaryToggle(e) {
    var name = e.target.dataset.secondaryFor;
    if (e.target.checked) {
      showSecondary[name] = true;
      if (stopsGeoJsonCache[name]) {
        buildSecondaryStopsLayer(name);
      }
    } else {
      showSecondary[name] = false;
      removeSecondaryStopsLayer(name);
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

        var layer;
        if (layerType === 'stops') {
          // Cache full data, show only parent stations initially
          stopsGeoJsonCache[name] = geojsonData;
          var mainOnly = {
            type: 'FeatureCollection',
            features: geojsonData.features.filter(function (f) {
              return f.properties && f.properties.stop_type === 'parent_station';
            })
          };
          layer = createGeoJsonLayer(mainOnly, {
            layerType: 'stops',
            style: DEFAULT_LAYER_STYLE
          });
          layer.addTo(map);
          activeLayers[name] = layer;

          // If secondary was already toggled on, rebuild it
          if (showSecondary[name]) {
            buildSecondaryStopsLayer(name);
          }
        } else if (layerType === 'routes') {
          // Split into per-route sub-layers for individual visibility control
          routesGeoJsonCache[name] = geojsonData;
          if (!routeVisibility[name]) routeVisibility[name] = {};
          routeSubLayers[name] = {};

          var routeFeatures = {};
          for (var k = 0; k < geojsonData.features.length; k++) {
            var feat = geojsonData.features[k];
            var rId = getRouteId(feat);
            if (!routeFeatures[rId]) routeFeatures[rId] = [];
            routeFeatures[rId].push(feat);
          }

          var allBounds = null;
          var rIds = Object.keys(routeFeatures);
          for (var j = 0; j < rIds.length; j++) {
            var id = rIds[j];
            var fc = { type: 'FeatureCollection', features: routeFeatures[id] };
            var sub = createGeoJsonLayer(fc, { layerType: 'routes', style: DEFAULT_LAYER_STYLE });
            if (routeVisibility[name][id] !== false) {
              sub.addTo(map);
              if (routeVisibility[name][id] === undefined) routeVisibility[name][id] = true;
            }
            routeSubLayers[name][id] = sub;
            var b = sub.getBounds();
            if (b.isValid()) allBounds = allBounds ? allBounds.extend(b) : b;
          }

          // Placeholder so the main checkbox considers the layer active
          activeLayers[name] = L.layerGroup().addTo(map);

          // Populate route filter checkboxes
          var container = document.getElementById('route-filter-' + name);
          if (container) buildRouteFilterItems(name, container);

          if (allBounds) map.fitBounds(allBounds, { padding: [30, 30] });
        } else {
          layer = createGeoJsonLayer(geojsonData, {
            layerType: layerType,
            style: DEFAULT_LAYER_STYLE
          });
          layer.addTo(map);
          activeLayers[name] = layer;
        }

        // Fit bounds (routes handled above)
        if (layerType !== 'routes' && activeLayers[name] && activeLayers[name].getBounds) {
          var bounds = activeLayers[name].getBounds();
          if (bounds.isValid()) {
            map.fitBounds(bounds, { padding: [30, 30] });
          }
        }
      })
      .catch(function (err) {
        showNotification('Error loading layer: ' + err.message, 'error');
        var cb = layerListEl.querySelector('input[data-layer-name="' + name + '"]');
        if (cb) cb.checked = false;
      });
  }

  function buildSecondaryStopsLayer(name) {
    var geojsonData = stopsGeoJsonCache[name];
    if (!geojsonData) return;

    // Remove existing secondary layer
    if (secondaryStopsLayers[name]) {
      map.removeLayer(secondaryStopsLayers[name]);
      delete secondaryStopsLayers[name];
    }

    // Index parent stations by id
    var parentIndex = {};
    for (var i = 0; i < geojsonData.features.length; i++) {
      var f = geojsonData.features[i];
      if (f.properties && f.properties.stop_type === 'parent_station') {
        parentIndex[f.id] = f.geometry.coordinates; // [lng, lat]
      }
    }

    var group = L.layerGroup();
    var lines = L.layerGroup();
    var zoom = map.getZoom();

    for (var i = 0; i < geojsonData.features.length; i++) {
      var f = geojsonData.features[i];
      if (!f.properties || f.properties.stop_type !== 'child_stop') continue;

      var childCoords = f.geometry.coordinates; // [lng, lat]
      var parentId = f.properties.parent_station;
      var parentCoords = parentIndex[parentId];
      var childLatLng = L.latLng(childCoords[1], childCoords[0]);

      // Skip child entirely if within 20m of parent — avoids overlapping markers
      if (parentCoords) {
        var parentLatLng = L.latLng(parentCoords[1], parentCoords[0]);
        if (parentLatLng.distanceTo(childLatLng) < 20) continue;
      }

      // Draw the child stop marker
      var style = getStopStyle(f, zoom);
      var marker = L.circleMarker(childLatLng, style);
      marker.feature = f;
      marker.on('click', function (e) {
        L.DomEvent.stopPropagation(e);
        L.popup()
          .setLatLng(e.latlng)
          .setContent(buildPopupContent(this.feature))
          .openOn(map);
      });
      group.addLayer(marker);

      // Draw dashed connector line to parent
      if (parentCoords) {
        var line = L.polyline([childLatLng, parentLatLng], {
          color: '#000',
          weight: 2,
          opacity: 0.9,
          dashArray: '4 4'
        });
        lines.addLayer(line);
      }
    }

    group.addTo(map);
    secondaryStopsLayers[name] = group;

    // Lines shown only at zoom >= 15
    connectorLineLayers[name] = lines;
    if (map.getZoom() >= 15) {
      lines.addTo(map);
    }
  }

  function removeSecondaryStopsLayer(name) {
    if (secondaryStopsLayers[name]) {
      map.removeLayer(secondaryStopsLayers[name]);
      delete secondaryStopsLayers[name];
    }
    if (connectorLineLayers[name]) {
      map.removeLayer(connectorLineLayers[name]);
      delete connectorLineLayers[name];
    }
  }

  function removeLayer(name) {
    if (activeLayers[name]) {
      map.removeLayer(activeLayers[name]);
      delete activeLayers[name];
    }
    removeSecondaryStopsLayer(name);
    delete stopsGeoJsonCache[name];

    // Clean up per-route sub-layers
    if (routeSubLayers[name]) {
      var rIds = Object.keys(routeSubLayers[name]);
      for (var i = 0; i < rIds.length; i++) {
        if (map.hasLayer(routeSubLayers[name][rIds[i]])) {
          map.removeLayer(routeSubLayers[name][rIds[i]]);
        }
      }
      delete routeSubLayers[name];
    }
    delete routesGeoJsonCache[name];
    delete routeVisibility[name];
  }

  // Leaflet.draw Rectangle Control
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

  // Layer List Refresh
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

  // GTFS Upload Logic
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

  // Initialize on Load
  fetchLayers();

})();
