#include "photon.h"
#include <stdio.h>
#include <string.h>

#define INPUT_BYTES 8

int main(void){
	uint8_t text[INPUT_BYTES];
	uint8_t hash[16];
	memset(text,'a',sizeof(uint8_t)*INPUT_BYTES);

	photon128(text,INPUT_BYTES,hash);

	printf("Input text is: ");
	for(int i=0; i<INPUT_BYTES; i++){
		printf("%c",text[i]);
	}

	printf("\nHashed result is: ");
	for(int i=0; i<16; i++){
		printf("%x", hash[i]);
	}
	printf("\n");

	return 0;
}
