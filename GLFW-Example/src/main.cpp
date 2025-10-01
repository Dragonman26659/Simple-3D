#ifndef SDL_WINDOW

#include "Simple3D.h"
#include "GLFW/glfw3.h"
#include <stdio.h>


Simple3D::Model* loadModel(std::string MODEL_PATH) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error(err);
    }

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // Position (unchanged)
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // Normal (new)
            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            else {
                // Handle missing normals
                vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default upward-facing normal
            }

            // Texture coordinates (unchanged)
            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    return new Simple3D::Model(vertices, indices);
}


void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}


int main() {
	glfwInit();
	glfwSetErrorCallback(error_callback);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);


	GLFWwindow* window = glfwCreateWindow(640, 480, "GLFW example", NULL, NULL);

	if (!window) {
		return -1;
	}



	// Create Objects
	Simple3D::Renderer* renderer = new Simple3D::Renderer(window, "No engine", "GLFW Example");
	Simple3D::Model* myModel = new Simple3D::Model(TriangleVertices, TriangleIndices);
	Simple3D::Camera* mainCam = new Simple3D::Camera();



	// Setup Materials for all models
	Simple3D::MaterialInfo info;
	info.FragmentSource = "shaders/frag.spv";
	info.vertexSource = "shaders/vert.spv";
	info.textures.push_back("textures/albeado_viking_room.png");

	myModel->BindMaterial(renderer->CreateMaterial(info));


	// Set Positions
	mainCam->setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
	mainCam->perspectiveMode = true;

	myModel->SetTransform(glm::mat4(1.0f));

	mainCam->lookAt(glm::vec3(0.0f));

	renderer->SubmitMainCamera(mainCam);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();


		// Submit models to be rendered Ect
		renderer->SumbitModelToFrame(myModel);


		// Render
		renderer->Render();
	}

	renderer->WaitToFinish();

	delete myModel;
	delete mainCam;
	delete renderer;
	glfwDestroyWindow(window); 

	return 0;
}

#endif