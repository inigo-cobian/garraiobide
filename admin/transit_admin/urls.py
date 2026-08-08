"""URL configuration for transit admin."""

from django.contrib import admin
from django.urls import path

admin.site.site_header = "Garraiobide Transit Admin"
admin.site.site_title = "Transit Admin"
admin.site.index_title = "Transit Data Management"

urlpatterns = [
    path("admin/", admin.site.urls),
]
