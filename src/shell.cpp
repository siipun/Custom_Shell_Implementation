#include<iostream> // input/output
#include<string> // string operations
#include<sstream> // string manipulation
#include<vector> // for lists and token storage
#include <process.h>//process creation and mnagement
#include <errno.h>//error code for system-level function
#include<map> // for value pair data structre
#include<windows.h>//for main windows API header 
#include <filesystem>//tools for dictionaries and files
#include <fstream> // for piping through temporary file handling

using namespace std;
namespace fs = std::filesystem;

// Split input line into tokens
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

// Detect if command should run in background (&)
bool isBackgroundCommand(string &input){
    if(!input.empty() && input.back()=='&'){
        input.pop_back();
        return true;
    }
    return false;
}

// helper structs for window search by PID
struct EnumData { DWORD pid; HWND hwnd; };

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    EnumData* data = (EnumData*)lParam;
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid == data->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL) {
        data->hwnd = hwnd;
        return FALSE; 
    }
    return TRUE; 
}

HWND FindMainWindowByPID(DWORD pid) {
    EnumData data;
    data.pid = pid;
    data.hwnd = NULL;
    EnumWindows(EnumWindowsCallback, (LPARAM)&data);
    return data.hwnd;
}

// Executes a single command (used in piping)
void executeSingleCommand(vector<string> tokens, string inputFile = "", string outputFile = "", bool append = false){
    FILE* saved_stdin = nullptr;
    FILE* saved_stdout = nullptr;

    if (!inputFile.empty()) {
        saved_stdin = stdin;
        FILE* f = freopen(inputFile.c_str(), "r", stdin);
        if (!f) {
            cerr << "Error opening input file: " << inputFile << endl;
            return;
        }
    }

    if (!outputFile.empty()) {
        saved_stdout = stdout;
        freopen(outputFile.c_str(), append ? "a" : "w", stdout);
    }

    vector<char*> cmdArgs;
    for (auto &arg : tokens) cmdArgs.push_back(&arg[0]);
    cmdArgs.push_back(NULL);

    _spawnvp(_P_WAIT, cmdArgs[0], cmdArgs.data());

    if (saved_stdout) {
        fflush(stdout);
        freopen("CONOUT$", "w", stdout);
    }
    if (saved_stdin) {
        fflush(stdin);
        freopen("CONIN$", "r", stdin);
    }
}

