#pragma once

using namespace DirectX;

class ClassicNoise
{

private:

	std::vector<std::vector<int>>	grad3;
	std::vector<int>				perm;
	std::vector<int>				p;


public:
	ClassicNoise();
	~ClassicNoise();
	double noise(double, double, double);

private:
	static int fastfloor(double);
	double dot(std::vector<int>, double, double, double);
	double mix(double, double, double);
	double fade(double);
};


