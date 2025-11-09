# Custom_Shell_Implementation
 A simple shell  in  C++ that can execute  commands, manage processes,  and handle redirection and piping.
---

## Example Commands

Here are some example commands supported by the custom shell:

| Command | Description |
|----------|-------------|
| `dir` | Lists the current directory contents |
| `type file.txt` | Displays the contents of a file |
| `echo hello > output.txt` | Redirects output to a file |
| `sort < input.txt | find "a" > result.txt` | Sorts and filters file content using pipes |
| `notepad &` | Launches Notepad in background |
| `jobs` | Displays running background processes |
| `fg 1` | Brings job 1 to foreground |
| `kill 2` | Terminates job 2 |

---

## How It Works

1. The shell continuously reads user input.
2. It parses the command into tokens, detecting operators like `|`, `<`, `>`, and `&`.
3. Depending on the operators, it executes processes either in the **foreground** or **background**.
4. Commands are executed via **Windows API (CreateProcessA)** or **system()**.
5. The shell maintains a process table for background jobs.
6. It allows foreground switching (`fg`) and controlled termination (`kill`).

---

## Screenshots

Screenshots of the shell’s operation can be found in the `/screenshots` folder:
1. Compilation and Directory Listing  
2. Output Redirection to File  
3. Viewing Redirected File Contents  
4. Running Background Processes  
5. Foreground and Background Job Control  
6. Command Filtering and Sorting  
7. Multi-Command Chaining  
8. Job Listing and Termination  
9. Advanced Pipeline Demonstration  
10. Final Integrated Test  

---

## Requirements

- **Language:** C++  
- **Compiler:** MinGW / g++  
- **Operating System:** Windows 10 or higher  
- **IDE (Optional):** Visual Studio Code  

---

## Compilation and Execution

To compile and run the shell manually:

```bash
g++ shell.cpp -o sh.exe
sh.exe
