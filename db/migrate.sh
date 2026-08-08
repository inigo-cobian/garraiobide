#!/usr/bin/env bash
# migrate.sh — Apply all SQL migrations in order against PostGIS.
#
# Usage: ./db/migrate.sh [PGHOST] [PGPORT] [PGUSER] [PGDATABASE]
#
# Environment variables PGPASSWORD, PGHOST, PGPORT, PGUSER, PGDATABASE
# can be used instead of positional args.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MIGRATIONS_DIR="${SCRIPT_DIR}/migrations"

PGHOST="${1:-${PGHOST:-localhost}}"
PGPORT="${2:-${PGPORT:-5432}}"
PGUSER="${3:-${PGUSER:-postgres}}"
PGDATABASE="${4:-${PGDATABASE:-garraiobide}}"

export PGHOST PGPORT PGUSER PGDATABASE

echo "=== Running migrations against ${PGUSER}@${PGHOST}:${PGPORT}/${PGDATABASE} ==="

# Create migrations tracking table if it doesn't exist
psql -v ON_ERROR_STOP=1 <<'EOF'
CREATE TABLE IF NOT EXISTS _migrations (
    filename    TEXT PRIMARY KEY,
    applied_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
EOF

# Apply each migration in sorted order, skipping already-applied ones
for migration in $(find "${MIGRATIONS_DIR}" -name '*.sql' | sort); do
    filename="$(basename "${migration}")"

    already_applied=$(psql -tAc "SELECT COUNT(*) FROM _migrations WHERE filename = '${filename}';")
    if [ "${already_applied}" -eq 1 ]; then
        echo "  [skip] ${filename} (already applied)"
        continue
    fi

    echo "  [apply] ${filename}"
    psql -v ON_ERROR_STOP=1 -f "${migration}"

    psql -v ON_ERROR_STOP=1 -c "INSERT INTO _migrations (filename) VALUES ('${filename}');"
done

echo "=== Migrations complete ==="
