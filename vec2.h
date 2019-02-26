/*
	Author: Lesley van Hoek

	simple 2d vector class for arduino
*/

#ifndef vec2_h
#define vec2_h

#include "Arduino.h"

class vec2
{
public:
	vec2();
	vec2(float, float);

	float x;
	float y;

	void add_self(const vec2);
	void subtract_self(const vec2);
	void scale_self(const float);
	void divide_self(const float);
	void constrain_self(const float, const float, const float, const float);
	void setmagnitude_self(const float);
	void limit_self(const float);

	static vec2 add(const vec2, const vec2);
	static vec2 subtract(const vec2, const vec2);
	static vec2 scale(const vec2, const float);
	static vec2 divide(const vec2, const float);
	static vec2 normalize(const vec2);
	static vec2 zero();

	static float dot(const vec2, const vec2);
	static float magnitude(const vec2);
	static void print(vec2);
};

#endif