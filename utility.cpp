//
//  utility.cpp
//  CSE422_PJ1
//
//  Created by Vincewang on 10/5/15.
//  Copyright © 2015 Vincewang. All rights reserved.
//

#include "utility.hpp"

using namespace std;

bool isnumber(string str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

vector<string> parse_command(string command)
{
    istringstream iss(command);     // deliminate each token by whitespace
    string str, remainder;
    vector<string> args;
    while (iss >> str)
    {
        /* ignore comment */
        if (str.find("#") != string::npos)
        {
            remainder = str.substr(0, str.find("#"));
            break;
        }
        args.push_back(str);
    }
    if (remainder != "")
    {
        args.push_back(remainder);
    }
    return args;
}

int numOfpipe(string s,vector<string> &commands,bool &backprocess){
    int num=0;
    string temp="";
    for(int i=0;i<s.size();++i){
        if(s[i]=='!'){
            backprocess=true;
            continue;
        }
        if(s[i]=='|'){
            commands.push_back(temp);
            temp="";
            num++;
        }else{
            temp=temp+s[i];
        }
    }
    num++;
    commands.push_back(temp);
    return num;
}