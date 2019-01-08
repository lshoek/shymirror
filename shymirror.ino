/*
	Author: Lesley van Hoek

    SHY MIRROR: PROOF OF CONCEPT

	Amber Tijsma (light Design)
	Claudia Agustini (light Design)
	Lesley van Hoek (code and mirror)

*/

/* custom data structs */
#include "userdata.h"

/* libs */
#include <ServoTimer2.h>
#include <TimerOne.h>

/* calc toolbox */
#define PI 		3.1415926535897932384626433832795
#define TWO_PI 	6.2831853071795864769252867665596
#define USTOCM(x) x/58

/* tick setup */
#define TIMER_US 50
#define TICK_COUNTS 2000	// 2000US/50TICKS = 100 ms

/* pins */
#define PIN_SERVO_VERT 7
#define PIN_SERVO_HOR 8
#define PIN_DIST_TRIG 4
#define PIN_DIST_ECHO 2		// interrupt 0 = pin 2

#define PIN_RED 3
#define PIN_BLUE 6
#define PIN_GREEN 5

/* minimum 600 maximum 2400 */
const unsigned long MIN_US_JOINT = 1200;	// bottom
const unsigned long MAX_US_JOINT = 2400;	// top
const unsigned long MIN_US_BASE = 1200;		// right
const unsigned long MAX_US_BASE = 2400;		// left
const unsigned long DIFF = 500;

/* time constants */
const unsigned long TRIGGER_DISTANCE = 40;
const unsigned long TIME_TO_RECOVER = 500;
const unsigned long TIME_OF_FEAR = 1000;

/* fixed point math */
const float FIXED_POINT_PRECISION_LONG = 1048576;
const int FIXED_POINT_PRECISION_INT = 32767;

/* servos on interrupt timer 2 */
ServoTimer2 servoBase;
ServoTimer2 servoJoint;

/* servo states */
unsigned long servoBaseUS = 0;
unsigned long servoJointUS = 0;
unsigned long servoBaseTargetUS = 0;
unsigned long servoJointTargetUS = 0;

/* time variables */
unsigned long flashPulse = 500;
unsigned long lastMoodShift;
unsigned long time;

/* misc */
bool lockWave = false;

/* interrupt variables */
volatile unsigned long echo_start = 0;
volatile unsigned long echo_end = 0;
volatile unsigned long distance = 4000;	// centimeters
volatile unsigned int triggerTimeCountdown;
bool interruptState;
bool interruptActive;

/* states */
TriggerState triggerState;
MoodState moodState;

/* colors */
RGB moodColor;

const RGB COLOR_WHITE = {255, 255, 255};
const RGB COLOR_RED = {255, 0, 0};
const RGB COLOR_CARMINEPINK = {255, 89, 100};

// INITIALIZE MIRROR
void setup()
{
	setMood(IDLE);
	triggerState = WAITING; 

	Serial.begin(9600);

	// timer 2 is used for the servos
	servoBase.attach(PIN_SERVO_HOR);
	servoJoint.attach(PIN_SERVO_VERT);
	servoBase.write(0);
	servoBase.write(0);

	// initialize servo position values in center positions
	servoBaseUS = (MIN_US_BASE + (MAX_US_BASE - MIN_US_BASE)/2);
	servoJointUS = (MIN_US_JOINT + (MAX_US_JOINT - MIN_US_JOINT)/2);

	pinMode(PIN_DIST_TRIG, OUTPUT);
	pinMode(PIN_DIST_ECHO, INPUT);
	pinMode(LED_BUILTIN, OUTPUT);

	pinMode(PIN_RED, OUTPUT);
	pinMode(PIN_GREEN, OUTPUT);
	pinMode(PIN_BLUE, OUTPUT);

	// timer 1 is used for the ultrasonic sensor and utilizes a custom isr for non-blocking triggers
	Timer1.initialize(TIMER_US);
	Timer1.attachInterrupt(timerIsr);

	// interrupt the program when the echo pin has changed its state
	attachInterrupt(0, pinChange, CHANGE);

	// the trigger timer utilizes ticks as timing method
	triggerTimeCountdown = TICK_COUNTS * 20;

	// set default color values
	moodColor = COLOR_WHITE;
	writeMoodColor();
}

// FLASH BUILTIN LED
boolean flasher()
{
	// simple heartbeat
	flashPulse = distance * 10;
	bool f = time%(flashPulse*2) < flashPulse;
	digitalWrite(LED_BUILTIN, f);
	return f;
}

