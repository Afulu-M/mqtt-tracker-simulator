#pragma once

#include "Event.hpp"
#include <vector>
#include <string>

namespace tracker {

struct Geofence {
    std::string id;
    double lat = 0.0;
    double lon = 0.0;
    double radiusMeters = 100.0;
};

struct RoutePoint {
    double lat = 0.0;
    double lon = 0.0;
};

class Geo {
public:
    static double distanceMeters(double lat1, double lon1, double lat2, double lon2);
    static double bearingDegrees(double lat1, double lon1, double lat2, double lon2);
    
    static Location moveLocation(const Location& from, double bearingDeg, double distanceMeters);
    
    static bool isInsideGeofence(const Location& location, const Geofence& fence);
    
    static std::vector<std::string> checkGeofences(const Location& location, 
                                                   const std::vector<Geofence>& fences);
    
    static Location interpolateRoute(const std::vector<RoutePoint>& route, 
                                   double progress);
    
private:
    static constexpr double EARTH_RADIUS_METERS = 6371000.0;
    static double toRadians(double degrees);
    static double toDegrees(double radians);
};

} // namespace tracker