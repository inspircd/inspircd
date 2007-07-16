/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __COLOURS_H
#define __COLOURS_H

#define TRED FOREGROUND_RED | FOREGROUND_INTENSITY
#define TGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define TYELLOW FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
#define TNORMAL FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE
#define TWHITE TNORMAL | FOREGROUND_INTENSITY
#define TBLUE FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY

inline void sc(WORD color) { SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color); }

/* Handles colors in printf */
int printf_c(const char * format, ...)
{
	// Better hope we're not multithreaded, otherwise we'll have chickens crossing the road other side to get the to :P
	static char message[500];
	static char temp[500];
	int color1, color2;

	/* parse arguments */
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, 500, format, ap);
	va_end(ap);

	/* search for unix-style escape sequences */
	int t;
	int c = 0;
	const char * p = message;
	while (*p != 0)
	{
		if (*p == '\033')
		{
			// Escape sequence -> copy into the temp buffer, and parse the color.
			p++;
			t = 0;
			while ((*p) && (*p != 'm'))
			{
				temp[t++] = *p;
				++p;
			}

			temp[t] = 0;
			p++;

			if (*temp == '[')
			{
				if (sscanf(temp, "[%u;%u", &color1, &color2) == 2)
				{
					switch(color2)
					{
					case 32:		// Green
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);		// Yellow
						break;

					default:		// Unknown
						// White
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
						break;
					}
				}
				else
				{
					switch (*(temp+1))
					{
						case '0':
							// Returning to normal colour.
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
							break;

						case '1':
							// White
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), TWHITE);
							break;

						default:
							char message[50];
							sprintf(message, "Unknown color code: %s", temp);
							MessageBox(0, message, message, MB_OK);
							break;
					}
				}
			}
		}

		putchar(*p);
		++c;
		++p;
	}

	return c;
}

#endif

