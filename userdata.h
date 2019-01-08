/*
	Author: Lesley van Hoek
*/

/* state enums */
enum MoodState 
{ 
	IDLE,
	SENSITIVE,
	FEAR,
	RECOVERY
};
enum TriggerState
{
	WAITING,
	ACTIVE,
	INACTIVE
};

/* color struct */
struct RGB
{ 
	int r;
	int g;
	int b;
};