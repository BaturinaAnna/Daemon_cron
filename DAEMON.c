#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>

#define MAX_LEN 1000
#define MAX_COMS 100
#define MAX_ARG 256

_Bool signal_interrupt = 0;
_Bool signal_terminate = 0;

sem_t semaphore;

void sigint_handler()
{
	signal_interrupt = 1;
}

void sigterm_handler()
{
	signal_terminate = 1;
}

void Daemon(char **argv)
{
	openlog("DAEMON_OF_INTERRUPTION", LOG_PID | LOG_CONS, LOG_DAEMON);
	int daemon_info_file = open("DAEMON_info.txt", O_CREAT|O_RDWR, S_IRWXU);
	lseek(daemon_info_file, 0, SEEK_END);
	char buf[] = "DAEMON: interrupt\n"; 
	
	write(daemon_info_file, "DAEMON: starts work\n", 21);	
	syslog(LOG_INFO, "DAEMON: starts work");
	
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigterm_handler);

	if(sem_init(&semaphore, 0, 1) == -1)
	{
		syslog(LOG_INFO, "ERROR: can't to initialize the semaphore");
		write(daemon_info_file, "ERROR: can't to initialize the semaphore\n", 42);
		signal_terminate = 1; //to end Daemon's work because of error
	}
		
	while(1)
	{
		pause();
		if (signal_interrupt == 1)
		{			
			syslog(LOG_INFO, "DAEMON: interrupt");
			write(daemon_info_file, buf, sizeof(buf));
			signal_interrupt = 0;
			
			//read commands from file
			char com_to_parse[MAX_LEN] = "";
			int daemon_file_input = open(argv[1], O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
			if(daemon_file_input == -1)
			{
				syslog(LOG_INFO, "ERROR: can't open input file");
				write(daemon_info_file, "ERROR: can't open input file\n", 30);
				signal_terminate = 1;
				break; //to end Daemon's work because of error
			}
			read(daemon_file_input, com_to_parse, MAX_LEN);
			close(daemon_file_input);
			
			int daemon_file_output = open("DAEMON_output.txt", O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
			if(daemon_file_output == -1)
			{
				syslog(LOG_INFO, "ERROR: can't open output file");
				write(daemon_info_file, "ERROR: can't open output file\n", 31);
				signal_terminate = 1;
				break; //to end Daemon's work because of error
			}
			
			//parse commands
			char * commands [MAX_COMS];
			int numb_of_com = 0;
			char * sep_com;
			sep_com = strtok(com_to_parse, "\n");
			while(sep_com != NULL)
			{	
				commands[numb_of_com] = sep_com;
				commands[numb_of_com][strlen(sep_com)] = '\0';
				numb_of_com++;
				sep_com = strtok(NULL, "\n");
			}
			//execution of commands
			pid_t pid;
			for(int i = 0; i < numb_of_com; i++)
			{
				 pid = fork();
				 if (pid == 0)
				 {
					 //parse args in command
					 char * args[MAX_ARG];
					 int numb_of_args = 0;
					 char * sep_arg;
					 sep_arg = strtok(commands[i], " ");
					 char * sep_command;
					 sep_command = sep_arg;
					 sep_arg = strtok(NULL, " ");
					 while(sep_arg != NULL)
					 {
						 args[numb_of_args] = sep_arg;
						 numb_of_args++;
						 sep_arg = strtok(NULL, " ");
					 }
					 args[numb_of_args] = NULL;
					 
					 if(sem_wait(&semaphore) == -1)
					 {
						syslog(LOG_INFO, "ERROR: can't lock the semaphore");
						write(daemon_info_file, "ERROR: can't lock the semaphore\n", 33);
						signal_terminate = 1;
						break; //to end Daemon's work because of error
					 }
					 
					 close(STDOUT_FILENO);
					 if(dup2(daemon_file_output, STDOUT_FILENO) == -1)
					 {
						syslog(LOG_INFO, "ERROR: can't duplicate a file descriptor");
						write(daemon_info_file, "ERROR: can't duplicate a file descriptor\n", 42);
						signal_terminate = 1;
						break; //to end Daemon's work because of error
					 }			
					 
					 write(daemon_info_file, "DAEMON: exec command\n", 22);
					 if(sem_post(&semaphore) == -1)
					 {
						syslog(LOG_INFO, "ERROR: can't unlock the semaphore");
						write(daemon_info_file, "ERROR: can't unlock the semaphore\n", 34);
						signal_terminate = 1;
						break; //to end Daemon's work because of error
					 }
					 if(execve(sep_command, args, NULL) == -1)
					 {
						 syslog(LOG_INFO, "ERROR: can't exec");
						 write(daemon_info_file, "ERROR: can't exec\n", 19);
						 signal_terminate = 1;
						 break; //to end Daemon's work because of error
					 }		 		 				 
				 }				 	
			}
			write(daemon_file_output, "----------\n", 12);
			close(daemon_file_output);			
		}
		if (signal_terminate == 1)
		{
			if(sem_destroy(&semaphore)== -1)
			{
				syslog(LOG_INFO, "ERROR: can't destroy the semaphore");
				write(daemon_info_file, "ERROR: can't destroy the semaphore\n", 36);
				signal_terminate = 1;
				break; //to end Daemon's work because of error
			}
			syslog(LOG_INFO, "DAEMON ends work");
			write(daemon_info_file, "DAEMON: ends work\n", 19);
			closelog();
			close(daemon_info_file);
			exit(0);	
			break;
		}
	}
}

int main(int argc, char **argv)
{
	pid_t parpid;
    if((parpid=fork())<0) 
    {                   
		printf("\ncan't fork"); 
        exit(1);                
    }   
    else if (parpid!=0) 
		exit(0);               
    setsid();         
    Daemon(argv);           
    return 0;
}
