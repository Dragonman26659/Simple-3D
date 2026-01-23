#pragma once
#include "SimpleCore.h"


/*
Needs to have an orthographic and perspective mode so that i can do both 2D and 3D rendering
*/



namespace Simple3D {
    class Camera {
    public:
        Camera(
            float fovDegrees = 45.0f,
            float nearPlane = 0.1f,
            float farPlane = 100.0f
        );

        // --- Position ---
        void setPosition(const glm::vec3& pos);
        const glm::vec3& getPosition() const;

        // --- Rotation ---
        void setRotationEuler(float pitchDeg, float yawDeg, float rollDeg = 0.0f);
        void setRotationQuat(const glm::quat& q);

        glm::vec3 getRotationEuler() const;
        const glm::quat& getRotationQuat() const;

        // --- Projection ---
        void toggleProjectionMode();
        bool isPerspectiveMode() const { return perspectiveMode; }

        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix(float viewportWidth, float viewportHeight) const;

        // --- Orientation vectors ---
        glm::vec3 getForward() const;
        glm::vec3 getRight() const;
        glm::vec3 getUp() const;

        // --- LookAt ---
        void lookAt(const glm::vec3& target);


        bool perspectiveMode = true;

        float fov;
        float nearPlane, farPlane;
        float orthoWidth = 5.0f;
        float orthoHeight = 5.0f;

    private:
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1, 0, 0, 0 }; // identity
    };
}