/*
	Author: Lesley van Hoek

    SHY MIRROR: PROOF OF CONCEPT

	Amber Tijsma (light Design)
	Claudia Agustini (light Design)
	Lesley van Hoek (code and mirror)

*/

/* custom data structs */
#include "userdata.h"
#include "vec2.h"

/* libs */
#include <ServoTimer2.h>

/* calc toolbox */
#define PI 		3.1415926535897932384626433832795
#define TWO_PI 	6.2831853071795864769252867665596
#define USTOCM(x) x/58
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x)))
#define SMOOTHERSTEP(x) ((x) * (x) * (x) * ((x) * ((x) * 6 - 15) + 10))

/* abbreviations */
#define FPPI FIXED_POINT_PRECISION_INT
#define FPPL FIXED_POINT_PRECISION_LONG

/* pins */
#define PIN_DIST A0 		// SHARP IR GP2Y0A21YK0F

#define PIN_SERVO_VERT 7
#define PIN_SERVO_HOR 8

#define PIN_RED 3
#define PIN_BLUE 6
#define PIN_GREEN 5

/* minimum 600 maximum 2400 */
const unsigned long MIN_US_JOINT = 1200;	// left
const unsigned long MAX_US_JOINT = 2500;	// right
const unsigned long MIN_US_BASE = 1500;		// bottom
const unsigned long MAX_US_BASE = 2200;		// top

/* fixed point math */
const float FIXED_POINT_PRECISION_LONG = 1048576.0f;
const int FIXED_POINT_PRECISION_INT = 32767;
const int MAX_RANDOM_TRIES = 2;

/* servos on interrupt timer 2 */
ServoTimer2 servoBase;
ServoTimer2 servoJoint;

/* servo states */
unsigned long servoBaseUS = 0;
unsigned long servoJointUS = 0;
unsigned long servoBaseTargetUS = 0;
unsigned long servoJointTargetUS = 0;

/* ir distance */
const unsigned long TRIGGER_DISTANCE = 30;
const uint8_t MIN_DIST = 10;
const uint8_t MAX_DIST = 80;

const int NUM_DISTREADS = 32;
uint8_t distreads[NUM_DISTREADS];
int distance = 0;
int frameIndex = 0;

/* time variables */
unsigned long lastSight = 0;
unsigned long time;

/* colors */
RGB moodColor;

const RGB COLOR_WHITE = {255, 255, 255};
const RGB COLOR_TARTORANGE = {254, 25, 25};
const RGB COLOR_CARMINEPINK = {255, 89, 100};

/* mirror vectors */
vec2 currentPos(0.5f, 0.5f);
vec2 targetPos(0.5f, 0.5f);
vec2 velocity(0.0f, 0.0f);
vec2 acceleration(0.0f, 0.0f);

/* mirror movement */
float deviation = 0.45;
float brakeproximity = 0.15f;
float maxSpeed = 0.00075f;

float targetDist = 0.0f;
float dist = 0.0f;

unsigned long timeout = 1000;

// INITIALIZE MIRROR
void setup()
{
	lastSight = 0;
	Serial.begin(9600);
	Serial.println("shy mirror v2");

	// initialize servo position values in center positions
	servoBaseUS = mapToServoState(0.5f, BASE);
	servoJointUS = mapToServoState(0.5f, JOINT);

	// timer 2 is used for the servos
	servoBase.attach(PIN_SERVO_HOR);
	servoJoint.attach(PIN_SERVO_VERT);
	servoBase.write(servoBaseUS);
	servoJoint.write(servoJointUS);

	pinMode(LED_BUILTIN, OUTPUT);

	pinMode(PIN_RED, OUTPUT);
	pinMode(PIN_GREEN, OUTPUT);
	pinMode(PIN_BLUE, OUTPUT);

	// init distance reads
	for (int i=0; i<NUM_DISTREADS; i++) distreads[i] = MAX_DIST+1;

	// set default color values
	moodColor = COLOR_WHITE;
	writeMoodColor();
}

// FLASH BUILTIN LED
boolean flasher()
{
	// change pulse frequency based on measured distance
	long pulse = (MAX_DIST+1)*2-distance*2;
	bool f = time%pulse < pulse;
	digitalWrite(LED_BUILTIN, f);
	return f;
}

// COLOR INTERPOLATION
void lerpMoodColor(RGB src, RGB target, float x)
{
	int pct = x * FIXED_POINT_PRECISION_INT;
	moodColor.r = map(pct, 0, FIXED_POINT_PRECISION_INT, src.r, target.r);
	moodColor.g = map(pct, 0, FIXED_POINT_PRECISION_INT, src.g, target.g);
	moodColor.b = map(pct, 0, FIXED_POINT_PRECISION_INT, src.b, target.b);
}

