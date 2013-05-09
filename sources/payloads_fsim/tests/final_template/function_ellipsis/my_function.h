/*
 * Analogue to kasprintf, but without inclusion of <stdarg.h>
 * in header file.
 */
#ifndef MY_FUNCTION_H 
#define MY_FUNCTION_H

char* my_kasprintf(const char* fmt, ...);

#endif /* MY_FUNCTION_H */
