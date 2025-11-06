#include<iostream>
#include<string>
#include<sstream>
#include<vector>

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
        getline(cin,input); //get inputline

        if(cin.eof() || input=="exit"){ //code to check for quitting shell
            break;
        }
        vector<string> tokens =parseInput(input);   //parsing input to tokens

        if(tokens.empty())
            continue;
        
        cout<<"Parsed tokens: ";    
        for (const string &t : tokens){     //showing parsed tokens 
            cout<<"["<<t<<"]";
        }
        cout<<endl;
    }
    cout<<"Existing shell..."<<endl;
    return 0;
    
}


