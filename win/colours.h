/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef COLORS_H
#define COLORS_H

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

