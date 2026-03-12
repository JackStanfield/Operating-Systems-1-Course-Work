#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netinet/in.h>


char* read_file(char* filename, int* length){
    FILE* file = fopen(filename, "r");
    if (file == NULL){
        fprintf(stderr, "error when opening file: %s\n", filename);
        exit(1);
    }
    //thought strtok might not work correctly
    //we didn't cover SEEK_END or fseek I don't think but I found them online
    fseek(file, 0, SEEK_END);
    *length = ftell(file);
    //this resets the file pointer to the beginninig of the file
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(*length + 1);
    fread(buffer, 1, *length, file);
    fclose(file);
    //just in case the last character is newline
    if(*length > 0 && buffer[*length -1] == '\n'){
        (*length)--;
    }
    buffer[*length] = '\0';

    return buffer;
}

int check_chars(char* str, int length){
    for (int i = 0; i < length; i++){
        //can check if char is in alphabet through less/greater than operators
        if (str[i] != ' ' && (str[i] < 'A' || str[i] > 'Z')){
        return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {


    char *plaintext_file = argv[1];
    char *key_file = argv[2];
    int port = atoi(argv[3]);

    
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port");
        return 1;
    }

	//must use connect_socket = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    //inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    int plaintext_len;
    int key_len;

    char* plaintext = read_file(plaintext_file, &plaintext_len);
    char* key = read_file(key_file, &key_len);
    
    if (!check_chars(plaintext, plaintext_len)) {
        fprintf(stderr, "invalid chars in plaintext");
        free(plaintext);
        free(key);
        exit(1);
    }

    if (!check_chars(key, key_len)) {
        fprintf(stderr, "invalid chars in key");
        free(plaintext);
        free(key);
        exit(1);
    }

    if (key_len < plaintext_len){
        fprintf(stderr, "key is shorter than the plaintext");
        free(plaintext);
        free(key);
        exit(1);
    }

    
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        fprintf(stderr, "error creating client socket");
        exit(1);
    }

    int result = connect(
    sock, 
    (struct sockaddr*) &server_address,
    sizeof(server_address));

    if (result == -1){
        fprintf(stderr, "error when connecting to server");
        close(sock);
        exit(1);
    }

    char encCli = 'E';
    int total_bytes_sent = 0;
    
    ssize_t sent_char = send(
        sock,
        &encCli,
        1,
        0);

    if (sent_char < 1){
        fprintf(stderr, "error when sending the distinction char");
        exit(1);
    }

    int message_received = 0;
    char recChar;
    ssize_t rec_char = recv(
        sock,
        &recChar,
        1,
        0
    );

    if (recChar != 'E'){
        free(plaintext);
        free(key);
        close(sock);
        exit(1);
    }

    char lenbuf[16];
    int len = snprintf(
        lenbuf, 
        sizeof(lenbuf), 
        "%d\n", plaintext_len);

    if (len <= 0){
        fprintf(stderr, "plaintext is invalid");
        close(sock);
        free(plaintext);
        free(key);
        exit(1);
    }
        
    int sent = 0;
    while (sent < len){
        ssize_t n = send(
            sock,
            lenbuf + sent,
            len - sent,
            0
        );

        if (n <= 0) {
            fprintf(stderr, "error when sending length to server");
            close(sock);
            free(plaintext);
            free(key);
            exit(1);
        }
        sent += n;
    }
    //check plaintext len ands send to server
    if (plaintext_len > 0){
        int sent = 0;
        while (sent < plaintext_len){
            ssize_t n = send(
                sock,
                plaintext + sent,
                plaintext_len - sent,
                0
            );
            if ( n <= 0){
                fprintf(stderr, "error when sending plaintext");
                close(sock);
                free(plaintext);
                free(key);
                exit(1);
            }
            sent += n;
        }
    }
    //send the newline for the plaintext
        char newline = '\n';
        ssize_t n = send(
            sock,
            &newline,
            1,
            0 
        );
        if(n < 1){
            fprintf(stderr, "error when sending the plaintext newlind");
            close(sock);
            free(plaintext);
            free(key);
            exit(1);
        }
        //send the key to the server
         sent = 0;
        //use plaintext length because sending anymore would be unecessary
        while (sent < plaintext_len){
            ssize_t n = send(
                sock,
                key + sent,
                plaintext_len - sent,
                0);
                if (n <= 0){
                    fprintf(stderr, " error when sending key");
                    close(sock);
                    free(plaintext);
                    free(key);
                    exit(1);
                }
                sent += n;
        }
        //allocate enough space to store the plaintext sent plus a null terminator
        char* encryptedText = (char*) malloc((size_t)plaintext_len + 1);

        if (!encryptedText){
            fprintf(stderr, "error with encrypted text malloc I guess");
            close(sock);
            free(plaintext);
            free(key);
            exit(1);
            
        }

        int received = 0;
        while (received < plaintext_len){
            ssize_t n = recv(
            sock,
            encryptedText + received,
            plaintext_len - received,
            0
            );
            if ( n <= 0){
                fprintf(stderr, "error when receiving encrypted text");
                //I really should put this in like a function or something lol
                free(encryptedText);
                close(sock);
                free(plaintext);
                free(key);
                exit(1);
            }
            received += n;

        }

        
         n = recv(
            sock, &newline,
            1,
            0
        );
        if (n < 1){
            fprintf(stderr, " error when receiving newline for encrypted text from server");
            free(encryptedText);
            close(sock);
            free(plaintext);
            free(key);
            exit(1);
        }

        encryptedText[plaintext_len] = '\0';
        printf("%s", encryptedText);

        free(encryptedText);
        close(sock);
        free(plaintext);
        free(key);
        return 0;

}   




