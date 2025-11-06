#include<iostream>
#include<string>
#include<sstream>
#include<vector>
#include<unistd.h>
#include<sys/types.h>
#include <process.h>
#include <errno.h>

using namespace std;

//split words into tokens
vector<string> parseInput(const string &input){
    vector<string> tokens;
    stringstream ss(input);
    string token;

    while (ss >> token){
        tokens.push_back(token);
    }
    return tokens;
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
        vector<string> tokens =parseInput(input);   //parsing input to tokens

        if(tokens.empty())
            continue;
        

        //convert tokens to c-style array for executimug
        vector<char*> cmdArgs;
        for(auto &arg:tokens){
            cmdArgs.push_back(&arg[0]);
        }
        cmdArgs.push_back(NULL);


// Special case for internal Windows commands like dir, cd, cls
if(tokens[0] == "dir" || tokens[0] == "cls"){
    vector<char*> cmdArgs2 = { (char*)"cmd", (char*)"/c", &tokens[0][0], NULL };
    _spawnvp(_P_WAIT, "cmd", cmdArgs2.data());
    continue;
}

// Normal external commands
int result = _spawnvp(_P_WAIT, cmdArgs[0], cmdArgs.data());
if(result == -1) {
    perror("Command execution failed!");
}
        if(result==-1){
            perror("Command execution failed!");
        }

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


