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
        glm::vec3 dir = glm::normalize(target - position);

        float pitchRad = glm::asin(glm::clamp(dir.y, -1.0f, 1.0f));
        float yawRad = glm::atan(dir.x, -dir.z); // matches -Z forward

        setRotation(
            glm::degrees(pitchRad),
            glm::degrees(yawRad),
            0.0f
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
        return glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), getForward()));
    }

    glm::vec3 Camera::getUp() const {
        return glm::normalize(glm::cross(getForward(), getRight()));
    }
}