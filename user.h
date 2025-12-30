#pragma once
#include "common.h"

__attribute__((noreturn)) void exit(void);
void putchar(char ch);
int getchar(void);
int readfile(const char *path, char *buf, int size);
int writefile(const char *path, const char *buf, int size);