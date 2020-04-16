// Backend for Bsh by Himnish Hunma 04/10/2020

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include "abash_parse.h" //contains struct for each node in abstract syntax tree

#define TEMP_NAME "tmpfile_XXXXXX"
#define NEWPATHSIZE 500

#define CHECK_STATUS(x) (WIFEXITED(x) ? WEXITSTATUS(x) : 128+WTERMSIG(x))

typedef struct zombies {
	pid_t process_id;
	struct zombies * next;
	struct zombies * prev;
} zombies;


void insert(zombies ** head, pid_t new_pid) {
	struct zombies * new_zombie = (struct zombies *)malloc(sizeof(struct zombies));
	new_zombie->process_id = new_pid;
	//insertion cases
	new_zombie->next = *head;
	new_zombie->prev = NULL;
	//now deal with head
	if (*head != NULL) (*head)->prev = new_zombie;
	*head = new_zombie;
	return;
}

void delete(zombies ** head, zombies * current) {
	if (*head == NULL) return;
	if (current == NULL) return;
	if (*head == current) {
		*head = current->next;
	}
	if (current->next != NULL) {
		current->next->prev = current->prev;
	}
	if (current->prev != NULL) {
		current->prev->next = current->next;
	}
	free(current);
	return;
}

zombies * zombie_list = NULL;

int subprocess(CMD *cmd_list);

