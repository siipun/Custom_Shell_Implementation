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

    // Kill a background or foreground job
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

    // Output redirection detection > and >>
    string outFile = "";
    bool appendMode = false;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == ">" || tokens[i] == ">>") {
            if (i + 1 < tokens.size()) {
                outFile = tokens[i + 1];
                appendMode = (tokens[i] == ">>");
                tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
            } else {
                cerr << "Syntax error near '>'\n";
                return;
            }
            break;
        }
    }

    // Input redirection detection <
    string inFile = "";
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "<") {
            if (i + 1 < tokens.size()) {
                inFile = tokens[i + 1];
                tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
            } else {
                cerr << "Syntax error near '<'\n";
                return;
            }
            break;
        }
    }

    FILE* saved_stdout = nullptr;
    FILE* saved_stdin = nullptr;

    // redirect stdout if output redirection is specified
    if (!outFile.empty()) {
        saved_stdout = stdout;
        freopen(outFile.c_str(), appendMode ? "a" : "w", stdout);
    }

    // redirect stdin if input redirection is specified
    if (!inFile.empty()) {
        saved_stdin = stdin;
        FILE* f = freopen(inFile.c_str(), "r", stdin);
        if (!f) {
            cerr << "Error opening input file: " << inFile << endl;
            if (saved_stdout) {
                fflush(stdout);
                freopen("CONOUT$", "w", stdout);
            }
            return;
        }
    }

    // for converting vector<string> to char* array for _spawnvp()
    vector<char*> cmdArgs;
    for (auto &arg : tokens) cmdArgs.push_back(&arg[0]);
    cmdArgs.push_back(NULL);

    //Handle Windows internal commands
    vector<string> internalCmds = {
        // File & Directory Management
        "dir", "chdir", "md", "mkdir", "rd", "rmdir",
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

    // Handle 'cd' command internally 
    if (tokens[0] == "cd" || tokens[0] == "chdir") {
        if (tokens.size() < 2) {
            cout << fs::current_path() << endl; // Show current directory if no argument
        } else {
            try {
                fs::current_path(tokens[1]);
            } catch (const std::exception &e) {
                cerr << "The system cannot find the path specified: " << tokens[1] << endl;
            }
        }
        if (saved_stdout) {
            fflush(stdout);
            freopen("CONOUT$", "w", stdout);
        }
        if (saved_stdin) {
            fflush(stdin);
            freopen("CONIN$", "r", stdin);
        }
        return;
    }

    bool isInternal = false;
    for (auto &cmd : internalCmds)
        if (tokens[0] == cmd) { isInternal = true; break; }

    if (isInternal) {
        vector<char*> cmdArgs2 = {(char*)"cmd", (char*)"/c"};
        for (auto &arg : tokens)
            cmdArgs2.push_back(&arg[0]);
        cmdArgs2.push_back(NULL);

        if (background) { // Run internal command in background
            int pid = _spawnvp(_P_NOWAIT, "cmd", cmdArgs2.data());
            if (pid != -1) {
                jobs[jobCounter] = {jobCounter, pid, tokens[0], true};
                cout << "[" << jobCounter++ << "] Running in background (PID: " << pid << ")\n";
            } else perror("Background command failed");
        } else { // Run internal command in foreground
            _spawnvp(_P_WAIT, "cmd", cmdArgs2.data());
        }
        if (saved_stdout) {
            fflush(stdout);
            freopen("CONOUT$", "w", stdout);
        }
        if (saved_stdin) {
            fflush(stdin);
            freopen("CONIN$", "r", stdin);
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

    // Restore stdout and stdin if redirected
    if (saved_stdout) {
        fflush(stdout);
        freopen("CONOUT$", "w", stdout);
    }
    if (saved_stdin) {
        fflush(stdin);
        freopen("CONIN$", "r", stdin);
    }
}


int main(){
    string input;
    while (true)
    {
        cout << "cshell(" << fs::current_path().string() << ")>";
            //shell prompt
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
