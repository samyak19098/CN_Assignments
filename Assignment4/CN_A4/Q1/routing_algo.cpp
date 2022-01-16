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
  //Your code here

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
  /*Print routing table entries after routing algo converges */
  printRT(nd);
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
        if(new_cost < mytbl.tbl[idx].cost){
          mytbl.tbl[idx].cost = new_cost;
          mytbl.tbl[idx].nexthop = msg->from;
        }
      }
      else{
        RoutingEntry routing_entry;
        routing_entry.ip_interface = msg->recvip;
        routing_entry.nexthop = msg->from;
        routing_entry.dstip = msg->mytbl->tbl[i].dstip;
        routing_entry.cost = msg->mytbl->tbl[i].cost + 1;
        mytbl.tbl.push_back(routing_entry);
      }
    }
  }
}