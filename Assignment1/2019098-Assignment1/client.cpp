#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<errno.h>
#include<string.h>
#include<iostream>
#include<netdb.h>
#include<dirent.h>
#include<ctype.h>
#include<vector>
#include<algorithm>
#include<fcntl.h>

using namespace std;

//a structure to store information about the process
struct process{
	string name;
	int PID;
	int userSpaceTime;
	int kernelSpaceTime;
	int totalCPUtime;
};

//comparator to compare two processes based on cpu time; used in sorting
bool compareByCPUTime(process a, process b){
	return a.totalCPUtime > b.totalCPUtime;
}

//checks if character array represents a number, used while checking for process-id named directories in /proc
bool checkNumber(char d_name[]){
	for(int i = 0; d_name[i] != '\0'; i++){
		if(!isdigit(d_name[i])){
			return false;
		}
	}
	return true;
}

//Gives the details of the process(in form of process struct) that has PID equal to that passed as argument
process get_proc_stat(string PID){

	string pathString = "/proc/" + PID + "/stat";
	const char *pid_StoC = pathString.c_str();
	char path[50] = "";
	strcpy(path, pid_StoC);
	int fd = open(path, O_RDONLY);
	if(fd == -1){ 
		//if process folder not found(observed as process change dynamically) just skip 
		//over it(returns a dummy process with PID -1 which is handled accordingly)
		process dummyProc;
		dummyProc.PID = -1;
		return dummyProc;
	}

	char buff[2];
	string proc_stat = "";
	while(read(fd, buff, 1)){
		proc_stat += buff[0];
	}
	close(fd);
	int len = proc_stat.length();
	int itr = 0;
	string str = "";
	vector<string> procDetails(4);
	int col = 1;
	while(itr < len){
		if(proc_stat[itr] == '('){
			itr++;
			while(proc_stat[itr] != ')'){
				str += proc_stat[itr];
				itr++;
			}
			procDetails[1] = str;
			str = "";
			itr += 2;
			col++;
		}
		else{
			if(proc_stat[itr] == ' '){
				if(col == 1){
					procDetails[0] = str;
				}
				else if(col == 14){
					procDetails[2] = str;
				}
				else if(col == 15){
					procDetails[3] = str;
				}
				str = "";
				itr++;
				col++;
			}
			else{
				str = str + proc_stat[itr];
				itr++;
			}
		}
	}
	process proc;
	proc.name = procDetails[1];
	proc.PID = stoi(procDetails[0]);
	proc.userSpaceTime = stoi(procDetails[2]);
	proc.kernelSpaceTime = stoi(procDetails[3]);
	proc.totalCPUtime = stoi(procDetails[2]) + stoi(procDetails[3]);
	return proc;
}

//The function gives the top N CPU consuming processes(at client side, used to find the topmost CPU consuming process)
vector<process> topNprocess(int N){
	vector<process> allProc;
	DIR *procDir = opendir("/proc");
	dirent *entity;

	//iterate over contents of /proc directory to check if a particular directory represents the process
	while(entity = readdir(procDir)){
		if(checkNumber(entity->d_name)){
			string entName = "";
			for(int i = 0 ; (entity->d_name)[i] != '\0'; i++){
				entName += entity->d_name[i];
			}
			process p = get_proc_stat(entName);
			if(p.PID == -1){ //handling that dummy process case
				continue;
			}
			allProc.push_back(p);
		}
	}

	//sorting all process in order of decreasing CPU consumption
	sort(allProc.begin(), allProc.end(), compareByCPUTime);
	//getting the top N CPU consuming processes
	vector<process> topN(allProc.begin(), allProc.begin() + N);

	return topN;

}

int main(int argc, char *argv[]){

	//arguments fine store it & then, start the client
	int PORT = atoi(argv[1]);
	char *hostAddress = argv[2];
	struct hostent *hostServer = gethostbyname(argv[2]);
	char *N_cli = argv[3];
	if(hostServer == NULL){
		perror("Host not found");
		exit(1);
	}

	int clientSocket_fd;
	struct sockaddr_in serverAddress;

	clientSocket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(clientSocket_fd < 0){
		perror("Client : Error setting up socket ");
		exit(1);
	}

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	memcpy(&serverAddress.sin_addr, hostServer->h_addr_list[0], hostServer->h_length);

	int connect_retval = connect(clientSocket_fd, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
	if(connect_retval < 0){
		perror("Client : Error connecting ");
		exit(1);
	}

	int num_rwSocket;

	//receiving ID (this is just for clarity in file naming i.e- unique for each client on client-side)
	int myID;
	char myID_recv[50] = "";
	num_rwSocket = recv(clientSocket_fd, myID_recv, 50, 0);
	if(num_rwSocket < 0){
		perror("Socket Receive Error");
		exit(1);
	}
	myID = atoi(myID_recv);

	//sending the value of 'N' taken as command line input
	num_rwSocket = send(clientSocket_fd, N_cli, strlen(N_cli), 0);
	if(num_rwSocket < 0){
		perror("Socket Send Error");
		exit(1);
	}

	//receiving data from server about the top N processes n server side
	char serverProcessDetails[5096] = "";
	num_rwSocket = recv(clientSocket_fd, serverProcessDetails, 5096, 0);
	if(num_rwSocket < 0){
		perror("Socket Receive Error");
		exit(1);
	}
	printf("::::: Top %s CPU consuming processes on server side ::::\n", N_cli);
	printf("%s", serverProcessDetails);
	
	// writing received data from server to a file on client side
	FILE *fp;
	string strClientFileName = "clientFile_" + to_string(myID) + ".txt";
	const char *charFileName = strClientFileName.c_str();
	fp = fopen (charFileName, "w+");
	if(fp == NULL){
		perror("File open error");
		exit(1);
	}
	int fileWrite_retval = fprintf(fp, "%s", serverProcessDetails);
	if(fileWrite_retval < 0){
		perror("Error writing to file .");
		exit(1);
	}
	fclose(fp);


	//Getting the details of the top process on client side
	vector<process> topProcess = topNprocess(1);
	string processDetails = "PID: " + to_string(topProcess[0].PID) + " |Name: " + topProcess[0].name + " |User : " + to_string(topProcess[0].userSpaceTime) + " |Kernel: " + to_string(topProcess[0].kernelSpaceTime) + " |Total : " + to_string(topProcess[0].totalCPUtime);
	const char *tempChar = processDetails.c_str();
	char topProcessDetails[1024] = "";
	strcpy(topProcessDetails, tempChar);
	
	//sending the same to the server
	num_rwSocket = send(clientSocket_fd, topProcessDetails, 1024, 0);
	if(num_rwSocket < 0){
		perror("Socket Send Error");
		exit(1);
	}

	sleep(40);
	close(clientSocket_fd);
	return 0;
}