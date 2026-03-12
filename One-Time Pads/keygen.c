#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int main(int args, char *argv[]) {
	char* keyLenStr = argv[1];
    int keyLen = atoi(keyLenStr);
    if(keyLen > 1024){
        fprintf(stderr, "key length greater than 1024\n");
        exit(1);
    }
    srand(time(NULL));

    char alphabet[27] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ "
    };

    char key[1025]  = "";
    for(int i = 0; i<keyLen; i++){
        int index = rand() % 27;
        key[i] = alphabet[index];
    }
    key[keyLen] = '\0';

    printf("%s\n", key);

}
