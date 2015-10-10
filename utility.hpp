//
//  utility.hpp
//  CSE422_PJ1
//
//  Created by Vincewang on 10/5/15.
//  Copyright © 2015 Vincewang. All rights reserved.
//

#ifndef utility_hpp
#define utility_hpp

#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

void execute_command(vector<string> &);
vector<string> parse_external(string,bool &, string &, bool &, string &);
void execute_external(vector<string> &, bool, string, bool, string);

bool isnumber(std::string str);

std::vector<std::string> parse_command(std::string command);

int numOfpipe(std::string s,std::vector<std::string> &commands,bool &backprocess);

#endif /* utility_hpp */
