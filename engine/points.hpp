#pragma once

struct cartesian_point;

struct geo_point {
    double lat, lon, alt;

    explicit operator cartesian_point() const;
};

struct cartesian_point {
    double x, y, z;
    cartesian_point operator+(const cartesian_point &p) const {
        return {x+p.x, y+p.y, z+p.z};
    };
    cartesian_point operator-(const cartesian_point &p) const {
        return {x-p.x, y-p.y, z-p.z};
    }

    explicit operator geo_point() const;
};