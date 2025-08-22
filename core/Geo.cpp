#include "Geo.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace tracker {

double Geo::distanceMeters(double lat1, double lon1, double lat2, double lon2) {
    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);
    
    double a = std::sin(dLat/2) * std::sin(dLat/2) +
               std::cos(toRadians(lat1)) * std::cos(toRadians(lat2)) *
               std::sin(dLon/2) * std::sin(dLon/2);
    
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
    return EARTH_RADIUS_METERS * c;
}

double Geo::bearingDegrees(double lat1, double lon1, double lat2, double lon2) {
    double dLon = toRadians(lon2 - lon1);
    double y = std::sin(dLon) * std::cos(toRadians(lat2));
    double x = std::cos(toRadians(lat1)) * std::sin(toRadians(lat2)) -
               std::sin(toRadians(lat1)) * std::cos(toRadians(lat2)) * std::cos(dLon);
    
    double bearing = toDegrees(std::atan2(y, x));
    return std::fmod(bearing + 360.0, 360.0);
}

Location Geo::moveLocation(const Location& from, double bearingDeg, double distanceMeters) {
    double bearing = toRadians(bearingDeg);
    double d = distanceMeters / EARTH_RADIUS_METERS;
    
    double lat1 = toRadians(from.lat);
    double lon1 = toRadians(from.lon);
    
    double lat2 = std::asin(std::sin(lat1) * std::cos(d) +
                           std::cos(lat1) * std::sin(d) * std::cos(bearing));
    
    double lon2 = lon1 + std::atan2(std::sin(bearing) * std::sin(d) * std::cos(lat1),
                                   std::cos(d) - std::sin(lat1) * std::sin(lat2));
    
    Location result = from;
    result.lat = toDegrees(lat2);
    result.lon = toDegrees(lon2);
    return result;
}

bool Geo::isInsideGeofence(const Location& location, const Geofence& fence) {
    double distance = distanceMeters(location.lat, location.lon, fence.lat, fence.lon);
    return distance <= fence.radiusMeters;
}

std::vector<std::string> Geo::checkGeofences(const Location& location, 
                                           const std::vector<Geofence>& fences) {
    std::vector<std::string> inside;
    for (const auto& fence : fences) {
        if (isInsideGeofence(location, fence)) {
            inside.push_back(fence.id);
        }
    }
    return inside;
}

Location Geo::interpolateRoute(const std::vector<RoutePoint>& route, double progress) {
    if (route.empty()) {
        return Location{};
    }
    
    if (route.size() == 1) {
        Location loc;
        loc.lat = route[0].lat;
        loc.lon = route[0].lon;
        return loc;
    }
    
    progress = std::clamp(progress, 0.0, 1.0);
    double segmentProgress = progress * (route.size() - 1);
    size_t segmentIndex = static_cast<size_t>(segmentProgress);
    double localProgress = segmentProgress - segmentIndex;
    
    if (segmentIndex >= route.size() - 1) {
        Location loc;
        loc.lat = route.back().lat;
        loc.lon = route.back().lon;
        return loc;
    }
    
    const auto& p1 = route[segmentIndex];
    const auto& p2 = route[segmentIndex + 1];
    
    Location loc;
    loc.lat = p1.lat + (p2.lat - p1.lat) * localProgress;
    loc.lon = p1.lon + (p2.lon - p1.lon) * localProgress;
    return loc;
}

double Geo::toRadians(double degrees) {
    return degrees * M_PI / 180.0;
}

double Geo::toDegrees(double radians) {
    return radians * 180.0 / M_PI;
}

} // namespace tracker