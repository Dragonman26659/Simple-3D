#include "Component/Tools/Camera.h"


namespace Simple3D {
    // --- Position ---
    void Camera::setPosition(const glm::vec3& pos) {
        position = pos;
    }

    // --- Rotation ---
    void Camera::setRotation(float pitchDegrees, float yawDegrees, float rollDegrees) {
        pitch = pitchDegrees;
        yaw = yawDegrees;
        roll = rollDegrees;
    }

    // --- Projection Mode ---
    void Camera::toggleProjectionMode() {
        perspectiveMode = !perspectiveMode;
    }

    // --- View Matrix ---
    glm::mat4 Camera::getViewMatrix() const {
        return glm::lookAt(position, position + getForward(), getUp());
    }

    // --- Projection Matrix ---
    const glm::mat4 Camera::getProjectionMatrix(float viewportWidth, float viewportHeight) const {
        if (perspectiveMode) {
            return glm::perspective(
                glm::radians(fov),
                viewportWidth / viewportHeight,
                nearPlane, farPlane
            );
        }
        else {
            return glm::ortho(
                -orthoWidth / 2.0f, orthoWidth / 2.0f,
                -orthoHeight / 2.0f, orthoHeight / 2.0f,
                nearPlane, farPlane
            );
        }
    }

    // --- Look At Target ---
    void Camera::lookAt(const glm::vec3& target) {
        // Direction from camera to target
        glm::vec3 direction = glm::normalize(target - position);

        // Compute Euler angles
        float pitchRad = glm::asin(glm::clamp(direction.y, -1.0f, 1.0f));
        float yawRad = glm::atan(-direction.x, -direction.z);

        // Set rotation in degrees
        setRotation(
            glm::degrees(pitchRad),
            glm::degrees(yawRad),
            0.0f // Keep roll at 0 for upright camera
        );
    }

    glm::vec3 Camera::getForward() const {
        glm::vec3 forward;
        forward.x = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
        forward.y = sin(glm::radians(pitch));
        forward.z = -cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        return glm::normalize(forward);
    }

    glm::vec3 Camera::getRight() const {
        // Always perpendicular to world up
        return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    glm::vec3 Camera::getUp() const {
        // Now recompute true up relative to corrected right vector
        return glm::normalize(glm::cross(getRight(), getForward()));
    }
}