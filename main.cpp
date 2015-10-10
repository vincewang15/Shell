//
//  main.cpp
//  Project1
//
//  Created by Vince Wang on 9/24/15.
//  Copyright © 2015 Vince Wang. All rights reserved.
//

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unordered_map>
#include <cstdio>
#include <fstream>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits>
#include "utility.hpp"
using namespace std;


#define setting_erro -1;

// environment
extern char **environ;
// global variable
unordered_map<string, string> local_words;
vector<string> history;
int last_fore_status;
pid_t last_back_pid;

bool substitution_display = false, batch_mode = false;
int debug_level = 0;
string batch_file = "";


string find_var_sub(vector<string> &command_args)
{
    string cmd_sub;
    size_t n = command_args.size();
    for (size_t i = 0; i < n; i++)
    {
        /* if there is a variable substituition */
        if (command_args[i][0] == '$')
        {
            string var_tofind = command_args[i].substr(1, command_args[i].size()-1);    // remove dollar sign
            if (var_tofind == "$") {
                cmd_sub += to_string(getpid()) + " ";
                continue;
            }
            else if (var_tofind == "?")
            {
                cmd_sub += to_string(last_fore_status) + " ";
                continue;
            }
            else if (var_tofind == "!")
            {
                cmd_sub += to_string(last_back_pid) + " ";
                continue;
            }
            /* find local variable first */
            unordered_map<string, string>::iterator it = local_words.find(var_tofind);
            if (it != local_words.end())
            {
                cmd_sub += it->second + " ";
            }
            /* find global environment next */
            else if (char *value = getenv(var_tofind.c_str()))
            {
                string tmp = value;
                cmd_sub += tmp;
                cmd_sub += " ";
            }
            else
            {
                return "echo undefined variable";
            }
        }
        else
        {
            cmd_sub += command_args[i] + " ";
        }
    }
    return cmd_sub;
}



int parse_background_setting(char * argv[], int argc, bool &substitution_display_, int &debug_level_, bool &batch_mode_, string &filename)
{
    /* go through each token */
    int index = 1;
    while (index < argc) {
        string token = argv[index];
        if (token == "-x") {
            substitution_display_ = true;
        }
        else if (token == "-d")
        {
            if (++index < argc && isdigit(argv[index][0]))
            {
                debug_level_ = stoi(argv[index]);   // assume debug level is a number no larger than 10
            }
            /* if debug level is not given, print error message and return */
            else
            {
                if (debug_level > 0) {
                    cout << "Usage: sish [-x] [-d <level>] [-f file [arg] ... ]" << endl;
                }
                return setting_erro;
            }
        }
        else if (token == "-f")
        {
            if (++index < argc)
            {
                batch_mode_ = true;
                filename = argv[index];
                /* parse argument and store it into $1, $2... */
                int i = 0;
                string remainder;
                while (++index < argc) {
                    string arg = argv[index];
                    /* ignore comment */
                    if (arg.find("#") != string::npos)
                    {
                        remainder = arg.substr(0, arg.find("#"));
                        break;
                    }
                    local_words[to_string(++i)] = argv[index];
                }
                if (remainder != "") {
                    local_words[to_string(++i)] = remainder;
                }
            }
            /* if no file provided, print error message and return */
            else
            {
                if (debug_level > 0) {
                    cout << "Usage: sish [-x] [-d <level>] [-f file [arg] ... ]" << endl;
                }
                return setting_erro;
            }
        }
        index++;
    }
    return 0;
}



bool built_in(string command_name)
{
    vector<string> built_in_dict = {"show", "set", "unset", "export", "unexport", "environ", "chdir", "exit", "wait", "clr", "dir", "echo", "help", "pause", "history", "repeat", "kill"};
    // search internal command dictionary
    for(auto str : built_in_dict)
    {
        if (str == command_name)
        {
            return true;
        }
    }
    return false;
}


