-- Migration 001: Create transit schema with typed entity tables
-- Requires: PostGIS extension enabled on the database

BEGIN;

-- Enable PostGIS extension if not already enabled
CREATE EXTENSION IF NOT EXISTS postgis;

-- ============================================================
-- agencies: Transit agencies operating services
-- ============================================================
CREATE TABLE IF NOT EXISTS agencies (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    url         TEXT,
    timezone    TEXT,
    lang        TEXT,
    phone       TEXT,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ============================================================
-- routes: Transit routes belonging to an agency
-- ============================================================
CREATE TABLE IF NOT EXISTS routes (
    id              TEXT PRIMARY KEY,
    agency_id       TEXT NOT NULL REFERENCES agencies(id) ON DELETE CASCADE,
    short_name      TEXT,
    long_name       TEXT,
    route_type      INTEGER NOT NULL DEFAULT 0,
    color           TEXT,
    text_color      TEXT,
    geometry        geometry(Geometry, 4326),
    station_sequence JSONB,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ============================================================
-- stops: Transit stops/stations with spatial coordinates
-- ============================================================
CREATE TABLE IF NOT EXISTS stops (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    code            TEXT,
    url             TEXT,
    geometry        geometry(Point, 4326) NOT NULL,
    stop_type       TEXT NOT NULL DEFAULT 'standalone'
                    CHECK (stop_type IN ('parent_station', 'child_stop', 'standalone')),
    parent_stop_id  TEXT REFERENCES stops(id) ON DELETE SET NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ============================================================
-- entrances: Physical entrances to a stop/station
-- ============================================================
CREATE TABLE IF NOT EXISTS entrances (
    id          TEXT PRIMARY KEY,
    stop_id     TEXT NOT NULL REFERENCES stops(id) ON DELETE CASCADE,
    name        TEXT,
    geometry    geometry(Point, 4326) NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ============================================================
-- route_stops: Many-to-many relationship between routes and stops
-- Includes stop_sequence for ordered station lists per route
-- ============================================================
CREATE TABLE IF NOT EXISTS route_stops (
    route_id        TEXT NOT NULL REFERENCES routes(id) ON DELETE CASCADE,
    stop_id         TEXT NOT NULL REFERENCES stops(id) ON DELETE CASCADE,
    stop_sequence   INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (route_id, stop_id)
);

-- ============================================================
-- sync_metadata: Tracks batch sync state between PostGIS and MongoDB
-- ============================================================
CREATE TABLE IF NOT EXISTS sync_metadata (
    id              SERIAL PRIMARY KEY,
    sync_type       TEXT NOT NULL DEFAULT 'full',
    started_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at    TIMESTAMPTZ,
    status          TEXT NOT NULL DEFAULT 'running'
                    CHECK (status IN ('running', 'completed', 'failed')),
    records_synced  INTEGER DEFAULT 0,
    error_message   TEXT
);

-- ============================================================
-- Spatial Indexes (GIST)
-- ============================================================
CREATE INDEX IF NOT EXISTS idx_routes_geometry
    ON routes USING GIST (geometry);

CREATE INDEX IF NOT EXISTS idx_stops_geometry
    ON stops USING GIST (geometry);

CREATE INDEX IF NOT EXISTS idx_entrances_geometry
    ON entrances USING GIST (geometry);

-- ============================================================
-- Additional indexes for common query patterns
-- ============================================================
CREATE INDEX IF NOT EXISTS idx_routes_agency_id
    ON routes(agency_id);

CREATE INDEX IF NOT EXISTS idx_stops_parent_stop_id
    ON stops(parent_stop_id);

CREATE INDEX IF NOT EXISTS idx_stops_stop_type
    ON stops(stop_type);

CREATE INDEX IF NOT EXISTS idx_entrances_stop_id
    ON entrances(stop_id);

CREATE INDEX IF NOT EXISTS idx_route_stops_stop_id
    ON route_stops(stop_id);

CREATE INDEX IF NOT EXISTS idx_route_stops_route_id
    ON route_stops(route_id);

CREATE INDEX IF NOT EXISTS idx_sync_metadata_status
    ON sync_metadata(status);

-- ============================================================
-- Trigger: auto-update updated_at on row modification
-- ============================================================
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE TRIGGER trg_agencies_updated_at
    BEFORE UPDATE ON agencies
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE OR REPLACE TRIGGER trg_routes_updated_at
    BEFORE UPDATE ON routes
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE OR REPLACE TRIGGER trg_stops_updated_at
    BEFORE UPDATE ON stops
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE OR REPLACE TRIGGER trg_entrances_updated_at
    BEFORE UPDATE ON entrances
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

COMMIT;
