#include <algorithm>// std::clamp, std::min
#include <chrono>	// time measurement related
#include <cmath>	// log2, sqrt
#include <cstdio>	// fgetc
#include <cstdint>	// uint8_t
#include <iostream>	// std::cout, std::ostream
#include <thread>	// std::thread and others
#include <utility>	// std::pair<U,V>
#include <vector>	// std::vector<T>
#include "lodepng.cpp"

// ----------------------

// Do not touch: Complex Number structure and Color3f structure

struct Complex{
	explicit Complex(float real = 0.0f, float im = 0.0f) : real(real), im(im){};
	explicit Complex(std::pair<float,float> point) : real(point.first), im(point.second){};
	Complex operator+(const Complex& other) const{ return Complex(this->real + other.real, this->im + other.im); }
	Complex& operator+=(const Complex& other){ this->real += other.real; this->im += other.im; return *this; }
	Complex operator*(const Complex& other) const{ return Complex((this->real*other.real) - (this->im*other.im), (this->real*other.im) + (this->im*other.real) ); }
	Complex& operator*=(const Complex& other){
		float r = (this->real*other.real) - (this->im*other.im);
		float i = (this->real*other.im) + (this->im*other.real);
		this->real = r;
		this->im = i;
		return *this;
	}
	float length() const{ return sqrt((this->real*this->real) + (this->im * this->im)); }
	float real, im;
};

std::ostream& operator<<(std::ostream& out, const Complex& c){ return out << '(' << c.real << ',' << c.im << 'i' << ')'; };

struct Color3f{
	Color3f(float r=0, float g=0, float b=0) : r(r), g(g), b(b){};
	Color3f operator*(float value){ return Color3f(value*this->r,value*this->g,value*this->b); }
	Color3f operator+(const Color3f& other){ return Color3f(other.r + this->r, other.g + this->g, other.b + this->b); }
	float r,g,b;
};

// ----------------------

// Configuration options

// Image size: 8K image
std::size_t imageWidth = 7680;
std::size_t imageHeight = 4320;
// Number of tests to perform per "pixel"
constexpr std::size_t iter_pow = 10;
constexpr std::size_t iterations = (1<<iter_pow)-1;
// Borders of the "camera"
constexpr float left = -2.0f;
constexpr float right = 0.75f;
constexpr float down = -1.0f;
constexpr float up = 1.0f;

// Color palette: For nice images
// Values should range from [0, iter_pow]
std::vector<std::pair<std::size_t, Color3f>> colorPalette = {
	{0, {0.0f,0.08f,0.25f}}, // If the point is obviously outside the set: deep blue
	{iter_pow, {1.0f,1.0f,1.0f}}  // If the point was reaaaaaally close to being in the set: white
};
Color3f inSet(0.0f,0.0f,0.0f); // If the point is in the set: black

// ----------------------

// Transforms an index of the array (which is a flattened matrix of imageWidth * imageWeight) to its position <x,y> within the plane
// Notice: the cells of the matrix are evenly distributed in the x axis from [left, right] and in the y axis from [down, up]
// Note: Technically, since we will be filling the array from top-to-bottom the lower "y" within the matrix should be the uppest value, but you can choose to ignore this.
std::pair<float, float> getPosition(std::size_t index, std::size_t width, std::size_t height, float left, float right, float up, float down)
{
	size_t row = index / width;
	size_t column = index % width;
	float x = left + (((right - left) / width) * column);
	float y = up - (((up - down) / height) * row);

	return std::pair<float, float>(x, y);
}

// Calculates whether the complex point c is in the Mandelbrot set or not
// To calculate this we require two complex numbers: the complex number c being evaluated and a complex number z
// The complex number z starts at 0,0
// Each iteration updates the value of z in the following way: z <- z**2 + c
// A point c is in the Mandelbrot set if after "iterations"-iterations the length of z is <= 2
// If the value is not in the set, the second value is the number of iteration in which we knew it was outside the set
std::pair<bool, std::size_t> evaluatePoint(const Complex& c, std::size_t iterations)
{
	Complex z(0, 0);

	for (int i = 0; i < iterations; i++)
	{
		if (z.length() > 2)
		{
			return std::pair<bool, std::size_t>(false, i + 1);
		}

		z = z * z + c;
	}

	return std::pair<bool, std::size_t>(true, 0);
}

