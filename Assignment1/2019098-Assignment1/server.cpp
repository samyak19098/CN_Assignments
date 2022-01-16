#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<errno.h>
#include<string.h>
#include<iostream>
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

	//iterate over contents of /proc directory to check if a particular directory represent
	while(entity = readdir(procDir)){

		if(checkNumber(entity->d_name)){
			string entName = "";
			for(int i = 0 ; (entity->d_name)[i] != '\0'; i++){
				entName += entity->d_name[i];
			}
			process p = get_proc_stat(entName);
			if(p.PID == -1){
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

//This struct is used to hold the socket-related information about client along with its ID
struct client_module{
	int clientSocket_fd;
	struct sockaddr_in client_address;
	int ID;
};

//The routine passed to and used by pthread_create to create a new thread
void * socketConnectionManager(void * argStruct){

	int num_rwSocket;
	client_module *clientPacket;
	clientPacket = (client_module *)argStruct;

	//assigning client a unique ID
	int client_ID = clientPacket->ID;
	cout<<"Client number : " << client_ID << endl;
	string strClient_ID = to_string(client_ID);
	const char *buffClient_ID = strClient_ID.c_str();
	//send it to client-side(this part is just used to create different files for each client on a client side)
	num_rwSocket = send(clientPacket->clientSocket_fd, buffClient_ID, strlen(buffClient_ID), 0);

	//Receiving value of N from client
	char N_recv[50] = "";
	int N;
	num_rwSocket = recv(clientPacket->clientSocket_fd, N_recv, 50, 0);
	if(num_rwSocket < 0){
		perror("Socket Receive Error");
		exit(1);
	}
	N = atoi(N_recv);

	// finding top N processes and storing it in a file
	FILE *fp;
	vector<process> serverProcesses = topNprocess(N);
	string strFileName = "serverFile_" + to_string(client_ID) + ".txt";
	const char *charFileName = strFileName.c_str();
	fp = fopen(charFileName, "w+");
	if(fp == NULL){
		perror("File Opening Error ");
		exit(1);
	}
	for(int i = 0; i < N ; i++){
		string processDetails = "PID: " + to_string(serverProcesses[i].PID) + " |Name: " + serverProcesses[i].name + " |User : " + to_string(serverProcesses[i].userSpaceTime) + " |Kernel: " + to_string(serverProcesses[i].kernelSpaceTime) + " |Total : " + to_string(serverProcesses[i].totalCPUtime);
		const char *formattedProcessDetails = processDetails.c_str();
		int fileWrite_retval;
		fileWrite_retval = fprintf(fp, "%s\n", formattedProcessDetails);
		if(fileWrite_retval < 0){
			perror("Error writing to file .");
			exit(1);
		}
	}
	fclose(fp);

	//reading from the file created and sending to client.
	FILE *fp1;
	fp1 = fopen(charFileName, "r");
	if(fp1 == NULL){
		perror("File Opening Error ");
		exit(1);
	}
	char fileReadBuffer[5096] = "";
	int numRead = fread(&fileReadBuffer, sizeof(char), 5096, fp1);
	fclose(fp1);

	num_rwSocket = send(clientPacket->clientSocket_fd, fileReadBuffer, strlen(fileReadBuffer), 0);
	if(num_rwSocket < 0){
		perror("Socket Send Error");
		exit(1);
	}

	//receiving information about the client's top CPU consuming process
	char clientTopProcess[1024] = "";
	num_rwSocket = recv(clientPacket->clientSocket_fd, clientTopProcess, 1024, 0);
	printf("Client's Top Process :\n %s \n\n", clientTopProcess);

	sleep(40);

	close(clientPacket->clientSocket_fd);
	pthread_exit(0);
}
int main(int argc, char *argv[]){


	int PORT = atoi(argv[1]); // reading the port input on server side
	int serverSocket_fd;
	struct sockaddr_in serverAddress;

	serverSocket_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if(serverSocket_fd < 0){
		perror("Server : Error in creating socket() ");
		exit(1);
	}
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	int bind_retval = bind(serverSocket_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if(bind_retval < 0){
		perror("Server : Error binding !");
		exit(1);
	}

	int backlog = 10;
	int listen_retval = listen(serverSocket_fd, backlog);
	if(listen_retval < 0){
		perror("Server : Error listening !");
		exit(1);
	}

	pthread_t clientConnection;
	client_module *clientPacket;
	int client_num = 0; //used to allocate ID to the clients
	while(1){

		// pthread_t clientConnection;
		clientPacket = new client_module();
		client_num++;
		clientPacket->ID = client_num;
		int len_client_address = sizeof(clientPacket->client_address);
		clientPacket->clientSocket_fd = accept(serverSocket_fd, (struct sockaddr *)&(clientPacket->client_address), (socklen_t *)&len_client_address);

		if(clientPacket->clientSocket_fd < 0){
			perror("Server : Failure connecting to client");
			exit(1);
		}
		else{
			pthread_create(&clientConnection, 0, socketConnectionManager, (void *)clientPacket); // create a new thread for the new client
			pthread_detach(clientConnection); // we detach the thread so that whenever it whenever socket closes and it terminates, its resources are released 
		}
	}

	return 0;
}