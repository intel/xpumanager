#pragma once

enum HealthType {
    HEALTH_ALL=0,
    HEALTH_MEMORY,
    HEALTH_POWER,
    HEALTH_TEMPERATURE,
    HEALTH_FABRIC_PORT,
};

enum HealthStatus {
    HEALTH_UNKNOWN=0,
    HEALTH_OK,
    HEALTH_WARNING,
    HEALTH_CRITICAL
};