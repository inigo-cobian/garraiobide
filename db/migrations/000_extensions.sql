-- Migration 000: Ensure required PostgreSQL extensions
-- This runs first to guarantee PostGIS is available for subsequent migrations.

CREATE EXTENSION IF NOT EXISTS postgis;