// WRITE COLOR
void writeMoodColor()
{
	analogWrite(PIN_RED, moodColor.r);
	analogWrite(PIN_GREEN, moodColor.g);
	analogWrite(PIN_BLUE, moodColor.b);
}

// RETURN NORMALIZED VALUE MAPPED TO SERVO STATE BOUNDS
long mapToServoState(float val, ServoType type)
{
	val = constrain(val, 0.0f, 1.0f);
	return (type == BASE) ?
		map(val*FPPL, 0, FPPL, MIN_US_BASE, MAX_US_BASE) : 
		map(val*FPPL, 0, FPPL, MIN_US_JOINT, MAX_US_JOINT);
}

// RETURN NEWLY-GENERATED NORMALIZED SELECTIVELY RANDOM LOCATION
vec2 randomLocation()
{
	vec2 pos = vec2(currentPos.x, currentPos.y);
	for (int i = 0; i < MAX_RANDOM_TRIES; i++) // try this up to MAX_RANDOM_TRIES times
	{
		float phi = (random(0, FPPL)/FPPL)*TWO_PI;
		pos.x = cos(phi)*deviation;
		pos.y = sin(phi)*deviation;

		if (pos.x > 0.0f && pos.x < 1.0f && pos.y > 0.0f && pos.y < 1.0f) return pos;
	}
	bool r1 = random(0, 1);
	bool r2 = random(0, 1);
	float x = (currentPos.x + deviation > 1.0f) ? 
		random(0, (currentPos.x - deviation) * FPPL)/FPPL : random((currentPos.x + deviation) * FPPL, FPPL)/FPPL;
	float y = (currentPos.y + deviation > 1.0f) ? 
		random(0, (currentPos.y - deviation) * FPPL)/FPPL : random((currentPos.y + deviation) * FPPL, FPPL)/FPPL;

	pos.x = (r1 > 0) ? x : pos.x;
	pos.y = (r2 > 0) ? y : pos.y;

	return vec2(random(0, FPPL)/FPPL, random(0, FPPL)/FPPL);
}

void updateDistance()
{		
	// read ir distance
	uint8_t rawdist = 4800/(analogRead(PIN_DIST)-20);
	rawdist = (rawdist > MAX_DIST) ? MAX_DIST+1 : rawdist;
	rawdist = (rawdist < MIN_DIST) ? MIN_DIST-1 : rawdist;
	distreads[frameIndex] = rawdist;

	// calculate average distance over 24 reads
	int totaldist = 0;
	for (int i=0; i<NUM_DISTREADS; i++) totaldist += distreads[i];
	distance = totaldist / NUM_DISTREADS;

	frameIndex++;
	frameIndex%=NUM_DISTREADS;
}

// BACKGROUND ROUTINE
void loop() 
{
	time = millis();
	updateDistance();

	// consistent time value per update
	if (time > lastSight + timeout)
	{
		if (distance < TRIGGER_DISTANCE)
		{
			lastSight = time;
			targetPos = randomLocation();
			targetDist = vec2::magnitude(vec2::subtract(targetPos, currentPos));
			Serial.println(distance);
		}
	}
	// calculate interpolated location based on loose physics
	vec2 diff = vec2::subtract(targetPos, currentPos);
	acceleration = vec2(diff.x, diff.y);
	dist = vec2::magnitude(diff);

	velocity.add_self(acceleration);
	velocity.limit_self(maxSpeed);

	vec2 step = vec2(velocity.x, velocity.y);
	step.setmagnitude_self(min(dist, vec2::magnitude(velocity)));

	float clampeddist = constrain(dist, 0, brakeproximity);
	float fac = SMOOTHERSTEP(clampeddist/brakeproximity);

	currentPos.add_self(vec2::scale(step, fac));
	currentPos.constrain_self(0.0f, 1.0f, 0.0f, 1.0f);

	// movement updates mapped to microseconds boundaries
	servoBase.write(mapToServoState(currentPos.x, BASE));
	servoJoint.write(mapToServoState(currentPos.y, JOINT));

	// update backlight
	float blush = 1.0f - distance/(float)MAX_DIST;
	lerpMoodColor(COLOR_CARMINEPINK, COLOR_TARTORANGE, blush);
	writeMoodColor();

	// heartbeat
	flasher(); 
}
