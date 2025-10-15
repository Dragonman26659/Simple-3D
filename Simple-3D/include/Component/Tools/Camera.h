#pragma once
#include "SimpleCore.h"


/*
Needs to have an orthographic and perspective mode so that i can do both 2D and 3D rendering
*/



namespace Simple3D {
    class Camera {
    public:
        Camera(float fovDegrees = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f)
            : position(0.0f),
            pitch(0.0f), yaw(0.0f), roll(0.0f),
            perspectiveMode(true),
            fov(fovDegrees),
            nearPlane(nearPlane),
            farPlane(farPlane),
            orthoWidth(5.0f),
            orthoHeight(5.0f) {}


        void setPosition(const glm::vec3& pos);
        //void setPosition(const glm::vec<3, float, (glm::qualifier)0>& pos);

        void setRotation(float pitchDegrees, float yawDegrees, float rollDegrees);

        void toggleProjectionMode();


        

        glm::mat4 getViewMatrix() const;
        //glm::mat<4, 4, float, (glm::qualifier)0> getViewMatrix();

        /// <summary>
        /// Generates and returns Projection matrix
        /// </summary>
        /// <param name="viewportWidth">Width of current viewport</param>
        /// <param name="viewportHeight">HEight of current viewport</param>
        /// <returns> Projection Matrix</returns>
        const glm::mat4 getProjectionMatrix(float viewportWidth, float viewportHeight) const;
        //const glm::mat<4, 4, float, (glm::qualifier)0> getProjectionMatrix(float viewportWidth, float viewportHeight);

        // Getters for camera properties
        const glm::vec3 getPosition() const { return position; }
        const glm::vec3 getRotation() const { return glm::vec3(pitch, yaw, roll); }
        bool isPerspectiveMode() const { return perspectiveMode; }

        // Make camera look at coordinate
        void lookAt(const glm::vec3& target);

        glm::vec3 getForward() const;
        glm::vec3 getRight() const;
        glm::vec3 getUp() const;

        glm::vec3 position;
        float pitch, yaw, roll;  // Euler angles in degrees
        bool perspectiveMode;
        float fov;              // Field of View in degrees
        float nearPlane, farPlane;
        float orthoWidth, orthoHeight;
    };
}