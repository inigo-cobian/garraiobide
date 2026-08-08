"""Django management command to sync PostGIS data to MongoDB.

Usage:
    python manage.py sync_mongo --mongo-uri mongodb://user:pass@host:27017
    python manage.py sync_mongo --mongo-uri mongodb://host:27017 --mongo-db garraiobide
"""

import json

from django.core.management.base import BaseCommand, CommandError
from django.contrib.gis.geos import GEOSGeometry
from django.utils import timezone

from transit.models import Agency, Route, Stop, SyncMetadata


class Command(BaseCommand):
    help = "Sync transit data from PostGIS to MongoDB"

    def add_arguments(self, parser):
        parser.add_argument(
            "--mongo-uri",
            required=True,
            help="MongoDB connection URI (e.g., mongodb://localhost:27017)",
        )
        parser.add_argument(
            "--mongo-db",
            default="garraiobide",
            help="MongoDB database name (default: garraiobide)",
        )

    def handle(self, *args, **options):
        mongo_uri = options["mongo_uri"]
        mongo_db = options["mongo_db"]

        try:
            from pymongo import MongoClient
        except ImportError:
            raise CommandError(
                "pymongo is required for MongoDB sync. "
                "Install it with: pip install pymongo"
            )

        # Record sync start
        sync_record = SyncMetadata(sync_type="full", status="running")
        sync_record.save()

        try:
            # Connect to MongoDB
            client = MongoClient(mongo_uri)
            db = client[mongo_db]

            # Build routes layer
            routes = Route.objects.select_related("agency").all()
            routes_features = []
            for route in routes:
                feature = {
                    "id": route.id,
                    "geometry": json.loads(route.geometry.geojson) if route.geometry else None,
                    "properties": {
                        "route_short_name": route.short_name or "",
                        "route_long_name": route.long_name or "",
                        "route_type": route.route_type,
                        "route_color": route.color or "",
                        "route_text_color": route.text_color or "",
                    },
                }
                if route.station_sequence:
                    feature["properties"]["station_sequence"] = json.dumps(
                        route.station_sequence
                    )
                routes_features.append(feature)

            # Build stops layer
            stops = Stop.objects.all()
            stops_features = []
            for stop in stops:
                feature = {
                    "id": stop.id,
                    "geometry": json.loads(stop.geometry.geojson),
                    "properties": {
                        "stop_name": stop.name,
                        "stop_type": stop.stop_type,
                        "stop_code": stop.code or "",
                    },
                }
                if stop.parent_stop_id:
                    feature["properties"]["parent_station"] = stop.parent_stop_id
                stops_features.append(feature)

            # Determine layer prefix
            agency = Agency.objects.first()
            prefix = agency.id if agency else "transit"

            routes_layer_name = f"{prefix}_routes"
            stops_layer_name = f"{prefix}_stops"

            # Drop and reinsert in MongoDB
            layers_collection = db["layers"]

            layers_collection.delete_one({"name": routes_layer_name})
            layers_collection.delete_one({"name": stops_layer_name})

            if routes_features:
                layers_collection.insert_one({
                    "name": routes_layer_name,
                    "scale": "Urban",
                    "features": routes_features,
                })

            if stops_features:
                layers_collection.insert_one({
                    "name": stops_layer_name,
                    "scale": "Urban",
                    "features": stops_features,
                })

            total_records = len(routes_features) + len(stops_features)

            # Record success
            sync_record.status = "completed"
            sync_record.completed_at = timezone.now()
            sync_record.records_synced = total_records
            sync_record.save()

            self.stdout.write(self.style.SUCCESS(
                f"Sync complete: {len(routes_features)} routes, "
                f"{len(stops_features)} stops synced to MongoDB."
            ))

        except Exception as e:
            sync_record.status = "failed"
            sync_record.completed_at = timezone.now()
            sync_record.error_message = str(e)
            sync_record.save()
            raise CommandError(f"Sync failed: {e}")
