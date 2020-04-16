# shell
 
 Implemented backend for "abash" shell, baby brother of the Bourne Again shell. It supports the following functionalities:

1. execution of simple commands with zero or more arguments
2. definition of local environment variables (NAME=VALUE)
3. redirection of the standard input (<, <<)
4. redirection of the standard output (>, >>)
5. execution of pipelines (sequences of simple commands or subcommands separated by the pipeline operator |)
6. execution of conditional commands (sequences of pipelines separated by the command operators && and ||)
7. execution of sequences of conditional commands separated or terminated by the command terminators ; and & execution of commands in the background (&)
8. subcommands (commands enclosed in parentheses that act like a single, simple command)
9. reporting the status of the last simple command, pipeline, conditional command, or subcommand executed by setting the environment variable $? to its "printed" value (e.g., the string "0" if the value is zero).
10. directory manipulation:
- cd dirName - Change current working directory to DIRNAME
- cd  - Equivalent to "cd $HOME" where HOME is an environment variable
- dirs - Print to stdout the working directory as defined by getcwd()
11. other built-in commands:
- wait - Wait until all children of the shell process have died. The status is 0. Note: The bash wait command takes command-line arguments that specify which children to wait for (vs. all of them).

Assumes the input of a recursive descent parser with the following grammar:

<local> = VARIABLE=VALUE

<red_op> = < / << / > / >>

<redirect> = <red_op> FILENAME

<prefix> = <local> / <redirect> / <local> <prefix> / <redirect> <prefix>

<suffix> = SIMPLE / <redirect> / SIMPLE <suffix> / <redirect> <suffix>

<redList> = <redirect> / <redirect> <redList>

<simple> = SIMPLE / <prefix> SIMPLE / SIMPLE <suffix>
/ <prefix> SIMPLE <suffix>

<subcmd> = (<command>) / <prefix> (<command>) / (<command>) <redList>
/ <prefix> (<command>) <redList>

<stage> = <simple> / <subcmd>

<pipeline> = <stage> / <stage> | <pipeline>

<and-or> = <pipeline> / <pipeline> && <and-or> / <pipeline> || <and-or>

<command> = <and-or> / <and-or> ; <command> / <and-or> ;
/ <and-or> & <command> / <and-or> &