void plotMandelbrotSection(uint8_t* image, size_t startIndex, size_t upperBound, std::size_t width, std::size_t height, float left, float right, float up, float down, std::size_t iterations)
{
	for (std::size_t i = startIndex; i < upperBound; ++i) {
		Complex point(getPosition(i, width, height, left, right, up, down));
		std::pair<bool, std::size_t> evaluation(evaluatePoint(point, iterations));
		// ---- Preferably, do not edit the code inside
		// This section basically sets the color, no need to modify it
		Color3f color;
		if (evaluation.first) {
			color = inSet;
		}
		else {
			float power = log2(static_cast<float>(evaluation.second));
			std::size_t colorIndex = 0;
			while (((colorIndex + 1) < colorPalette.size()) && (power > colorPalette[colorIndex].first)) { ++colorIndex; }
			if (colorIndex == 0) {
				color = colorPalette.front().second;
			}
			else {
				// Calculates interpolation of two colors
				float t = (power - colorPalette[colorIndex - 1].first) / (colorPalette[colorIndex].first - colorPalette[colorIndex - 1].first);
				color = (colorPalette[colorIndex].second * t) + (colorPalette[colorIndex - 1].second * (1.0f - t));
			}
		}
		image[3 * i] = static_cast<uint8_t>(std::clamp(color.r, 0.0f, 1.0f)) * 255.0f;
		image[3 * i + 1] = static_cast<uint8_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
		image[3 * i + 2] = static_cast<uint8_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
		// ----
	}
}

uint8_t* plotMandelbrot(unsigned int threads, std::size_t width, std::size_t height, float left, float right, float up, float down, std::size_t iterations){
	uint8_t* image = new uint8_t[width * height * 3];
	std::jthread* threadarr = new std::jthread[threads];
	int count = std::ceil(width * height / (float)threads);
	size_t totalPixels = width * height;
	for (size_t i = 0; i < threads; i++)
	{
		size_t startIndex = i * count;
		size_t upperBound = std::min(startIndex + count, totalPixels);
		threadarr[i] = std::jthread(plotMandelbrotSection, image, startIndex, upperBound, width, height, left, right, up, down, iterations);
	}
	
	delete [] threadarr;

	return image;
}

/*

Part 4 (Performance results):

Calling single threaded plot
Finished calculating the Mandelbrot set in single thread; time taken: 180281 ms
Calling multi threaded plot with: 10 threads
Finished calculating the Mandelbrot set in multithread; time taken: 46353 ms

I got much better performance using the multiple threads. However, it wasn't as 
much as I was expecting, I would have expected to get almost 10 times the performance 
since it was using 10 threads, but my performance was closer to 4 times faster. The 
main lesson/takeaway for me here is that you can't expect your performance gains to 
correspond directly to the number of threads you use, the overhead and other factors
can make you not gain as much time as you would hope.
*/


// ----------------------

// Do not modify

int main(){
	float time;
	// Calls the plot of the Mandelbrot set single thread
	std::cout << "Calling single threaded plot" << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	uint8_t* image = plotMandelbrot(1, imageWidth, imageHeight, left, right, up, down, iterations);
	auto end = std::chrono::high_resolution_clock::now();
	time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cout << "Finished calculating the Mandelbrot set in single thread; time taken: " << time << " ms" << std::endl;
	unsigned int error = lodepng::encode("Mandelbrot_single.png",image,imageWidth,imageHeight,LodePNGColorType::LCT_RGB,8);
	if(error){ std::cerr << "Error storing file: Mandelbrot_single.png, please check everything is ok" << std::endl; }
	delete[] image;
	
	
	// Calls the plot of the Mandelbrot set multi threaded
	std::cout << "Calling multi threaded plot with: " << std::thread::hardware_concurrency() / 2 << " threads" << std::endl;
	start = std::chrono::high_resolution_clock::now();
	image = plotMandelbrot(std::max(1u, std::thread::hardware_concurrency() / 2), imageWidth, imageHeight, left, right, up, down, iterations);
	end = std::chrono::high_resolution_clock::now();
	time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cout << "Finished calculating the Mandelbrot set in multithread; time taken: " << time << " ms" << std::endl;
	error = lodepng::encode("Mandelbrot_multi.png",image,imageWidth,imageHeight,LodePNGColorType::LCT_RGB,8);
    if(error){ std::cerr << "Error storing file: Mandelbrot_multi.png, please check everything is ok" << std::endl; }
	delete[] image;
    std::cout << "Press ENTER to exit...";
    fgetc(stdin);
}
