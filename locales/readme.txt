Here you can find locales configuration files.

(!) The idea and several locale files are derived from Bynets UnrealIRCd distribution (See http://www.bynets.org/)

*** File structure ***

Each file consists of 5-7 lines:
1: List of additional allowed characters

2: List of additional allowed multibyte characters ranges. In form: Sa_1 Ea_1 Sa_2 Ea_2 Sb_1 Eb_1 Sb_2 Eb_2... Total numbers count should be dividend of 4
   Sx_1 Start of highest byte
   Ex_1 End of highest byte
   Sx_2 Start of lowest byte
   Ex_2 End of lowest byte

3: List of additional characters that should be treated as upper-case

4: 255 characters table - to-lower case conversion table. 
Can be usefull for example for comparing nicknames that contains similar-looking characters with different codes.

5: 255 characters table - to-upper case conversion table. 
Can be usefull for example for comparing nicknames that contains similar-looking characters with different codes.

6: List of additional UTF-8 allowed characters

7: List of additional UTF-8 ranges (character followed by 1-byte "range").

8: List of additional UTF-8 ranges (i.e. start1, end1, start2, end2,... UTF8-characters between each start-end pair assumed valid).

*** Line format ***

Each line can be list of characters or decimal/hexadecimal/octal codes divided by spaces or commas in form like:
0 1 2 / 00 01 02 / 0x01 0x02 0x03...
or
'a', 'b', 'c'
or combined:
x01, 002 'a', 'b', 'c',

It is also possible to write plain-text line of characters. In this case it should begin with a . (dot) character. For example:
.abcdefABCDEF23432*&^*
In this case every character of line except first dot specifies one character-code for table

*** Notice ***

"bynets" directory contains tables from Bynets' UnrealIRCd distribution. You might find them useful.

*** TODO ***

- UTF-8 collation rules (Inapplieable to InspIRCd atm).
