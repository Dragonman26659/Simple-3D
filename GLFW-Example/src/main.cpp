#include <GLFW/glfw3.h>
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
	GLFWwindow* window = glfwCreateWindow(640, 480, "My Title", NULL, NULL);

	if (!window) {
		return -1;
	}



	// Create renderer
	Simple3D::Renderer* renderer = new Simple3D::Renderer(window, "No engine", "GLFW Example");



	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();


		// Submit models to be rendered Ect
		renderer->Render();

		// Swap Buffers
		glfwSwapBuffers(window);
	}


	glfwDestroyWindow(window);

	return 0;
}