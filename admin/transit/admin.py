"""Django Admin configuration for transit entities."""

import os

from django.contrib import admin
from django.contrib.gis.admin import GISModelAdmin
from django.core.management import call_command
from django.contrib import messages

from .models import Agency, Entrance, Route, RouteStop, Stop, SyncMetadata


class RouteInline(admin.TabularInline):
    model = Route
    extra = 0
    fields = ("id", "short_name", "long_name", "route_type", "color")
    readonly_fields = ("id",)
    show_change_link = True


@admin.register(Agency)
class AgencyAdmin(admin.ModelAdmin):
    list_display = ("id", "name", "url", "timezone", "route_count")
    search_fields = ("id", "name")
    list_filter = ("timezone",)
    inlines = [RouteInline]

    @admin.display(description="Routes")
    def route_count(self, obj):
        return obj.route_set.count()


class RouteStopInline(admin.TabularInline):
    model = RouteStop
    extra = 0
    fields = ("stop", "stop_sequence")
    ordering = ("stop_sequence",)
    autocomplete_fields = ("stop",)


@admin.register(Route)
class RouteAdmin(GISModelAdmin):
    list_display = (
        "id", "short_name", "long_name", "agency", "route_type", "color_preview"
    )
    list_filter = ("route_type", "agency")
    search_fields = ("id", "short_name", "long_name")
    readonly_fields = ("created_at", "updated_at")
    fieldsets = (
        (None, {
            "fields": ("id", "agency", "short_name", "long_name", "route_type")
        }),
        ("Appearance", {
            "fields": ("color", "text_color")
        }),
        ("Geometry", {
            "fields": ("geometry",),
            "classes": ("collapse",),
        }),
        ("Station Sequence", {
            "fields": ("station_sequence",),
            "classes": ("collapse",),
        }),
        ("Metadata", {
            "fields": ("created_at", "updated_at"),
            "classes": ("collapse",),
        }),
    )
    inlines = [RouteStopInline]

    @admin.display(description="Color")
    def color_preview(self, obj):
        if obj.color:
            return f'<span style="background:#{obj.color};padding:2px 12px;">&nbsp;</span>'
        return "-"

    color_preview.allow_tags = True


class EntranceInline(admin.TabularInline):
    model = Entrance
    extra = 0
    fields = ("id", "name", "geometry")


class ChildStopInline(admin.TabularInline):
    model = Stop
    fk_name = "parent_stop"
    extra = 0
    fields = ("id", "name", "stop_type", "geometry")
    readonly_fields = ("id",)
    show_change_link = True
    verbose_name = "Child Stop"
    verbose_name_plural = "Child Stops"


@admin.register(Stop)
class StopAdmin(GISModelAdmin):
    list_display = ("id", "name", "code", "stop_type", "parent_stop")
    list_filter = ("stop_type",)
    search_fields = ("id", "name", "code")
    readonly_fields = ("created_at", "updated_at")
    fieldsets = (
        (None, {
            "fields": ("id", "name", "code", "url", "stop_type", "parent_stop")
        }),
        ("Location", {
            "fields": ("geometry",)
        }),
        ("Metadata", {
            "fields": ("created_at", "updated_at"),
            "classes": ("collapse",),
        }),
    )
    inlines = [EntranceInline, ChildStopInline]


@admin.register(Entrance)
class EntranceAdmin(GISModelAdmin):
    list_display = ("id", "name", "stop")
    search_fields = ("id", "name")
    list_filter = ("stop__stop_type",)
    readonly_fields = ("created_at", "updated_at")


@admin.register(SyncMetadata)
class SyncMetadataAdmin(admin.ModelAdmin):
    list_display = ("id", "sync_type", "status", "started_at", "completed_at", "records_synced")
    list_filter = ("status", "sync_type")
    readonly_fields = (
        "sync_type", "started_at", "completed_at", "status",
        "records_synced", "error_message"
    )
    actions = ["trigger_sync"]

    @admin.action(description="Trigger MongoDB sync now")
    def trigger_sync(self, request, queryset):
        mongo_uri = os.environ.get("MONGO_URI", "mongodb://localhost:27017")
        mongo_db = os.environ.get("MONGO_DB", "garraiobide")
        try:
            call_command("sync_mongo", mongo_uri=mongo_uri, mongo_db=mongo_db)
            messages.success(request, "MongoDB sync completed successfully.")
        except Exception as e:
            messages.error(request, f"MongoDB sync failed: {e}")
