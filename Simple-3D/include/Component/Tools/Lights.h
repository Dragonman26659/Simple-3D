#pragma once
#include "SimpleCore.h"




namespace Simple3D {
	enum LightType {
		point = 1,
		spot = 2,
		directional = 3
	};

    struct Light {
        // Core properties
        LightType type;
        bool castShadows;

        // Position/direction properties
        // Direction only affects spot/directional lights
        glm::vec3 position;
        glm::vec3 direction;

        // Intensity and color properties
        glm::vec3 Color;
        float intensity;

        // Shadow-related properties
        float shadowBias;
        float shadowIntensity;

        // Spotlight-specific properties
        float cutoffAngle;
        float outerCutoffAngle;
    };
}