# Computer_Networks_Homework_1

A server/client application. 
The client is able to send the following commands to the server:

      - "login : username" - Checks if the username exists in the configuration file. If it does, the user enters the server and may execute other commands.  
      - "get-logged-users" - Prints data about current logged users (username, hostname for remote login, time entry was made). Executes only if the client is logged in.
      - "get-proc-info : pid" - Prints data about the process identified with "pid" (name, state, ppid, uid, vmsize). Executes only if the client is logged in.
      - "logout"
      - "quit"
	

I used socketpairs, pipes and fifos for communication.
