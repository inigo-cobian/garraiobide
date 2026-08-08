"""GeoDjango models mirroring the PostGIS transit schema.

These models use managed = False since the schema is managed by SQL migrations
in db/migrations/. Django uses them for admin UI and ORM queries only.
"""

from django.contrib.gis.db import models


class Agency(models.Model):
    """Transit agency operating routes."""

    id = models.TextField(primary_key=True)
    name = models.TextField()
    url = models.TextField(blank=True, null=True)
    timezone = models.TextField(blank=True, null=True)
    lang = models.TextField(blank=True, null=True)
    phone = models.TextField(blank=True, null=True)
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        managed = False
        db_table = "agencies"
        verbose_name_plural = "agencies"
        ordering = ["name"]

    def __str__(self):
        return f"{self.name} ({self.id})"


class Route(models.Model):
    """Transit route belonging to an agency."""

    ROUTE_TYPE_CHOICES = [
        (0, "Tram / Light Rail"),
        (1, "Subway / Metro"),
        (2, "Rail"),
        (3, "Bus"),
        (4, "Ferry"),
        (5, "Cable Tram"),
        (6, "Aerial Lift"),
        (7, "Funicular"),
        (11, "Trolleybus"),
        (12, "Monorail"),
    ]

    id = models.TextField(primary_key=True)
    agency = models.ForeignKey(
        Agency, on_delete=models.CASCADE, db_column="agency_id"
    )
    short_name = models.TextField(blank=True, null=True)
    long_name = models.TextField(blank=True, null=True)
    route_type = models.IntegerField(choices=ROUTE_TYPE_CHOICES, default=0)
    color = models.TextField(blank=True, null=True)
    text_color = models.TextField(blank=True, null=True)
    geometry = models.GeometryField(srid=4326, blank=True, null=True)
    station_sequence = models.JSONField(blank=True, null=True)
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        managed = False
        db_table = "routes"
        ordering = ["short_name", "long_name"]

    def __str__(self):
        name = self.short_name or self.long_name or self.id
        return f"{name} ({self.agency_id})"


class Stop(models.Model):
    """Transit stop or station."""

    STOP_TYPE_CHOICES = [
        ("parent_station", "Parent Station"),
        ("child_stop", "Child Stop"),
        ("standalone", "Standalone"),
    ]

    id = models.TextField(primary_key=True)
    name = models.TextField()
    code = models.TextField(blank=True, null=True)
    url = models.TextField(blank=True, null=True)
    geometry = models.PointField(srid=4326)
    stop_type = models.TextField(
        choices=STOP_TYPE_CHOICES, default="standalone"
    )
    parent_stop = models.ForeignKey(
        "self",
        on_delete=models.SET_NULL,
        blank=True,
        null=True,
        db_column="parent_stop_id",
        related_name="children",
    )
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        managed = False
        db_table = "stops"
        ordering = ["name"]

    def __str__(self):
        return f"{self.name} ({self.id})"


class Entrance(models.Model):
    """Physical entrance to a stop/station."""

    id = models.TextField(primary_key=True)
    stop = models.ForeignKey(
        Stop, on_delete=models.CASCADE, db_column="stop_id", related_name="entrances"
    )
    name = models.TextField(blank=True, null=True)
    geometry = models.PointField(srid=4326)
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        managed = False
        db_table = "entrances"
        ordering = ["name"]

    def __str__(self):
        return f"{self.name or self.id} @ {self.stop_id}"


class RouteStop(models.Model):
    """Many-to-many relationship between routes and stops with ordering."""

    route = models.ForeignKey(
        Route, on_delete=models.CASCADE, db_column="route_id"
    )
    stop = models.ForeignKey(
        Stop, on_delete=models.CASCADE, db_column="stop_id"
    )
    stop_sequence = models.IntegerField(default=0)

    class Meta:
        managed = False
        db_table = "route_stops"
        unique_together = [("route", "stop")]
        ordering = ["stop_sequence"]

    def __str__(self):
        return f"{self.route_id} -> {self.stop_id} (seq={self.stop_sequence})"


class SyncMetadata(models.Model):
    """Tracks batch sync state between PostGIS and MongoDB."""

    SYNC_STATUS_CHOICES = [
        ("running", "Running"),
        ("completed", "Completed"),
        ("failed", "Failed"),
    ]

    sync_type = models.TextField(default="full")
    started_at = models.DateTimeField(auto_now_add=True)
    completed_at = models.DateTimeField(blank=True, null=True)
    status = models.TextField(choices=SYNC_STATUS_CHOICES, default="running")
    records_synced = models.IntegerField(default=0)
    error_message = models.TextField(blank=True, null=True)

    class Meta:
        managed = False
        db_table = "sync_metadata"
        verbose_name_plural = "sync metadata"
        ordering = ["-started_at"]

    def __str__(self):
        return f"Sync {self.id}: {self.status} ({self.started_at})"
