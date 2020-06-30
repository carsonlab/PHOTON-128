#include "photon.h"
#include <stdio.h>


int main(void){
	uint8_t text[16];
	uint8_t hash[16];
	memset(text,0,sizeof(uint8_t)*16);

	photon128(text,16,hash);

	printf("Input text is: ");
	for(int i=0; i<16; i++){
		printf("%d",text[i]);
	}

	printf("\nHashed result is: ");
	for(int i=0; i<16; i++){
		printf("%d", hash[i]);
	}

	return 0;
}
