#include "Simple3D.h"
#include "GLFW/glfw3.h"
#include <stdio.h>


// Test for now (will replace with model loading)
const std::vector<Vertex> TriangleVertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

	{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> TriangleIndices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};


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