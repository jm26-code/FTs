#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>

struct termios old;

void reset_input_mode(void){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
}

int main(){
	/*int fd = isatty(STDIN_FILENO);
	char* ttyName = ttyname(fd);
*/
	tcgetattr(STDIN_FILENO, &old);

	struct termios new;
	tcgetattr(STDIN_FILENO, &new);

	new.c_lflag &= ~(ICANON | ECHO);
	new.c_oflag &= ~(OPOST);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);

	//puts(ttyName);

	atexit(reset_input_mode);

	while(1){

	}
/*
	char buff[100];
	
	int n_read = read(0, buff, 100);

	write(1, buff, strlen(buff));

	close(fd);

	//tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
*/
	return 0;
}
