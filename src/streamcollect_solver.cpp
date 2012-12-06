/*
 * Copyright (c) 2012 Bernhard Firner and Rutgers University
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * or visit http://www.gnu.org/licenses/gpl-2.0.html
 */

/*******************************************************************************
 * @file streamcollect_solver.cpp
 * Prints out packet information in a format that people have been using for
 * other projects.
 *
 * @author Bernhard Firner
 ******************************************************************************/


#include <owl/netbuffer.hpp>
#include <owl/solver_aggregator_connection.hpp>
#include <owl/sample_data.hpp>
#include <owl/grail_types.hpp>
#include <owl/world_model_protocol.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

using world_model::grail_time;
using namespace aggregator_solver;
using namespace std;

set<uint128_t> tx_ids;
set<uint128_t> rx_ids;

//Default to decimal but allow the user to specify hexedecimal output for the
//transmitter and receiver ids
bool use_hex = false;

// check the tx_id and rx_id of the sample data is valid 
bool data_packet(uint128_t tx_id, uint128_t rx_id) {
  bool tx_find = (0 < tx_ids.count(tx_id)) or tx_ids.empty();
  bool rx_find = (0 < rx_ids.count(rx_id)) or rx_ids.empty();
	return (tx_find and rx_find);
}

//This function is not thread safe (but could be made so with a mutex)
void packetCallback(SampleData& sample) {
	// Read the packets from all the links as a RSSI vector; drop the whole vector if any element misses
	if (data_packet(sample.tx_id, sample.rx_id)) {
    if (use_hex) {
      cout<<std::hex<<sample.rx_id<<"\t"<<world_model::getGRAILTime()<<'\t'<<std::hex<<sample.tx_id<<std::dec;
      //cout<<std::hex<<sample.rx_id<<"\t"<<std::dec<<sample.rx_timestamp<<'\t'<<std::hex<<sample.tx_id<<std::dec;
    }
    else {
      cout<<std::dec<<sample.rx_id<<"\t"<<sample.rx_timestamp<<'\t'<<sample.tx_id;
    }
    cout<<"\t0\t"<<sample.rss<<"\t0x00\tExtra:"<<sample.sense_data.size();
    for (auto I = sample.sense_data.begin(); I != sample.sense_data.end(); ++I) {
      cout<<'\t'<<(uint32_t)(*I);
    }
		cout<<endl;
/*
    cout<<"\tDropped:0"<<endl;
*/
	
	}
  //else {
    //cout<<"Got bad sample from "<<sample.rx_id<<" transmitter "<<sample.tx_id<<'\n';
  //}
	return;
}

int main(int argc, char** cargv) {
  std::vector<char*> argv(cargv, cargv+argc);

  //Default to all phy layers.
  uint8_t use_phy = 0;

  //Check if the user wants output in hex
  auto hex_cmd = std::find_if(argv.begin(), argv.end(), [&](char* arg) {
      return std::string(arg) == "--hex"; });
  if (hex_cmd != argv.end()) {
    use_hex = true;
    argv.erase(hex_cmd);
  }

  auto phy_cmd = std::find_if(argv.begin(), argv.end(), [&](char* arg) {
      return std::string(arg) == "--phy"; });
  if (phy_cmd != argv.end()) {
    auto phy_arg = phy_cmd+1;
    if (phy_arg == argv.end()) {
      std::cerr<<"Error: The '--phy' argument expects the physical layer number to request packets from.\n";
      return 0;
    }
    else {
      use_phy = std::stoi(std::string(*phy_arg));
      std::cout<<"Using physical layer "<<(uint32_t)use_phy<<'\n';
    }
    argv.erase(phy_arg);
    argv.erase(std::find_if(argv.begin(), argv.end(), [&](char* arg) {
      return std::string(arg) == "--phy"; }));
  }

	if (argv.size() < 4) {
    cerr<<"This program needs 3 or more arguments:\n";
    cerr<<"\tclient <config filename> [<aggregator ip> <aggregator port>]+\n";
    cerr<<"The first line of the config file lists transmitters and the second lists receivers.\n";
    cerr<<"Any number of aggregator ip/port pairs may be provided to connect to multiple aggregators.\n";
    return 0;
  }

	//Grab the ip and ports for the aggregators and distributor.
	vector<SolverAggregator::NetTarget> aggregators;

	for (int s_num = 2; s_num < argv.size(); s_num += 2) {
		string aggr_ip(argv[s_num]);
		uint16_t aggr_port = atoi(argv[s_num + 1]);
		aggregators.push_back(SolverAggregator::NetTarget{aggr_ip, aggr_port});
	}

	Rule winlab_rule;
	winlab_rule.physical_layer  = use_phy;
	winlab_rule.update_interval = 0;

	string data;
	ifstream infile;

	infile.open(argv[1]);
	if (!infile){
		cout<<"Error opening config file "<<argv[1]<<"\n";
		return 0;
	}
  auto stream_mod = std::dec;
  if (use_hex) { stream_mod = std::hex;}

  //Get the transmitters and receivers of interest.
  //Transmitters IDs appear on the first line of the file and receiver IDs
  //appear on the second line.
  getline(infile, data);
  {
    uint64_t buffer;
    istringstream s(data);
    while(s>>stream_mod>>buffer){
      //Only accept data from sensors that we care about
      Transmitter sensor_id;
      sensor_id.base_id = buffer;
      sensor_id.mask.upper = 0xFFFFFFFFFFFFFFFF;
      sensor_id.mask.lower = 0xFFFFFFFFFFFFFFFF;
      winlab_rule.txers.push_back(sensor_id);
      tx_ids.insert(buffer);
    }
  }

  getline(infile, data);
  {
    uint64_t buffer;
    istringstream s(data);
    while(s>>stream_mod>>buffer){
      rx_ids.insert(buffer);
    }
  }

	Subscription winlab_sub{winlab_rule};

  //Connect to the grail aggregators with our subscription lists.
  SolverAggregator aggregator(aggregators, packetCallback);
  aggregator.addRules(winlab_sub);
  while (1);
}
