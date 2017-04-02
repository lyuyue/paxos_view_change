## The Leader Election (View Change) from Paxos Protocol: CS7680 Programming Assignment 3
### Yue Lyu

### Instruction
Just `make` and run the binary with folloing command:<br />
`./server -p port -h hostfile [-t time]`<br />

`-p port`            : port number    
`-h hostfile`        : path to file with hosts  
`-t time`            : initial progress_timer_threshold, optional, threshold set to be 5 seconds by default

### Test Cases
- Test is run on 5 machines, 0-indexed, with initial view_id 0.
- Server 0 killed, after 5 seconds, remaining 4 server shift to new view (view_id = 1)
- Server 0 and 1 killed, remaining 3 server first shift to view 1, since server 1 is down, no progress will be made, finally the remaining servers reach view 2.
- Server 0, 1 and 2 killed, no majority exists, endless shift_to_leader_election()
- Crashed server comes back when new view is installed, it will jump to the newest view.
- Crashed server comes back and a majority is reached, servers will soon convert to a new view. 

This implementation passes all cases listed above. :)

### Code Architecture
**README.md**       
**Makefile**        
**constants.h**     : header file with constants definition
**message.h**       : header file with Message struct definition  
**main.c**          : main program  

### State Diagram
![Alt](/diagram/Leader_Election.png)

### Design Decisions
- server_port argument is discarded since no actual client in this implementation.
- The low-level communication protocol is UDP in this project, for better scalability.
- All server is started with assumption that initail view_id = 0 and server 0 is the initial leader
- The implementation is built with assumption that no malicious server exists in the system, they would respond to message received correctly and immediately.
- Progress_timer, theoritically, should be reset by update message from server, since we don't have actual updates here, I consider a VC_Proof from view leader as signal to reset progress_timer.
- Prepare phase is implemented to ensure the system won't crash if VC_Proof message lost.
