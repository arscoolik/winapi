#include "stdio.h"
#include "Windows.h"

void __stdcall c(int int_f, int int_s, int int_t, float fl_f, float fl_s, float fl_t) {
	printf("int sum : %d\n", int_f + int_s + int_t);
	printf("float sum : %f\n", fl_f + fl_s + fl_t);
}

extern int __fastcall asm(int int_f, int int_s, int int_t, float fl_f, float fl_s, float fl_t);

int main() {
	asm();
	return 0;
}