int process(CMD *cmd_list) {
    if (cmd_list == NULL) return 0; 
	for (int i = 0; i < cmd_list->nLocal; i++) {
		if (setenv(cmd_list->locVar[i], cmd_list->locVal[i], 1) < 0) {
			perror("setenv()");
			return EXIT_FAILURE;
		}
	}

	int exit_status = 0;
	zombies * current = zombie_list;
	while (current != NULL) {
		zombies * temp = current->next;
		if (waitpid(current->process_id, &exit_status, WNOHANG)){
			fprintf(stderr, "Completed: %d (%d)\n", current->process_id, exit_status);
			delete(&zombie_list, current);
		}
		current = temp;
	}


	if (subprocess(cmd_list) == 0) {
		if (setenv("?", "0", 1) < 0) {
			perror("setenv()");
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}
	else {
		if (setenv("?", "1", 1) < 0) {
			perror("setenv()");
			return EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	}

	for (int i = 0; i < cmd_list->nLocal; i++) {
		unsetenv(cmd_list->locVar[i]);
	}
		
	return 0;
}

int subprocess(CMD *cmd_list) {
	// wait(NULL);
	//Local Var Handling: loop through local var array and set values

	if (cmd_list == NULL) return 0;
	
	//SIMPLE operator
	if (cmd_list->type == SIMPLE) {
		if (strcmp(cmd_list->argv[0], "true") == 0) {
			return EXIT_SUCCESS;
		}
		if (strcmp(cmd_list->argv[0], "false") == 0) {
			return EXIT_FAILURE;
		}
		if (strcmp(cmd_list->argv[0], "wait") == 0) {
			wait(NULL);
			int exit_status = 0;
			zombies * current = zombie_list;
			while (current != NULL) {
				zombies * temp = current->next;
				if (waitpid(current->process_id, &exit_status, WNOHANG)){
					fprintf(stderr, "Completed: %d (%d)\n", current->process_id, exit_status);
					delete(&zombie_list, current);
				}
				current = temp;
			}
		}
		char here_template[15] = TEMP_NAME;
		int n = 0;
		//check for change in directory
		if (strcmp(cmd_list->argv[0], "cd") == 0) {
			if (cmd_list->argc > 2) {
				perror(cmd_list->argv[1]);
				return EXIT_FAILURE;
			}
			//char new_path[NEWPATHSIZE];
			if (cmd_list->argc == 1) { //just go to home directory
				if (chdir(getenv("HOME")) < 0) {
					perror("chdir()");
					return EXIT_FAILURE;
				}
			}
			else {
				if (chdir(cmd_list->argv[1]) < 0) {
					perror("chdir()");
					return EXIT_FAILURE;
				}
			}
			return 0;
		} 

		//check redirection principles
		int output_fd;
		int input_fd;

		if (cmd_list->toType == NONE) {
			output_fd = STDOUT_FILENO;
		} else if (cmd_list->toType == REDIR_OUT) {
			output_fd = open(cmd_list->toFile, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (output_fd < 0) {
				perror(cmd_list->toFile);
				return EXIT_FAILURE;
			}

		} else if (cmd_list->toType == REDIR_APP) {
			output_fd = open(cmd_list->toFile, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (output_fd < 0) {
				perror(cmd_list->toFile);
				return EXIT_FAILURE;
			}
		} 

		if (cmd_list->fromType == NONE) {
			input_fd = STDIN_FILENO;
			
		} else if (cmd_list->fromType == REDIR_IN) {
			input_fd = open(cmd_list->fromFile, O_RDONLY);
			if (input_fd < 0) {
				perror(cmd_list->fromFile);
				return EXIT_FAILURE;
			}
		} else if (cmd_list->fromType == REDIR_HERE) {	//cpy contents 
			//HEREDOCS
			char * here_content = cmd_list->fromFile;
			//printf("%s\n", here_content);
			input_fd = mkstemp(here_template);
			if ((n = write(input_fd, here_content, strlen(here_content))) < 0) {
				perror("write()");
				return EXIT_FAILURE;
			}
			input_fd = open(here_template, O_RDONLY);
		}
		//printf("%d input, %d output\n", input_fd, output_fd);
		// dirs can be redirected
		if (strcmp(cmd_list->argv[0], "dirs") == 0) {
			if (cmd_list->toFile == NULL) {
				char current_directory[NEWPATHSIZE];
				if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
					perror("dirs");
					return EXIT_FAILURE;
				}
				printf("%s\n", current_directory);
				
			}
			else {
				char current_directory[NEWPATHSIZE];
				if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
					perror("dirs");
					return EXIT_FAILURE;
				}
				strcat(current_directory, "\n");
				if ((n = write(output_fd, current_directory, strlen(current_directory))) < 0) {
					perror("pwd");
					return EXIT_FAILURE;
				}
			}
			return 0;			
		}



		pid_t rc = fork();
		if (rc < 0) {
			perror("fork()");
			return EXIT_FAILURE;
		} else if (rc == 0) {		//child process
			//want to start redirection
			//dup both in and out and close
			dup2(output_fd, STDOUT_FILENO);
			dup2(input_fd, STDIN_FILENO);
			if (input_fd != STDIN_FILENO) close(input_fd);
			
			if (output_fd != STDOUT_FILENO) close(output_fd);
			if (execvp(cmd_list->argv[0], cmd_list->argv) < 0) {
				//fprintf(stderr, "%s: No such file or directory\n", cmd_list->toFile);
				perror("execvp()");
				exit(1);
			}
			return EXIT_SUCCESS;
		} else {		//parent
			//close

			if (input_fd != STDIN_FILENO) close(input_fd);
			if (output_fd != STDOUT_FILENO) close(output_fd);
			if (cmd_list->fromType == REDIR_HERE) unlink(here_template);

			int return_status;
			signal(SIGINT, SIG_IGN);
			int rc_wait = waitpid(rc, &return_status, 0);
			signal(SIGINT, SIG_DFL);
			return CHECK_STATUS(return_status);			

			//int rc_wait = wait(NULL);
			//return EXIT_SUCCESS;
		}
	}

	//PIPE operator
	if (cmd_list->type == PIPE) {
		//need input of pipe - 2 file descriptors
		typedef struct fd_pair {
			int fd[2];
		} pipe_pair;

		//int saved_stdout = dup(1);
		//int saved_stdin = dup(0);

		pipe_pair pipe_file;
		if (pipe(pipe_file.fd) < 0) {
			perror("pipe()");
			exit(1);
		}
		// to check exit status of child processes
		int leftStatus = 0;
		int rightStatus = 0;
		//use subshell to process left node
		int rc_left = fork();
		if (rc_left < 0) {
			perror("fork()");
			exit(1);
		} else if (rc_left == 0) { //child subshell

			dup2(pipe_file.fd[1], STDOUT_FILENO);
			close(pipe_file.fd[0]);
			close(pipe_file.fd[1]);
			if (process(cmd_list->left) == EXIT_FAILURE) {
				//perror("process()");
				exit(1);
			} 
			else {
				
				exit(0);
			}
		} else {
			//int rc_wait1 = waitpid(rc_left, NULL, 0);
			int rc_right = fork();
			if (rc_right < 0) {
				perror("fork()");
				exit(1);
			} else if (rc_right == 0) { //child subshell
				//dup2(saved_stdout, STDOUT_FILENO);
				dup2(pipe_file.fd[0], STDIN_FILENO);
				close(pipe_file.fd[0]);
				close(pipe_file.fd[1]);
				if (process(cmd_list->right) == EXIT_FAILURE) {
					//perror("process()");
					exit(1);
				} 
				else {
					
					exit(0);
				}
			} 
			close(pipe_file.fd[0]);
			close(pipe_file.fd[1]);
			signal(SIGINT, SIG_IGN);
			int rc_wait_left = waitpid(rc_left, &leftStatus, 0);
			int rc_wait_right = waitpid(rc_right, &rightStatus, 0);
			signal(SIGINT, SIG_DFL);
			return CHECK_STATUS(leftStatus) | CHECK_STATUS(rightStatus);
			
		}
		//int rc_wait1 = waitpid(rc_left, NULL, 0);
		

	}

	if (cmd_list->type == SEP_AND) {
		//printTree(cmd_list, 0);
		CMD * new_root = cmd_list; // must be right child left child
		if (cmd_list->right->type == SEP_OR || cmd_list->right->type == SEP_AND) {
			while (new_root->right->type == SEP_OR || new_root->right->type == SEP_AND) {
				CMD * temp1 = new_root->right; // new root
				new_root->right = temp1->left;
				temp1->left = new_root;
				new_root = temp1;
			}
			//printf("calling process\n");
			return process(new_root);
		}
		/*
		printf("___________________________________\n");
		printTree(new_root, 0);
		printf("_________________________\n");*/
		
		if (process(new_root->left) == EXIT_FAILURE) {
				return EXIT_FAILURE;
			} 
			else {
				return process(new_root->right);
			}
	}

	if (cmd_list->type == SEP_OR) {
		//printTree(cmd_list, 0);
		CMD * new_root = cmd_list; // must be right child left child
		if (cmd_list->right->type == SEP_OR || cmd_list->right->type == SEP_AND) {
			while (new_root->right->type == SEP_OR || new_root->right->type == SEP_AND) {
				CMD * temp1 = new_root->right; // new root
				new_root->right = temp1->left;
				temp1->left = new_root;
				new_root = temp1;
			}
			//printf("calling process\n");
			return process(new_root);
		}
		/*
		printf("___________________________________\n");
		printTree(new_root, 0);
		printf("______________________\n");*/
		
		if (process(new_root->left) == EXIT_SUCCESS) {
				return EXIT_SUCCESS;
			} 
			else {
				return process(new_root->right);
			}

	}

	if (cmd_list->type == SEP_END) {
		return process(cmd_list->left) | process(cmd_list->right);
	}

	if (cmd_list->type == SEP_BG) {
		//add child's pid to lnked list in child, fork a subshell and 
		pid_t rc = fork();
		if (rc < 0) {
			perror("fork()");
			return EXIT_FAILURE;
		} else if (rc == 0) {		//child process
			
			if (process(cmd_list->left) == EXIT_FAILURE) {
				//fprintf(stderr, "%s: No such file or directory\n", cmd_list->toFile);
				exit(1);
			}
			else exit(0);
		} else {		//parent
			//add child to list of background processes
			insert(&zombie_list, rc);
			fprintf(stderr, "Backgrounded: %d\n", rc);
			return process(cmd_list->right);
		}

	}

	if (cmd_list->type == SUBCMD) {
		char here_template[15] = TEMP_NAME;
		int n = 0;
		//check redirection principles
		int output_fd;
		int input_fd;

		if (cmd_list->toType == NONE) {
			output_fd = STDOUT_FILENO;
		} else if (cmd_list->toType == REDIR_OUT) {
			output_fd = open(cmd_list->toFile, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (output_fd < 0) {
				perror(cmd_list->toFile);
				return EXIT_FAILURE;
			}

		} else if (cmd_list->toType == REDIR_APP) {
			output_fd = open(cmd_list->toFile, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (output_fd < 0) {
				perror(cmd_list->toFile);
				return EXIT_FAILURE;
			}
		} 

		if (cmd_list->fromType == NONE) {
			input_fd = STDIN_FILENO;
			
		} else if (cmd_list->fromType == REDIR_IN) {
			input_fd = open(cmd_list->fromFile, O_RDONLY);
			if (input_fd < 0) {
				perror(cmd_list->fromFile);
				return EXIT_FAILURE;
			}
		} else if (cmd_list->fromType == REDIR_HERE) {	//cpy contents 
			//HEREDOCS
			char * here_content = cmd_list->fromFile;
			input_fd = mkstemp(here_template);
			if ((n = write(input_fd, here_content, strlen(here_content))) < 0) {
				perror("write()");
				return EXIT_FAILURE;
			}
			input_fd = open(here_template, O_RDONLY);
		}
		//fork a subshell that process the rest of the tree
		pid_t rc = fork();
		if (rc < 0) {
			perror("fork()");
			return EXIT_FAILURE;
		} else if (rc == 0) {		//child process
			//want to start redirection
			//dup both in and out and close
			dup2(output_fd, STDOUT_FILENO);
			dup2(input_fd, STDIN_FILENO);
			if (input_fd != STDIN_FILENO) close(input_fd);
			if (output_fd != STDOUT_FILENO) close(output_fd);
			if (process(cmd_list->left) == EXIT_FAILURE) {
				exit(1);
			}
			else exit(0);
		} else {		//parent
			//close
			if (input_fd != STDIN_FILENO) close(input_fd);
			if (output_fd != STDOUT_FILENO) close(output_fd);
			if (cmd_list->fromType == REDIR_HERE) unlink(here_template);
			int return_status;
			signal(SIGINT, SIG_IGN);
			int rc_wait = waitpid(rc, &return_status, 0);
			signal(SIGINT, SIG_DFL);
			return CHECK_STATUS(return_status);	
		}
	}
	return 0;
}