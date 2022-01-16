#include "node.h"
#include <iostream>

using namespace std;

void printRT(vector<RoutingNode*> nd){
/*Print routing table entries*/
	for (int i = 0; i < nd.size(); i++) {
	  nd[i]->printTable();
	}
}

bool compareRoutingEntry(RoutingEntry oldEntry, RoutingEntry newEntry){
  if((oldEntry.dstip == newEntry.dstip) && (oldEntry.nexthop == newEntry.nexthop) && (oldEntry.ip_interface == newEntry.ip_interface) && (oldEntry.cost == newEntry.cost)){
    return true;
  }
  return false;
}

bool checkConvergence(vector<routingtbl> old, vector<routingtbl> upd){
  if(old.size() != upd.size()){
    return false;
  }
  else{
    for(int i = 0 ; i < old.size(); i++){
      if(old[i].tbl.size() != upd[i].tbl.size()){
        return false;
      }
      else{
        for(int j = 0; j < old[i].tbl.size(); j++){
          if(compareRoutingEntry(old[i].tbl[j], upd[i].tbl[j]) == false){
            return false;
          }
        }
      }
    }
  }
  return true;
}

void routingAlgo(vector<RoutingNode*> nd){
  bool isConverged = false;
  while(isConverged != true){
    vector<routingtbl> prevTables;
    for(int i = 0; i  < nd.size(); i++){
      prevTables.push_back(nd[i]->getTable());
      nd[i]->sendMsg();
    }
    vector<routingtbl> updatedTables;
    for(int i = 0; i < nd.size(); i++){
      updatedTables.push_back(nd[i]->getTable());
    }
    isConverged = checkConvergence(prevTables, updatedTables);
  }
  printRT(nd);
  cout << "---> ROUTING TABLE CONVERGED ONCE, NOW MODIFYING LINK COST" << endl;

  nd[1]->updateTblEntry("10.0.1.3", 16);
  nd[2]->updateTblEntry("10.0.1.23", 16);
  nd[2]->updateTblEntry("10.0.0.21", 16);

  nd[1]->modifyInterface("10.0.1.3");
  nd[2]->modifyInterface("10.0.1.23");

  cout << "---> COSTS UPDATED " << endl;
  printRT(nd);
  isConverged = false;
  int iters = 1;
  int maxLimitReached = false;
  while(isConverged != true){
    cout << endl << " -------------------- Iteration - " << iters << " ---------------------"<< endl;
    vector<routingtbl> prevTables;
    for(int i = 0; i  < nd.size(); i++){
      prevTables.push_back(nd[i]->getTable());
      nd[i]->sendMsg();
    }
    vector<routingtbl> updatedTables;
    for(int i = 0; i < nd.size(); i++){
      updatedTables.push_back(nd[i]->getTable());
    }
    iters++;
    printRT(nd);
    cout<<endl;
    isConverged = checkConvergence(prevTables, updatedTables);
  }

  cout<< endl << " ---> ROUTING TABLES HAVE CONVERGED " << endl << endl;

  cout<<"----------------- FINAL ROUTING TABLE ---------------------" << endl;
  /*Print routing table entries after routing algo converges */
  printRT(nd);


  cout << endl << "PART C : There is a count-to-infinity problem over here (detailed answer written as a comment at end of routing_algo.cpp" << endl;
}

void RoutingNode::recvMsg(RouteMsg *msg) {
  for(int i = 0 ; i < msg->mytbl->tbl.size(); i++){
    int idx = -1;
    if(isMyInterface(msg->mytbl->tbl[i].dstip) == false){
      for(int j = 0; j < mytbl.tbl.size(); j++){
        if(msg->mytbl->tbl[i].dstip == mytbl.tbl[j].dstip){
          idx = j;
          break;
        }
      }
      if(idx != -1){
        int new_cost = msg->mytbl->tbl[i].cost + 1;
        if(new_cost > 16){
          new_cost = 16; //16 is the maximum hop-count limit. so anything greater than 16 is brought down to 16.
        }
        if(new_cost < mytbl.tbl[idx].cost || mytbl.tbl[idx].nexthop == msg->from){
          mytbl.tbl[idx].cost = new_cost;
          mytbl.tbl[idx].nexthop = msg->from;
          mytbl.tbl[idx].ip_interface = msg->recvip;
        }
      }
      else{
        int new_cost = msg->mytbl->tbl[i].cost + 1;
        if(new_cost > 16){
          new_cost = 16;
        }
        RoutingEntry routing_entry;
        routing_entry.ip_interface = msg->recvip;
        routing_entry.nexthop = msg->from;
        routing_entry.dstip = msg->mytbl->tbl[i].dstip;
        routing_entry.cost = new_cost;
        mytbl.tbl.push_back(routing_entry);
      }
    }
  }
}

// Part C: We can clearly see that there is a formation of a routing loop over here due to which there is a count to infinity problem here. Both the nodes A and B are counting to infinity incrementally for the cost to reach C whose interface making a connection with B in actuality is taken to be disconnected(cost 16).