void my_waitpid(pid_t& pid, string& wait_message)
{
    while (waitpid(pid, &last_fore_status, WUNTRACED|WCONTINUED) > 0)
    {
        if (WIFSTOPPED(last_fore_status))
        {
            wait_message = "stopped";
        }
        else if (WIFCONTINUED(last_fore_status))
        {
            wait_message = "continued";
        }
        else if (WIFSIGNALED(last_fore_status))
        {
            wait_message = "exited signal = " + to_string(last_fore_status);
        }
        else if (WIFEXITED(last_fore_status))
        {
            wait_message = "exited status = " + to_string(last_fore_status);
        }
        cout << "waitpid received: pid = " << pid << "     " << wait_message << endl;
    }
}


void command_clr()
{
    cout << "\033[2J\033[1;1H";
}


void command_set(vector<string> &command_args)
{
    if (command_args.size() != 3)
    {
        if ((debug_level > 0))  cout << "usage: set <word1> <word2>" << endl;
        return;
    }
    local_words[command_args[1]] = command_args[2];
}


void command_unset(string w1)
{
    local_words.erase(w1);
}



void command_repeat(vector<string> &command_args)
{
    int n = (int)history.size() - 2;
    /* valid input check */
    if(command_args.size() == 2 && isnumber(command_args[1]))
    {
        int t = stoi(command_args[1]) - 1;
        n= min(t, n);
    }
    else if (command_args.size() > 1)
    {
        if (debug_level > 0)    cout << "usage: repeat [number]" << endl;
        return;
    }
    /* exception */
    if (history.size() <= 1)
    {
        cout << "No history" << endl;
        return;
    }
    /* show n command */
    cout << history[n] << endl;
    /* repeat command */
    vector<string> args;
    args = parse_command(history[n]);
    if (built_in(args[0]))
    {
        execute_command(args);
    }
    else
    {
        bool out_red=false, in_red=false;
        string out_name, in_name;
        args = parse_external(history[n], out_red, out_name, in_red, in_name);
        execute_external(args, out_red, out_name, in_red, in_name);
    }
}


void command_history(vector<string> &command_args)
{
    int n = 100;
    /* valid input check */
    if (command_args.size() == 2 && isnumber(command_args[1]))
    {
        n = stoi(command_args[1]);
    }
    else if (command_args.size() > 1)
    {
        if (debug_level > 0)    cout << "usage: history [number]" << endl;
        return;
    }
    // no history stored, just return
    if (history.size() <= 1)
    {
        cout << "No history" << endl;
        return;
    }
    if (command_args.size() > 1)
    {
        n=stoi(command_args[1]);
    }
    int temp = (int)history.size() - n - 1;
    int i = max(temp, 0), j = 1;
    for(;i<history.size();++i,++j)
    {
        cout << j << " " << history[i] << endl;
    }
}


void command_pause()
{
    cout << "Paused!"<<endl;
    cin.ignore(numeric_limits<streamsize>::max(),'\n');
}


void command_show(vector<string> &command_args)
{
    string show = find_var_sub(command_args);
    cout << show.substr(show.find(" ") + 1, show.size() - show.find(" ") - 1) << endl;  // find_var_sub function returns the command after substituition
}



void command_echo(vector<string> &command_args)
{
    string show = find_var_sub(command_args);
    cout << show.substr(show.find(" ") + 1, show.size() - show.find(" ") - 1) << endl;   // find_var_sub function returns the command after substituition
}



void command_export(std::vector<string> &command_args)
{
    /* valid input check */
    if (command_args.size() != 3)
    {
        if (debug_level > 0)    cout << "usage: export <word1> <word2>" << endl;
        return;
    }
    string temp = command_args[1] + "=" +command_args[2];
    char* s = new char[temp.size()+1];
    strcpy(s,temp.c_str());
    putenv(s);
}



void command_unexport(vector<string> &command_args)
{
    unsetenv(command_args[1].c_str());
}



