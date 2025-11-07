#include<iostream>
#include<string>
#include<sstream>
#include<vector>
// #include<unistd.h>
// #include<sys/types.h>
#include <process.h>
#include <errno.h>
#include<map>
#include<windows.h>
#include <filesystem>


using namespace std;
namespace fs = std::filesystem;

// To split words into tokens
vector<string> parseInput(const string &input){
    vector<string> tokens;
    stringstream ss(input);
    string token;

    while (ss >> token){
        tokens.push_back(token);
    }
    return tokens;
}

// Structure for managing background jobs
struct Job{
    int id;
    int pid;
    string command;
    bool running;
};

// Job tracking map and counter
map<int,Job> jobs;
int jobCounter = 1;

// Function to check if command should run in background (&)
bool isBackgroundCommand(string &input){
    if(!input.empty() && input.back()=='&'){
        input.pop_back();
        return true;
    }
    return false;
}

// Function that executes the user command
void executeCommand(vector<string>&tokens,bool background){
    if(tokens.empty()) return;

    // Exit command handling
    if(tokens[0] == "exit"){
        cout<<"Exiting shell..."<<endl;
        exit(0);
    }

    // Show list of background jobs
    if(tokens[0]=="jobs"){
        for(auto &[id,job]:jobs){
            cout<<"["<<id<<"] "
                <<(job.running ? "Running " : "Stopped ")
                <<job.command<<" (PID: "<<job.pid<<")\n";
        }
        return;
    }

    // Bring a background job to foreground
    if (tokens[0]=="fg"){
        if(tokens.size()<2){
            cout<<"Usage: fg <job_id>"<<endl; // 🔹 added missing ">"
            return;
        }
        int jobID=stoi(tokens[1]);
        if (jobs.find(jobID) != jobs.end()) {
            Job &job = jobs[jobID];
            cout << "Bringing job " << jobID << " (" << job.command << ") to foreground...\n";
            WaitForSingleObject(OpenProcess(SYNCHRONIZE, FALSE, job.pid), INFINITE);
            job.running = false;
        } else {
            cout << "No such job ID.\n";
        }
        return;
    }

    // 🔹 Kill a background or foreground job
    if (tokens[0] == "kill") {
    if (tokens.size() < 2) {
        cout << "Usage: kill <job_id>\n";
        return;
    }
    int jobID = stoi(tokens[1]);
    if (jobs.find(jobID) != jobs.end()) {
        Job &job = jobs[jobID];
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, job.pid);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            job.running = false;
            cout << "Job [" << jobID << "] (" << job.command << ") terminated.\n";
        } else {
            cout << "Failed to terminate via API. Trying taskkill...\n";
            string cmd = "taskkill /PID " + to_string(job.pid) + " /F";
            system(cmd.c_str());
        }
    } else {
        cout << "No such job ID.\n";
    }
    return;
}
    // for converting vector<string> to char* array for _spawnvp()
    vector<char*> cmdArgs;
    for (auto &arg : tokens) cmdArgs.push_back(&arg[0]);
    cmdArgs.push_back(NULL);

    //Handle Windows internal commands
    vector<string> internalCmds = {
    // File & Directory Management
    "dir", "cd", "chdir", "md", "mkdir", "rd", "rmdir",
    "copy", "move", "del", "erase", "ren", "rename", "type",
    "attrib", "tree", "xcopy", "robocopy",

    // Display / Screen / Prompt
    "cls", "color", "title", "echo", "prompt",

    // System Information
    "ver", "vol", "path", "assoc", "ftype",

    // Time & Date
    "date", "time",

    // Batch / Control Commands
    "call", "if", "for", "goto", "pause", "set", "setlocal", "endlocal", "shift",

    // System Utilities
    "tasklist", "taskkill", "shutdown", "help",

    // Misc
    "cmd", "exit"
};

    bool isInternal = false;
    for (auto &cmd : internalCmds)
        if (tokens[0] == cmd) { isInternal = true; break; }

    if (isInternal) {
        vector<char*> cmdArgs2 = {(char*)"cmd", (char*)"/c"};
        for (auto &arg : tokens)
            cmdArgs2.push_back(&arg[0]);
        cmdArgs2.push_back(NULL);

        if (background) { // 🔹 Run internal command in background
            int pid = _spawnvp(_P_NOWAIT, "cmd", cmdArgs2.data());
            if (pid != -1) {
                jobs[jobCounter] = {jobCounter, pid, tokens[0], true};
                cout << "[" << jobCounter++ << "] Running in background (PID: " << pid << ")\n";
            } else perror("Background command failed");
        } else { // 🔹 Run internal command in foreground
            _spawnvp(_P_WAIT, "cmd", cmdArgs2.data());
        }
        return;
    }

    // To Handle external commands (e.g., notepad, calc, etc.)
    if (background) {
        int pid = _spawnvp(_P_NOWAIT, cmdArgs[0], cmdArgs.data());
        if (pid != -1) {
            jobs[jobCounter] = {jobCounter, pid, tokens[0], true};
            cout << "[" << jobCounter++ << "] Running in background (PID: " << pid << ")\n";
        } else perror("Background command failed");
    } else {
        int result = _spawnvp(_P_WAIT, cmdArgs[0], cmdArgs.data());
        if (result == -1)
            perror("Command execution failed");
    }
}


int main(){
    string input;
    while (true)
    {
        cout<<"cshell>";    //shell prompt
        if(!getline(cin,input)) break; //get inputline

        if(cin.eof() || input=="exit" || input=="EXIT" || input=="Exit"){ //code to check for quitting shell
            cout<<"Exiting shell..."<<endl;
            break;
        }

        // Detect if & is present at end of command
        bool background = isBackgroundCommand(input);

        vector<string> tokens = parseInput(input);   //parsing input to tokens

        if(tokens.empty())
            continue;

        // Execute command using our function
        executeCommand(tokens, background);

        /*
        For Showing Tokens
        cout<<"Parsed tokens: ";    
        for (const string &t : tokens){     //showing parsed tokens 
            cout<<"["<<t<<"]";
        }
        cout<<endl;*/
    }
    return 0;
}
