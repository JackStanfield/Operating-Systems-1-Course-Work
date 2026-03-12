#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int read_line(int socket, char* buffer, int max){
	int idx = 0;
	while (idx < max -1){
		char a; 
		int n = recv(socket, &a, 1, 0);
		if ( n <= 0) return -1;
		if (a == '\n') break;
		buffer [idx++] = a;
	}
	buffer[idx] = '\0';
	return idx;
}

char decrypt_char(char cipher, char key) {

    int cipherv;
    if (cipher == ' ')
        cipherv = 26;
    else
        cipherv = cipher - 'A';

    int keyv;
    if (key == ' ')
        keyv = 26;
    else
        keyv = key - 'A';

    // Reverse the encryption
    int plainv = (cipherv - keyv + 27) % 27;

    if (plainv == 26)
        return ' ';
    else
        return 'A' + plainv;
}


int main(int argc, char* argv[]) {
	
    int port = atoi(argv[1]);
	int PID_array[5] = {-1,-1,-1,-1,-1 };
	//create socket
	int listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in bind_addr;
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(port);
	bind_addr.sin_addr.s_addr = INADDR_ANY;


	int bind_result = bind(
		listen_socket,
		(struct sockaddr*) &bind_addr,
		sizeof(bind_addr)
	);
	if (bind_result == -1){
		fprintf(stderr, "Error on bind");
		return 1;
	}


	int listen_result = listen(listen_socket, 5);
	if(listen_result == -1){
		fprintf(stderr,"Error when listening");
		return 1;
	}

	while(1){
        for(int i = 0; i < 5; i++){
                if(PID_array[i] != -1){
                    int status;
                    pid_t w = waitpid(PID_array[i], &status, WNOHANG);
                    if(w == PID_array[i]){
                        PID_array[i] = -1;
                    }
                }
            }
		//might want to wait here instead of here as well.
		struct sockaddr_in client_addr;
		socklen_t client_addr_size = sizeof(client_addr);
		int communication_socket = accept(
			listen_socket,
			(struct sockaddr*) &client_addr,
			&client_addr_size
		);
		if (communication_socket == -1){
			fprintf(stderr,"Error on accept");
			continue;
			//failed to accept so continue loop
		}

		pid_t fork_result = fork();

		if (fork_result == 0){
			//child

			close(listen_socket);

			char encServ = 'E';
			int total_bytes_sent = 0;
			
		//Below sends the character E to let the client know this is the
		//enc server
			ssize_t sent_char = send(
				communication_socket,
				&encServ,
				1,
				0
			);
			if (sent_char < 1){
				fprintf(stderr, "Error when sending distinction char");
			}

			//Below receives the character send by the client,
			//Looking for an E confirm communication with enc client
			int message_received = 0;
			char recChar;
			ssize_t rec_char = recv(
				communication_socket, 
				&recChar,
				1,
				0
			);

			if (recChar != 'E'){
				close(communication_socket);	
				exit(1);
			}

			//clientwill send the length of the key and crypticText first
			//Get length to use for other receivals
			char str_len[10];
			read_line(communication_socket, str_len, sizeof(str_len));
			int length = atoi(str_len);

			char* crypticText = malloc(length + 1);
			int received = 0;
			while (received < length){
				ssize_t n = recv(communication_socket, crypticText + received, length - received, 0);
				if (n <= 0){
					fprintf(stderr, "error when reading crypticText in enc server");
				}
				received += n;
			}
			crypticText[length] = '\0';

			char dummy;
			// this removes the newline
			recv(communication_socket, &dummy, 1, 0);

			char* key = malloc(length + 1);
			received = 0;
			while (received < length){
				ssize_t n = recv(communication_socket, key + received, length - received, 0 );
				if (n <= 0){
					fprintf(stderr, "error when reading key in enc server");
				}
				received += n;
			}
			key[length] = '\0';


			for(int i = 0; i < length; i++){
				crypticText[i] = decrypt_char(crypticText[i], key[i]);
			}

			int total_len = length + 1;
			char *buff = malloc(total_len);

			memcpy(buff, crypticText, length);
			buff[length] = '\n';

			int sent = 0;
			while (sent < total_len){
				ssize_t n = send(communication_socket, buff + sent, total_len - sent, 0);
				if(n <= 0){
					fprintf(stderr, "error when sending encrypted text");
					exit(1);
				}
				sent += n;
			}


			free(crypticText);
			free(key);
			free(buff);

			close(communication_socket);
			exit(0);
		}else{
			//parent
            for(int i = 0; i<5;i++){
                if(PID_array[i] != -1){
                    PID_array[i] = fork_result;
                    break;
                }
            }

            //Wait on the child process array using WNOHANG to not block the parent process
            for(int i = 0; i < 5; i++){
                if(PID_array[i] != -1){
                    int status;
                    pid_t w = waitpid(PID_array[i], &status, WNOHANG);
                    if(w == PID_array[i]){
                        PID_array[i] = -1;
                    }
                }
            }

			close(communication_socket);
			//wait on the running child processes

		}


		//accept and fork a child process which will check if its
		//communicating with the enc_client and then do the encryption and send back
		// then in the end of the while loop in the parent we wait on the child process'
		//array
	}


}