void command_exit(vector<string> &command_args)
{
    /* valid input check */
    if (command_args.size() > 2 || (command_args.size() == 2 && !isnumber(command_args[1]))) {
        if (debug_level > 0)    cout << "usage: exit <number>" << endl;
        return;
    }
    kill(0, SIGTERM);   // kill all running processes by default signal SIGTERM
    int status = command_args.size() == 2 ? stoi(command_args[1]) : 0;
    exit(status);
}


void command_environ()
{
    for(char **current = environ; *current; current++)
    {
        puts(*current);
    }
}


void command_chdir(vector<string> &command_args)
{
    const char *directory = command_args[1].c_str();
    if (chdir(directory) != 0  && debug_level > 0)
    {
        cout << "-sish: " << command_args[0] << ": " << command_args[1] << ": No such file or directory" << endl;
    }
}


void command_dir()
{
    DIR *working_dir;
    struct dirent *pent;
    /* go through every file under working directory */
    working_dir=opendir(".");      //"." refers to the current dir
    while ((pent = readdir(working_dir)))
    {
        if (pent->d_name[0] != '.' && strcmp(pent->d_name, "..") != 0)
        {
            cout << pent->d_name << endl;
        }
    }
    closedir(working_dir);
}



void command_wait(vector<string> &command_args)
{
    string msg;
    /* valid input check */
    if (command_args.size() != 2 || !isnumber(command_args[1]))
    {
        if (debug_level > 0)    cout << "Usage: wait <number>" << endl;
    }
    else if (command_args[1] == "-1")
    {
        pid_t pid_to_wait = 0;
        my_waitpid(pid_to_wait, msg);
    }
    else
    {
        pid_t pid_to_wait = stoi(command_args[1]);
        my_waitpid(pid_to_wait, msg);
    }
}


void command_kill(vector<string> &command_args)
{
    /* valid input check */
    if (command_args.size() == 3 && (!isnumber(command_args[1].substr(1, command_args.size() - 1)) || !isnumber(command_args[2])))
    {
        if (debug_level > 0)    cout << "Usage: kill [number] pid" << endl;
        return;
    }
    else if (command_args.size() == 2 && !isnumber(command_args[1]))
    {
        if (debug_level > 0)    cout << "Usage: kill [number] pid" << endl;
        return;
    }
    else if (command_args.size() > 3)
    {
        if (debug_level > 0)    cout << "Usage: kill [number] pid" << endl;
        return;
    }
    pid_t pid;
    int sig = SIGTERM;
    if (command_args.size() >= 3)
    {
        pid = stoi(command_args[2]);
        sig = 0 - stoi(command_args[1]);
    }
    else
    {
        pid = stoi(command_args[1]);
    }
    kill(pid, sig);
}


void command_help()
{
    pid_t pid;
    if ((pid = fork()) < 0 && debug_level > 0)
    {
        cout << "fork error" << endl;
    }
    else if (pid == 0)
    {
        if (execlp("more", "more -p", "readme", NULL) < 0 && debug_level > 0)
        {
            perror("execute error");
        }
        exit(1);
    }
    string msg;
    my_waitpid(pid, msg);    // parent waits for child
}




void execute_command(vector<string> &command_args)
{
    if (command_args[0] == "exit")
    {
        command_exit(command_args);
    }
    else if(command_args[0] == "clr")
    {
        command_clr();
    }
    else if (command_args[0] == "echo")
    {
        command_echo(command_args);
    }
    else if (command_args[0] == "set")
    {
        command_set(command_args);
    }
    else if (command_args[0] == "show")
    {
        command_show(command_args);
    }
    else if (command_args[0] == "export")
    {
        command_export(command_args);
    }
    else if (command_args[0] == "unexport")
    {
        command_unexport(command_args);
    }
    else if (command_args[0] == "environ")
    {
        command_environ();
    }
    else if (command_args[0] == "chdir")
    {
        command_chdir(command_args);
    }
    else if (command_args[0] == "history")
    {
        command_history(command_args);
    }
    else if (command_args[0] == "unset")
    {
        command_unset(command_args[1]);
    }
    else if (command_args[0] == "repeat")
    {
        command_repeat(command_args);
    }
    else if (command_args[0] == "pause")
    {
        command_pause();
    }
    else if (command_args[0] == "dir")
    {
        command_dir();
    }
    else if (command_args[0] == "kill")
    {
        command_kill(command_args);
    }
    else if (command_args[0] == "wait")
    {
        command_wait(command_args);
    }
    else if (command_args[0] == "help")
    {
        command_help();
    }
}



