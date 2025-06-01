#include "Component/Tools/Camera.h"

namespace Simple3D {
    void Camera::setPosition(const glm::vec3& pos) {
        position = pos;
    }

    void Camera::setRotation(float pitchDegrees, float yawDegrees, float rollDegrees) {
        pitch = pitchDegrees;
        yaw = yawDegrees;
        roll = rollDegrees;
    }

    void Camera::toggleProjectionMode() {
        perspectiveMode = !perspectiveMode;
    }

    glm::mat4 Camera::getViewMatrix() const {
        // Convert Euler angles to radians for GLM
        glm::vec3 rotation(pitch * glm::pi<float>() / 180.0f,
            yaw * glm::pi<float>() / 180.0f,
            roll * glm::pi<float>() / 180.0f);
        
        // Create rotation matrix from Euler angles
        glm::mat4 rotationMatrix =
            glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1, 0, 0)) *
            glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0, 1, 0)) *
            glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0, 0, 1));
        
        // Create translation matrix
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position);
        
        // Combine rotation and translation
        return translation * rotationMatrix;

        //return glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    glm::mat4 Camera::getProjectionMatrix(float viewportWidth, float viewportHeight) const {
        if (perspectiveMode) {
            return glm::perspective(glm::radians(fov),
                viewportWidth / viewportHeight,
                nearPlane, farPlane);
        }
        else {
            return glm::ortho(-orthoWidth / 2, orthoWidth / 2,
                -orthoHeight / 2, orthoHeight / 2,
                nearPlane, farPlane);
        }


        //return glm::perspective(glm::radians(45.0f), viewportWidth / (float)viewportHeight, 0.1f, 10.0f);
    }

    void Camera::lookAt(const glm::vec3& target) {
        // Calculate direction vector from camera to target
        glm::vec3 direction = glm::normalize(target - position);

        // Calculate right vector using cross product
        glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));

        // Calculate up vector using cross product of direction and right
        glm::vec3 up = glm::normalize(glm::cross(right, direction));

        // Convert to Euler angles
        float pitch = asin(-direction.y);
        float yaw = atan2(direction.x, direction.z);
        float roll = 0.0f; // Keep roll at 0 since we're using world up vector

        // Convert to degrees
        setRotation(
            pitch * (180.0f / glm::pi<float>()),
            yaw * (180.0f / glm::pi<float>()),
            roll
        );
    }
}