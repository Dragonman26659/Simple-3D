#include "Simple3D.h"
#include <stdio.h>





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


	GLFWwindow* window = glfwCreateWindow(640, 480, "My Title", NULL, NULL);

	if (!window) {
		return -1;
	}



	// Create renderer
	Simple3D::Renderer* renderer = new Simple3D::Renderer(window, "No engine", "GLFW Example");
	Simple3D::Model* myModel = new Simple3D::Model(TriangleVertices, TriangleIndices);


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
	delete renderer;
	glfwDestroyWindow(window); 

	return 0;
}