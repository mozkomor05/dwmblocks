//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Icon*/	/*Command*/	 	/*Update Interval*/	/*Update Signal*/
	{"\x02",				"db-battery",					60,	4},

	{"\x05\xef\x84\x9c ",	"xkb-switch",					60,	3},

	{"\x04",				"db-volume",					60,	2},

	{"\x07\xef\x80\x97 ",	"date \"+%a, %d. %b - %R\"",	5,	1},
};

//sets delimeter between status commands. NULL character ('\0') means no delimeter.
static char delim[] = "<";
static unsigned int delimLen = 5;