// CHANGE MOOD
void setMood(MoodState mood)
{
	MoodState prev = moodState;
	lastMoodShift = time;
	moodState = mood;
	lockWave = false;

	Serial.print(prev);
	Serial.print(" >> ");
	Serial.println(mood);
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

// MOODSTATE LOGIC
void stateMachine()
{
	// resets itself on moodshifts
	float scaledTime = (time-lastMoodShift)/750.0f;
	float phi = PI*scaledTime/2.0f;

	float wave = abs(pow(sin(phi), 2.0f));			// time-based sine wave
	float wave_safe = (!lockWave) ? wave : 1.0;		// does not count back after reaching 1
	if (!lockWave && wave >= 0.975)					// lock wave when it reaches 1
	{
		lockWave = true;
	}

	// this is a nostate and only used as default at the start of the mirror lifetime
	if (moodState == IDLE)
	{
		setMood(SENSITIVE);
	}
	// this state triggers the ultrasonic sensor to detect nearby objects
	else if (moodState == SENSITIVE)
	{
		// setup variables for state transition to fear state
		if (distance < TRIGGER_DISTANCE)
		{
			setMood(FEAR);

			// naive selective randomness
			int randomBaseState = (servoBaseUS > (MIN_US_BASE + (MAX_US_BASE - MIN_US_BASE)/2)) ?
				random(MIN_US_BASE, servoBaseUS-DIFF) :
				random(servoBaseUS+DIFF, MAX_US_BASE);

			int randomJointState = (servoJointUS > (MIN_US_JOINT + (MAX_US_JOINT - MIN_US_JOINT)/2)) ?
				random(MIN_US_JOINT, servoJointUS-DIFF) :
				random(servoJointUS+DIFF, MAX_US_JOINT);

			servoBaseTargetUS = randomBaseState;
			servoJointTargetUS = randomJointState;
		}
	}
	// this state activates the servos to move the mirror to a new selectively randomized position
	else if (moodState == FEAR)
	{
		// generate normalized amplitude value for smooth updates
		long wave_raw = wave_safe*FIXED_POINT_PRECISION_LONG;
		long wave_mapped_base = map(wave_raw, 0, FIXED_POINT_PRECISION_LONG, servoBaseUS, servoBaseTargetUS);
		long wave_mapped_joint = map(wave_raw, 0, FIXED_POINT_PRECISION_LONG, servoJointUS, servoJointTargetUS);

		// movement updates mapped to microseconds boundaries
		servoBase.write(wave_mapped_base);
		servoJoint.write(wave_mapped_joint);

		// update backlight
		lerpMoodColor(COLOR_CARMINEPINK, COLOR_RED, wave_safe);
		writeMoodColor();

		// setup variables for state transition to recovery state
		if (time > lastMoodShift+TIME_OF_FEAR)
		{
			servoBaseUS = servoBaseTargetUS;
			servoJointUS = servoJointTargetUS;

			setMood(RECOVERY);
		}
	}
	// this state leaves the mirror in its current position for a short time before becoming sensitive to nearby objects again
	else if (moodState == RECOVERY)
	{
		lerpMoodColor(COLOR_RED, COLOR_CARMINEPINK, wave_safe);
		writeMoodColor();

		// setup variables for state transition to idle state
		if (time > lastMoodShift+TIME_TO_RECOVER) 
		{
			setMood(SENSITIVE);
		}
	}
}

// ULTRASONIC SENSOR PINCHANGE EVENT
void pinChange()
{    
	switch (digitalRead(PIN_DIST_ECHO))
	{
		// start of pulse
		case HIGH:
			echo_end = 0;
			echo_start = micros();
			break;

		// end of pulse
		case LOW:
			echo_end = micros();
			long duration = echo_end - echo_start;
			distance = USTOCM(duration);
			break;
	}
}

// ULTRASONIC SENSOR PULSE TRIGGER
void triggerPulse()
{
	if (!(--triggerTimeCountdown))
	{
		triggerTimeCountdown = TICK_COUNTS;
		triggerState = ACTIVE;
	}

	if (triggerState == ACTIVE)
	{
		digitalWrite(PIN_DIST_TRIG, HIGH);
		triggerState = INACTIVE;
	}
	else if (triggerState == INACTIVE)
	{
		digitalWrite(PIN_DIST_TRIG, LOW);
		triggerState = WAITING;
	}
}

// TIMER INTERRUPT SERVICE ROUTINE
void timerIsr()
{
	triggerPulse();
}

// BACKGROUND ROUTINE
void loop() 
{
	// consistent time value per update
	time = millis();

	// behavior logic
	stateMachine(); 

	// heartbeat
	flasher(); 
}