[![CI](https://github.com/inigo-cobian/garraiobide/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/inigo-cobian/garraiobide/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/inigo-cobian/garraiobide/actions/workflows/integration-tests.yml/badge.svg?branch=main)](https://github.com/inigo-cobian/garraiobide/actions/workflows/integration-tests.yml)
[![codecov](https://codecov.io/github/inigo-cobian/garraiobide/graph/badge.svg?token=Z3KPMGUI0W)](https://codecov.io/github/inigo-cobian/garraiobide)
![License](https://img.shields.io/badge/license-AGPLv3-blue.svg)

# Garraiobide

A Basque word [meaning](https://hiztegiak.elhuyar.eus/eu_en/garraiobide) "means of transport".

Garraiobide is a public transport visualizer (quite WIP) built with C++. The goal of the project is to provide an overview of the current, past, and future state of transportation, as well as projects that have been canceled or modified. 

## Architecture

Garraiobide follows a hexagonal (ports & adapters) architecture with PostGIS as the canonical data store, a Django Admin UI for management, and an optional MongoDB read replica for high-performance map serving.

```
                         +------------------+
                         |   Leaflet Map    |
                         |   (Frontend)     |
                         +--------+---------+
                                  |
                         REST API (GeoJSON)
                                  |
                   +--------------+---------------+
                   |       HTTP Adapter           |
                   |    (cpp-httplib server)      |
                   +--------------+---------------+
                                  |
                   +--------------+---------------+
                   |       LayerService           |
                   |    (Application Core)        |
                   +--------------+---------------+
                                  |
              +-------------------+-------------------+
              |                   |                   |
   +----------+------+  +--------+--------+  +------+----------+
   | PersistencePort |  | DataIngestionPort|  |PresentationPort|
   +----------+------+  +--------+--------+  +------+----------+
              |                   |
     +--------+--------+         |
     |        |        |    GTFS Parser
     v        v        v
  +------+ +------+ +-------+
  | File | |PostGIS| |MongoDB|
  +------+ +------+ +-------+

   PostGIS also serves:
     TransitRepositoryPort (typed CRUD)
       |
     Django Admin (GeoDjango ORM)
```

### Data Flow

1. **Ingest**: GTFS ZIP -> `gtfs_ingest` tool -> PostGIS (typed entities)
2. **Serve**: PostGIS -> `PostgisPersistenceAdapter` -> REST API -> Leaflet map
3. **Sync**: PostGIS -> `sync_mongo` tool -> MongoDB (denormalized layers)
4. **Admin**: Django Admin -> PostGIS (direct ORM access)

### Storage Backends

| Backend | Role | When to use |
|---------|------|-------------|
| PostGIS | Primary relational store | Always (canonical source of truth) |
| MongoDB | Denormalized read replica | High-traffic map serving at scale |
| File    | GeoJSON files | Development/testing without databases |

## Schema

The PostGIS schema models transit data with proper referential integrity:

```
agencies (1) ---> (*) routes
                       |
               route_stops (M:N)
                       |
stops (1) ---> (*) entrances
  |
  +---> (*) stops (parent/child self-reference)
```

**Tables**: `agencies`, `routes`, `stops`, `entrances`, `route_stops`, `sync_metadata`

All geometry columns use SRID 4326 (WGS84) with GIST spatial indexes.

## Quick Start

### Prerequisites

- C++23 compiler (GCC 13+ or Clang 17+)
- Conan 2.x package manager
- Docker & Docker Compose
- Python 3.12+ (for Django Admin)

### Setup

```bash
# Start databases
make docker-up

# Apply schema migrations
make migrate

# Install C++ dependencies and build
make install-deps
make configure
make build

# Run tests
make test
```

### Ingest GTFS Data

```bash
# Ingest to PostGIS (recommended)
make ingest-postgis ZIP=path/to/gtfs.zip

# Or ingest to file (legacy mode)
make ingest-file ZIP=path/to/gtfs.zip
```

### Run the Server

```bash
# With PostGIS backend (recommended)
make serve-postgis

# With file backend (development)
make serve-file

# With MongoDB spatial backend (performance at scale)
make sync           # First sync PostGIS -> MongoDB
make serve-mongo
```

### Django Admin

```bash
# Setup
make admin-setup

# Run (or use docker-compose which runs it automatically)
make admin-run
# Visit http://localhost:8000/admin/
```

### Sync to MongoDB

```bash
# CLI tool
make sync

# Or from Django Admin: Sync Metadata -> Select any -> Actions -> "Trigger MongoDB sync now"
```

## Project Structure

```
garraiobide/
+-- src/
|   +-- core/
|   |   +-- domain/         # Domain entities (Agency, Route, Stop, Entrance, GeoFeature, Layer)
|   |   +-- ports/          # Port interfaces (PersistencePort, TransitRepositoryPort, ...)
|   +-- adapters/
|   |   +-- persistence/    # File, MongoDB, PostGIS adapters
|   |   +-- ingestion/      # GTFS parser and entity parser
|   |   +-- http/           # REST API (GeoJSON endpoints)
|   +-- app/                # Application services (LayerService)
|   +-- server/             # Server entry point
|   +-- tools/
|       +-- gtfs_ingest/    # GTFS ingestion CLI
|       +-- sync_mongo/     # PostGIS -> MongoDB sync CLI
+-- admin/                  # Django Admin (GeoDjango)
+-- db/
|   +-- migrations/         # SQL schema migrations
|   +-- migrate.sh          # Migration runner script
+-- docker/
|   +-- docker-compose.yml  # PostGIS, MongoDB, Admin services
+-- tests/                  # GTest unit and integration tests
```

## Configuration

The server accepts these flags:

| Flag | Description | Default |
|------|-------------|---------|
| `-p, --port` | HTTP port | 8080 |
| `-d, --data` | File backend directory | `data/` |
| `--postgis` | PostGIS connection string | (none) |
| `--spatial-backend` | Spatial query backend: `postgis` or `mongodb` | `postgis` |
| `--mongo` | MongoDB URI | (none) |
| `--mongo-db` | MongoDB database name | `garraiobide` |

## License

[AGPLv3](LICENSE.md)