// Execute full user command
void executeCommand(vector<string>&tokens,bool background){
    if(tokens.empty()) return;

    // Detect "bg" keyword to force background mode
    bool forceBackground = false;
    if (tokens[0] == "bg") {
        forceBackground = true;
        tokens.erase(tokens.begin());
    }

    if (forceBackground) background = true;

    // auto delegate to cmd.exe if command contains |, >, or <
    string rawCommand;
    for (const auto &t : tokens) rawCommand += t + " ";

    // for detecting redirections and pipelines
    if (rawCommand.find("|") != string::npos || 
        rawCommand.find(">") != string::npos || 
        rawCommand.find("<") != string::npos) {

        // Use cmd.exe to execute combined redirection/pipeline commands
        string cmdLine = "/c " + rawCommand;

        if (background) {
            int pid = _spawnlp(_P_NOWAIT, "cmd", "cmd", cmdLine.c_str(), NULL);
            if (pid != -1) {
                jobs[jobCounter] = {jobCounter, pid, rawCommand, true};
                cout << "[" << jobCounter++ << "] Running pipeline/redirection in background (PID: " << pid << ")\n";
            } else perror("Background pipeline failed");
        } else {
            int ret = system(("cmd " + cmdLine).c_str());
            if (ret != 0)
                cerr << "Command failed or syntax incorrect.\n";
        }
        return;
    }

    // Exit shell
    if(tokens[0] == "exit"){
        cout<<"Exiting shell..."<<endl;
        exit(0);
    }

    // Display background jobs
    if(tokens[0]=="jobs"){
        for(auto &[id,job]:jobs){
            cout<<"["<<id<<"] "
                <<(job.running ? "Running " : "Stopped ")
                <<job.command<<" (PID: "<<job.pid<<")\n";
        }
        return;
    }

    // Bring background job to foreground
    if (tokens[0] == "fg") {
        if (tokens.size() < 2) {
            cout << "Usage: fg <job_id>\n";
            return;
        }

        int jobID = stoi(tokens[1]);
        if (jobs.find(jobID) != jobs.end()) {
            Job &job = jobs[jobID];

            HWND hwnd = FindMainWindowByPID((DWORD)job.pid);
            if (hwnd) {
                if (SetForegroundWindow(hwnd)) {
                    cout << "Brought job " << jobID << " (" << job.command << ") window to front.\n";
                } else {
                    cout << "Found window but SetForegroundWindow failed (may be blocked by OS policy).\n";
                }
            } else {
                cout << "Could not find window for " << job.command << ". Waiting for it to finish instead...\n";
                HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, job.pid);
                if (hProc) {
                    WaitForSingleObject(hProc, INFINITE);
                    CloseHandle(hProc);
                    job.running = false;
                    cout << "Job [" << jobID << "] has finished.\n";
                } else {
                    cout << "Unable to open process. It may have already exited.\n";
                }
            }
        } else {
            cout << "No such job ID.\n";
        }
        return;
    }

    // Kill job
    if (tokens[0] == "kill") {
        if (tokens.size() < 2) {
            cout << "Usage: kill <job_id>\n";
            return;
        }

        int jobID = stoi(tokens[1]);
        if (jobs.find(jobID) != jobs.end()) {
            Job &job = jobs[jobID];
            cout << "Attempting to terminate Job [" << jobID << "] (" << job.command << ")...\n";

            bool killed = false;

            // Try PID-based termination first
            string cmd = "taskkill /PID " + to_string(job.pid) + " /F >nul 2>&1";
            int result = system(cmd.c_str());
            if (result == 0) {
                cout << "Job [" << jobID << "] terminated successfully.\n";
                job.running = false;
                killed = true;
            }

            // If that fails, try killing by image name (e.g., notepad.exe)
            if (!killed) {
                string exeName = job.command + ".exe";
                string cmd2 = "taskkill /IM " + exeName + " /F >nul 2>&1";
                int result2 = system(cmd2.c_str());
                if (result2 == 0) {
                    cout << "Job [" << jobID << "] (" << exeName << ") terminated using /IM.\n";
                    job.running = false;
                    killed = true;
                }
            }

            // If that still fails, handle special UWP apps (like Calculator)
            if (!killed && (job.command == "calc" || job.command == "calculator")) {
                string uwpKillCmd = "powershell -Command \"Get-Process CalculatorApp -ErrorAction SilentlyContinue | Stop-Process -Force\"";
                int uwpResult = system(uwpKillCmd.c_str());
                if (uwpResult == 0) {
                    cout << "UWP app (Calculator) terminated successfully.\n";
                    job.running = false;
                    killed = true;
                }
            }

            if (!killed) {
                cout << "Failed to terminate Job [" << jobID << "]. Process may have already exited or is protected.\n";
            }

        } else {
            cout << "No such job ID.\n";
        }
        return;
    }

    // Handle internal commands
    vector<char*> cmdArgs;
    for (auto &arg : tokens) cmdArgs.push_back(&arg[0]);
    cmdArgs.push_back(NULL);

    vector<string> internalCmds = {
        "dir", "chdir", "md", "mkdir", "rd", "rmdir",
        "copy", "move", "del", "erase", "ren", "rename", "type",
        "attrib", "tree", "xcopy", "robocopy",
        "cls", "color", "title", "echo", "prompt",
        "ver", "vol", "path", "assoc", "ftype",
        "date", "time",
        "call", "if", "for", "goto", "pause", "set", "setlocal", "endlocal", "shift",
        "tasklist", "taskkill", "shutdown", "help",
        "cmd", "exit"
    };

    // Handle cd internally
    if (tokens[0] == "cd" || tokens[0] == "chdir") {
        if (tokens.size() < 2) {
            cout << fs::current_path() << endl;
        } else {
            try {
                fs::current_path(tokens[1]);
            } catch (const std::exception &e) {
                cerr << "The system cannot find the path specified: " << tokens[1] << endl;
            }
        }
        return;
    }

    bool isInternal = false;
    for (auto &cmd : internalCmds)
        if (tokens[0] == cmd) { isInternal = true; break; }

    // Execute internal command through CMD
    if (isInternal) {
        vector<char*> cmdArgs2 = {(char*)"cmd", (char*)"/c"};
        for (auto &arg : tokens)
            cmdArgs2.push_back(&arg[0]);
        cmdArgs2.push_back(NULL);

        if (background) {
            int pid = _spawnvp(_P_NOWAIT, "cmd", cmdArgs2.data());
            if (pid != -1) {
                jobs[jobCounter] = {jobCounter, pid, tokens[0], true};
                cout << "[" << jobCounter++ << "] Running in background (PID: " << pid << ")\n";
            } else perror("Background command failed");
        } else {
            _spawnvp(_P_WAIT, "cmd", cmdArgs2.data());
        }
        return;
    }

    // Handle external commands (e.g., notepad, calc)
    if (background) {
        STARTUPINFO si = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION pi;

        string fullCommand;
        for (auto &t : tokens) fullCommand += t + " ";

        if (CreateProcessA(
                NULL,
                (LPSTR)fullCommand.c_str(),
                NULL, NULL, FALSE,
                CREATE_NEW_CONSOLE,
                NULL, NULL,
                &si, &pi))
        {
            int pid = (int)pi.dwProcessId;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            jobs[jobCounter] = { jobCounter, pid, tokens[0], true };
            cout << "[" << jobCounter++ << "] Running in background (PID: " << pid << ")\n";
        } else {
            perror("Background command failed");
        }
    } else {
        int result = _spawnvp(_P_WAIT, cmdArgs[0], cmdArgs.data());
        if (result == -1)
            perror("Command execution failed");
    }
}

//main function 
int main(){
    string input;
    while (true)
    {
        cout << "cshell(" << fs::current_path().string() << ")>";
        if(!getline(cin,input)) break;

        if(cin.eof() || input=="exit" || input=="EXIT" || input=="Exit"){
            cout<<"Exiting shell..."<<endl;
            break;
        }

        bool background = isBackgroundCommand(input);
        vector<string> tokens = parseInput(input);

        if(tokens.empty())
            continue;

        executeCommand(tokens, background);

    }
    return 0;
}
