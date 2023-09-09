// ConsoleGraphicsTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#define CG_DEBUG
#include "ConsoleGraphics.hpp"

using cg::InterpolationMethod;

int main()
{
	cg::ConsoleGraphics window(1920 / 2, 1080 / 2, true, 2, true);
	window.setTitle("ConsoleGraphics Example");
	window.enableAlpha();
	uint32 colourArr[] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF};
	cg::Image image("example.bmp");
	//image.resize(256, 256, InterpolationMethod::Bilinear);

	//image.scale(0.5f);

	float theta = 0.f;
	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		//Whole window isn't been drawn to so it must be cleared.
		window.clear();

		image.setPos(25 + (25 * sinf(theta)), 0);
		window.draw(image);

		window.display();
		theta += 0.001f;
		if (theta > 360.f) {
			theta -= 360.f;
		}
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
