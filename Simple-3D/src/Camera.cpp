#include "Component/Tools/Camera.h"


namespace Simple3D {
    Camera::Camera(float fovDegrees, float nearPlane, float farPlane) {
        fov = fovDegrees;
        this->nearPlane = nearPlane;
        this->farPlane = farPlane;
    }


    // ------------------------------------------------------------
    // Position
    // ------------------------------------------------------------

    void Camera::setPosition(const glm::vec3& pos)
    {
        position = pos;
    }

    const glm::vec3& Camera::getPosition() const
    {
        return position;
    }

    // ------------------------------------------------------------
   // Rotation
   // ------------------------------------------------------------

    void Camera::setRotationEuler(float pitchDeg, float yawDeg, float rollDeg)
    {
        glm::vec3 radians = glm::radians(glm::vec3(pitchDeg, yawDeg, rollDeg));
        rotation = glm::quat(radians);
    }

    void Camera::setRotationQuat(const glm::quat& q)
    {
        rotation = glm::normalize(q);
    }

    glm::vec3 Camera::getRotationEuler() const
    {
        return glm::degrees(glm::eulerAngles(rotation));
    }

    const glm::quat& Camera::getRotationQuat() const
    {
        return rotation;
    }

    // ------------------------------------------------------------
   // View / Projection
   // ------------------------------------------------------------

    glm::mat4 Camera::getViewMatrix() const
    {
        // View = inverse(Translation * Rotation)
        glm::mat4 world =
            glm::translate(glm::mat4(1.0f), position) *
            glm::mat4(rotation);

        return glm::inverse(world);
    }

    glm::mat4 Camera::getProjectionMatrix(float viewportWidth, float viewportHeight) const
    {
        float aspect = viewportWidth / viewportHeight;

        if (perspectiveMode)
        {
            return glm::perspective(
                glm::radians(fov),
                aspect,
                nearPlane,
                farPlane
            );
        }
        else
        {
            return glm::ortho(
                -orthoWidth * 0.5f,
                orthoWidth * 0.5f,
                -orthoHeight * 0.5f,
                orthoHeight * 0.5f,
                nearPlane,
                farPlane
            );
        }
    }

    // ------------------------------------------------------------
   // Orientation vectors
   // ------------------------------------------------------------

    glm::vec3 Camera::getForward() const
    {
        return glm::normalize(rotation * glm::vec3(0, 0, -1));
    }

    glm::vec3 Camera::getRight() const
    {
        return glm::normalize(rotation * glm::vec3(1, 0, 0));
    }

    glm::vec3 Camera::getUp() const
    {
        return glm::normalize(rotation * glm::vec3(0, 1, 0));
    }

    // ------------------------------------------------------------
    // LookAt
    // ------------------------------------------------------------

    void Camera::lookAt(const glm::vec3& target)
    {
        glm::vec3 forward = glm::normalize(target - position);
        rotation = glm::quatLookAtRH(forward, glm::vec3(0, 1, 0));
    }

    // ------------------------------------------------------------
    // Projection toggle
    // ------------------------------------------------------------

    void Camera::toggleProjectionMode()
    {
        perspectiveMode = !perspectiveMode;
    }
}