vector<string> parse_external(string inputstring,bool &out_red,string &out_name,bool &in_red,string &in_name)
{
    vector<string> ret;
    string temp="";
    for(int i=0;i<inputstring.size();++i){
        temp="";
        if(inputstring[i]==' '||inputstring[i]=='\n'||inputstring[i]=='\t'||inputstring[i]=='\0'){
            continue;
        }else if (inputstring[i]=='>'){
            out_red=true;
            i++;
            while(i<inputstring.size()&&inputstring[i]!=' '&&inputstring[i]!='\n'&&inputstring[i]!='\t'&&inputstring[i]!='\0'&&inputstring[i]!='<'){
                out_name=out_name+inputstring[i];
                i++;
            }
            i--;
        }else if (inputstring[i]=='<'){
            in_red=true;
            i++;
            while(i<inputstring.size()&&inputstring[i]!=' '&&inputstring[i]!='\n'&&inputstring[i]!='\t'&&inputstring[i]!='\0'&&inputstring[i]!='>'){
                in_name=in_name+inputstring[i];
                i++;
            }
            i--;
        }else{
            temp=temp+inputstring[i];
            i++;
            while(i<inputstring.size()&&inputstring[i]!=' '&&inputstring[i]!='\n'&&inputstring[i]!='\t'&&inputstring[i]!='\0'&&inputstring[i]!='<'&&inputstring[i]!='>'){
                temp=temp+inputstring[i];
                i++;
            }
            i--;
            ret.push_back(temp);
        }
    }
    return ret;
}


