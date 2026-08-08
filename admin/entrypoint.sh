#!/bin/bash
set -e

# Read Docker secrets into environment variables
if [ -f /run/secrets/postgres_user ]; then
    export POSTGRES_USER=$(cat /run/secrets/postgres_user)
fi
if [ -f /run/secrets/postgres_password ]; then
    export POSTGRES_PASSWORD=$(cat /run/secrets/postgres_password)
fi

# Run Django migrations for auth tables (our transit models are managed=False)
python manage.py migrate --run-syncdb 2>/dev/null || true

exec "$@"
