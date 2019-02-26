#include "vec2.h"
#include <math.h>

vec2::vec2(float x, float y)
{
	this->x = x;
	this->y = y;
}

void vec2::add_self(const vec2 a)
{
	this->x += a.x;
	this->y += a.y;
}

void vec2::subtract_self(const vec2 a)
{
	this->x -= a.x;
	this->y -= a.y;
}

void vec2::scale_self(const float scale)
{
	this->x *= scale;
	this->y *= scale;
}

void vec2::divide_self(const float div)
{
	this->x /= div;
	this->y /= div;
}

void vec2::constrain_self(const float min_x, const float max_x, const float min_y, const float max_y)
{
	this->x = constrain(this->x, 0.0f, 1.0f);
	this->y = constrain(this->y, 0.0f, 1.0f);
}

void vec2::setmagnitude_self(const float mag)
{
	vec2 normalized = normalize(*this);
	normalized.scale_self(mag);
	this->x = normalized.x;
	this->y = normalized.y;
}

void vec2::limit_self(const float lim)
{
	if (magnitude(*this) > lim) setmagnitude_self(lim);
}

static vec2 vec2::normalize(const vec2 a)
{
	float len = magnitude(a);

	if (len == 0) return vec2::zero();
	return vec2(a.x/len, a.y/len);
}

static vec2 vec2::add(const vec2 a, const vec2 b)
{
	return vec2(a.x+b.x, a.y+b.y);
}

static vec2 vec2::subtract(const vec2 a, const vec2 b)
{
	return vec2(a.x-b.x, a.y-b.y);
}

static vec2 vec2::scale(const vec2 a, float scale)
{
	return vec2(a.x * scale, a.y * scale);
}

static vec2 vec2::divide(const vec2 a, float div)
{
	return vec2(a.x / div, a.y / div);
}

static float vec2::dot(const vec2 a, const vec2 b)
{
    return a.x*b.x + a.y*b.y;
}

static float vec2::magnitude(const vec2 a)
{
	return sqrt(dot(a, a));
}

static vec2 vec2::zero()
{
	return vec2(0.0f, 0.0f);
}

static void vec2::print(const vec2 a)
{
	Serial.print("(");
	Serial.print(a.x);
	Serial.print(", ");
	Serial.print(a.y);
	Serial.print(")\n");
} 