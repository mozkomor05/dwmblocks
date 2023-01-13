// Modify this file to change what commands output to your statusbar, and
// recompile using the make command.
static const Block blocks[] = {
        /*Icon*/ /*Command*/ /*Update Interval*/ /*Update Signal*/
        BLOCK("\x02", "db-battery", 60, 4),
        BLOCK("\x05\xef\x84\x9c ", "xkb-switch", 60, 3),
        BLOCK("\x04", "db-volume", 60, 2),
        BLOCK("\x07\xef\x80\x97 ", "db-clock", 5, 1),
};

// sets delimeter between status commands. NULL character ('\0') means no
// delimeter.
static const char delim[] = "<";