void child_signo_hdlr()
{
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = SIG_DFL;
    if (sigaction(SIGINT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGINT signal handler");
    }
    if (sigaction(SIGQUIT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGQUIT signal handler");
    }
    if (sigaction(SIGTSTP, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGTSTP signal handler");
    }
    if (sigaction(SIGCONT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGCONT signal handler");
    }
    if (sigaction(SIGTERM, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGTERM signal handler");
    }
    if (sigaction(SIGABRT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGABRT signal handler");
    }
    if (sigaction(SIGALRM, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGALRM signal handler");
    }
    if (sigaction(SIGHUP, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGHUP signal handler");
    }
}


void execute_external(vector<string> &command_args, bool out_red, string out_name, bool in_red, string in_name)
{
    bool background_process = false;
    /* check foreground or background */
    if (command_args[command_args.size() - 1] == "!")
    {
        background_process = true;
        command_args.pop_back();    // if background, remove last ampersand
    }
    /* convert argument vector to char* array */
    char **external_args = new char*[command_args.size() + 1];
    for (size_t i = 0; i < command_args.size(); i++)
    {
        external_args[i] = new char[command_args[i].size() + 1];
        strcpy(external_args[i], command_args[i].c_str());
    }
    external_args[command_args.size()] = NULL;   // last null string
    /* fork/exev */
    pid_t pid;
    if ((pid = fork()) < 0 && debug_level > 0)
    {
        cout << "fork error" << endl;
    }
    else if (pid == 0)
    {
        /* child process */
        child_signo_hdlr();    // install signal handler
        /* if run in background, start a new group */
        if (background_process && setpgid(0,  0) == -1 && debug_level > 0)
        {
            perror("setpgid error");
        }
        /* stdin/stdout redirection */
        FILE *outfile,*infile;
        if(out_red)
        {
            char* s = new char[out_name.size()+1];
            strcpy(s,out_name.c_str());
            outfile = fopen(s, "w+");
            dup2(fileno(outfile), 1);
        }
        if(in_red)
        {
            char* s = new char[in_name.size()+1];
            strcpy(s,in_name.c_str());
            outfile = fopen(s, "r");
            dup2(fileno(infile), 0);
        }
        /* find the file and execute it */
        if (execve(command_args[0].c_str(), external_args, environ) < 0)
        {
            if (execvp(*external_args, external_args) < 0 && debug_level > 0)
            {
                perror("execute error");
            }
        }
        exit(1);
    }
    /* parent process, group child into background or foreground */
    if (!background_process)
    {
        string str;
        my_waitpid(pid, str);    // to capture fore child status
    }
    else
    {
        // if background process, display to terminal the process ID numbers for any processes associated with that command
        cout << "background PID: " << pid << endl;
        last_back_pid = pid;
    }
}


void parent_signo_hdlr()
{
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = SIG_IGN;
    /* install signal hdlrn */
    if (sigaction(SIGINT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGINT signal handler");
    }
    if (sigaction(SIGQUIT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGQUIT signal handler");
    }
    if (sigaction(SIGTSTP, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGTSTP signal handler");
    }
    if (sigaction(SIGABRT, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGABRT signal handler");
    }
    if (sigaction(SIGALRM, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGALRM signal handler");
    }
    if (sigaction(SIGHUP, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGHUP signal handler");
    }
    if (sigaction(SIGTERM, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGTERM signal handler");
    }
    if (sigaction(SIGUSR1, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGUSR1 signal handler");
    }
    if (sigaction(SIGUSR2, &action, 0) && debug_level > 0)
    {
        perror("could not install SIGUSR2 signal handler");
    }
}


void execute_Pipe(vector<string> commands,int num_cmds,bool background_process)
{
    int filedes[2]; // pos. 0 output, pos. 1 input of the pipe
    int filedes2[2];
    vector<int>  p;
    pid_t pid;
    for(int i=0;i<num_cmds;++i)
    {
        if (i % 2 != 0){
            pipe(filedes); // for odd i
        }else{
            pipe(filedes2); // for even i
        }
        pid=fork();
        if(pid==-1)
        {
            if (i != num_cmds - 1)
            {
                if (i % 2 != 0){
                    close(filedes[1]); // for odd i
                }else{
                    close(filedes2[1]); // for even i
                }
            }
            if(debug_level > 0)
                printf("Child process could not be created\n");
            return;
        }
        if(pid==0)
        {
            /* child process */
            child_signo_hdlr();    // install signal handler
            if (background_process && setpgid(0,  0) == -1 && debug_level > 0)
            {
                perror("setpgid error");
            }
            // If we are in the first command
            if (i == 0){
                dup2(filedes2[1], STDOUT_FILENO);
            }
            // If we are in the last command, depending on whether it
            // is placed in an odd or even position, we will replace
            // the standard input for one pipe or another. The standard
            // output will be untouched because we want to see the
            // output in the terminal
            else if (i == num_cmds - 1)
            {
                if (num_cmds % 2 != 0){ // for odd number of commands
                    dup2(filedes[0],STDIN_FILENO);
                }else{ // for even number of commands
                    dup2(filedes2[0],STDIN_FILENO);
                }
                // If we are in a command that is in the middle, we will
                // have to use two pipes, one for input and another for
                // output. The position is also important in order to choose
                // which file descriptor corresponds to each input/output
            }
            else
            {
                if (i % 2 != 0){ // for odd i
                    dup2(filedes2[0],STDIN_FILENO);
                    dup2(filedes[1],STDOUT_FILENO);
                }else{ // for even i
                    dup2(filedes[0],STDIN_FILENO);
                    dup2(filedes2[1],STDOUT_FILENO);
                }
            }
            bool out_red=false,in_red=false;
            string out_name,in_name;
            vector<string> command_args=parse_external(commands[i], out_red, out_name, in_red, in_name);
            char **external_args = new char*[command_args.size() + 1];
            for (size_t j = 0; j < command_args.size(); j++)
            {
                external_args[j] = new char[command_args[j].size() + 1];
                strcpy(external_args[j], command_args[j].c_str());
            }
            external_args[command_args.size()] = NULL;
            FILE *outfile,*infile;
            if(out_red){
                char* s = new char[out_name.size()+1];
                strcpy(s,out_name.c_str());
                outfile = fopen(s, "w+");
                dup2(fileno(outfile), 1);
            }
            if(in_red){
                char* s = new char[in_name.size()+1];
                strcpy(s,in_name.c_str());
                outfile = fopen(s, "r");
                dup2(fileno(infile), 0);
            }
            //sleep(10);     //for test
            if (execve(command_args[0].c_str(), external_args, environ) < 0)
            {
                if (execvp(*external_args, external_args) < 0)
                {
                    cout << "execute error" << endl;
                }
            }
            exit(1);
        }
        // CLOSING DESCRIPTORS ON PARENT
        if (i == 0){
            close(filedes2[1]);
        }
        else if (i == num_cmds - 1){
            if (num_cmds % 2 != 0){
                close(filedes[0]);
            }else{
                close(filedes2[0]);
            }
        }else{
            if (i % 2 != 0){
                close(filedes2[0]);
                close(filedes[1]);
            }else{
                close(filedes[0]);
                close(filedes2[1]);
            }
        }
        p.push_back(pid);
    }
    for(int i = 0; i < num_cmds; i++){
        string msg;
        /* parent process, group child into background or foreground */
        if (!background_process)
        {
            string msg = "";
            my_waitpid(p[i],msg);
            if(msg != "exited status = 0" && msg != "")
            {
                return;
            }
        }
        else
        {
            // if background process, display to terminal the process ID numbers for any processes associated with that command
            cout << "background PID: " << p[i] << endl;
            last_back_pid = p[i] ;
        }
    }
}



int main(int argc, char * argv[])
{
    // background setting
    if(parse_background_setting(argv, argc, substitution_display, debug_level, batch_mode, batch_file))
    {
        return setting_erro;    // invalid user input
    }
    string s = getenv("PWD");
    s += "/sish";
    setenv("SHELL", s.c_str(), 1);      // set shell environ
    setenv("PARENT", s.c_str(), 1);     // set parent environ
    parent_signo_hdlr();   // install parent signal handler
    /* open batch file */
    ifstream file;
    if (batch_mode)
    {
        file.open(batch_file);
        if (!file)
        {
            if (debug_level > 0) {
                perror("cannot open batch file");
            }
            exit(0);
        }
    }
    /* main for loop */
    for(;;)
    {
        string command;
        /* choose input */
        if (batch_mode)
        {
            if (!file.eof())    getline(file, command);
            else
            {
                file.close();
                break;
            }
        }
        else
        {
            cout << "sish >> ";
            getline(cin, command);      // get user command
        }
        if (command == "")  continue;   // ignore blank line
        history.push_back(command);
        vector<string> command_args = parse_command(command);   // push all command arguments into vector
        /* internal command */
        if (built_in(command_args[0]))
        {
            execute_command(command_args);
        }
        /* external command */
        else
        {
            /* substituite variable before external command */
            string cmd_after_sub = find_var_sub(command_args);
            /* if -x on */
            if (substitution_display)
            {
                cout << "cmd after substituition: " << cmd_after_sub << endl;
            }
            vector<string> pipe_commands;
            bool backprocess=false;
            int num=numOfpipe(command, pipe_commands,backprocess);
            if(num == 1)
            {
                /* setup redirection */
                bool out_red = false, in_red = false;
                string out_name, in_name;
                command_args = parse_external(command, out_red, out_name, in_red, in_name);
                /* execute */
                execute_external(command_args,out_red, out_name, in_red, in_name);
            }
            else
            {
                execute_Pipe(pipe_commands, num,backprocess);
            }
        }
    }
    
    return 0;
}
