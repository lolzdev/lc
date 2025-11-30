#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "lexer.h"

int main(void)
{
	FILE *fp = fopen("test.c", "r");
	usize size = 0;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = malloc(size+1);
	fread(src, size, 1, fp);
	fclose(fp);
	src[size] = '\0';

	arena a = arena_init(0x1000 * 0x1000 * 64);
	lexer *l = lexer_init(src, size, &a);

	arena_deinit(a);

	return 0;
}
