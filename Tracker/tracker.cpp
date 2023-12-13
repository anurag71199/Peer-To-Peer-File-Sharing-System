#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>

using namespace std;

string tracker1_ip, tracker2_ip;
uint16_t tracker1_port, tracker2_port;
unordered_map<string, int> session;
unordered_map<string, string> client_port;

unordered_map<string, int> loginmap;                               // username and session status
unordered_map<string, string> login_list;                          // username and password
unordered_map<string, string> admins;                              // map of admins with grp number and username
unordered_map<string, unordered_set<string>> grp_deets;            // group number and set of members in the group
unordered_map<string, unordered_map<string, set<string>>> seeders; // list of seeders : groupid-> filename:members

unordered_map<string, //group id
pair<pair<vector<string>, //first = members 
vector<string>>, // second = pending requests
pair<vector<string>, //shared files
vector<unordered_set<string>>>>>grp_ds; //seeding peers

int usercount=0;
int activesessioners=0;
int mycount=0;
string adminofgrp;


// 			//gid		#1mems 		#2pending reqs
// unordered_map<string,pair<vector<string>,vector<string>>>g;
// 							//filenames		#vector of set of seeders
// unordered_map<string,pair<vector<string>,vector<unordered_set<string>>>>grp_ds;



unordered_map<string, string> blockhash;
unordered_map<string, string> file_size;
unordered_map<string, int>peertracker;
unordered_set<string>filemapper;

int vectdel(vector<string> v, string ele)
{
	vector<string>::iterator i = find(v.begin(), v.end(), ele);
	if (i != v.end())
	{
		v.erase(i);
		return 0;
	}
	else{
	return -1; 
	}
}

void logger(string str)
{
	ofstream log_file("tracker_log.txt", ios_base::out | ios_base::app);
	log_file << str;
	log_file.close();
}

vector<string> mysplit(string given, char splitter)
{

	vector<string> ans;
	string s = "";
	for (int i = 0; i < given.size(); i++)
	{
		if (given[i] == splitter)
		{
			ans.push_back(s);
			s.clear();
			continue;
		}
		s += given[i];
	}
	ans.push_back(s);
	return ans;
}

int create_user(string username, string password)
{
	
	string line, check_username_exists;

	ifstream read_auth_file("users.txt");

	if (!read_auth_file)
	{
		logger("File users.txt not found!\n");
		return 1;
	}
	while (getline(read_auth_file, line))
	{
		check_username_exists = mysplit(line, ' ')[0];
		if (username == check_username_exists)
		{
			return 2; 
		}
	}
	loginmap[username] = 1; 

	read_auth_file.close();

	login_list[username] = "get_pass";
	ofstream write_auth_file;
	write_auth_file.open("users.txt", ios::out | ios::app); // open in append mode

	if (!write_auth_file)
	{
		logger("Cannot open users.txt in append mode!\n");
		return 1;
	}

	write_auth_file << username << " " << password << endl;

	write_auth_file.close();

	// user created successfully
	usercount++;
	logger(username + "created\n");
	return 0;
}

int login(string username, string password)
{
	string line, uname_check, pass_check;

	ifstream read_auth_file("users.txt");

	if (!read_auth_file)
	{
		logger("File users.txt not found!\n");
		return 1;
	}

	while (getline(read_auth_file, line))
	{
		uname_check = mysplit(line, ' ')[0];
		pass_check = mysplit(line, ' ')[1];

		if (username == uname_check)
		{
			if (password == pass_check)
			{
				if (session.find(username) == session.end())
				{
					session.insert({username, 1});
				}
				else
				{
					if (session[username])
					{
						return 1; // already logged in
					}
					else
					{
						session[username] = 1;
					}
				}
				return 0; // login successful
			}
			else
			{
				return 2; // wrong password
			}
		}
	}

	activesessioners++;
	read_auth_file.close();

	return 2; // username not found
}

int create_group(string groupname, string admin)
{
	int sz = login_list[groupname].size();
	for(int i=0;i<sz;i++){
		mycount++;
	}
	if (grp_ds.find(groupname) == grp_ds.end())
	{
		grp_ds[groupname].first.first.push_back(admin);
	}
	else
	{
		return 1; // groupname already exist
	}

	// group created successfully
	return 0;
}

int request_join_group(string groupname, string username)
{
	if (grp_ds.find(groupname) == grp_ds.end())
	{
		// group not found
		return 1;
	}
	else
	{
		grp_ds[groupname].first.second.push_back(username);
	}
	int sz = login_list[groupname].size();
	for(int i=0;i<sz;i++){
		mycount++;
	}
	// request to join group sent successfully
	return 0;
}

int accept_join_group(string groupname, string admin, string username)
{
	if (grp_ds.find(groupname) == grp_ds.end())
	{
		// group not found
		return 1;
	}
	else
	{
		vector<string>::iterator index = find(grp_ds[groupname].first.second.begin(), grp_ds[groupname].first.second.end(), username);
		if (index == grp_ds[groupname].first.second.end())
		{
			return 2; // join request not found
		}
		else
		{
			if (admin == grp_ds[groupname].first.first[0])
			{
				grp_ds[groupname].first.first.push_back(username);
				vectdel(grp_ds[groupname].first.second, username);
			}
			else
			{
				return 3; // you are not an admin
			}
		}
	}
	return 0; // user accepted successfully
}

int leave_group(string groupname, string username)
{
	vector<string>::iterator index = find(grp_ds[groupname].first.first.begin(), grp_ds[groupname].first.first.end(), username);
	if (index == grp_ds[groupname].first.first.end())
	{
		return 1; // user not present in group
	}
	else
	{
		vectdel(grp_ds[groupname].first.first, username);
	}
	adminofgrp = username;
	return 0; // group left successfully
}

string list_pending_join_group_requests(string groupname)
{
	string ans = "";
	for (size_t i = 0; i < grp_ds[groupname].first.second.size(); i++)
	{
		ans += grp_ds[groupname].first.second[i] + "\n";
	}
	return ans;
}

string list_all_groups()
{
	string ans = "";
	for (auto i : grp_ds)
	{
		ans += i.first + "\n";
	}
	return ans;
}

int logout(string username)
{
	if (session[username] == 0)
	{
		return 1; // user not logged in
	}

	session[username] = 0;
	return 0; // logout successful
}

int upload_file(string file_path, string groupname, int client_socket, string username)
{
	if (grp_ds.find(groupname) == grp_ds.end())
	{
		// group not found
		return 1;
	}
	if (find(grp_ds[groupname].first.first.begin(), grp_ds[groupname].first.first.end(), username) == grp_ds[groupname].first.first.end())
	{
		// you are not a member of this group
		return 2;
	}
	struct stat buffer1;
	if (!(stat(file_path.c_str(), &buffer1) == 0))
	{
		// file does not exist
		return 3;
	}

	string filename = mysplit(file_path, '/').back();
	if (find(grp_ds[groupname].second.first.begin(), grp_ds[groupname].second.first.end(), filename) != grp_ds[groupname].second.first.end())
	{
		for (size_t i = 0; i < grp_ds[groupname].second.first.size(); i++)
		{
			if (grp_ds[groupname].second.first[i] == filename)
			{
				grp_ds[groupname].second.second[i].insert(username);
			}
		}
	}
	else
	{
		grp_ds[groupname].second.first.push_back(filename);
		unordered_set<string> s = {username};
		grp_ds[groupname].second.second.push_back(s);
	}

	char file_details[524288] = {0};
	write(client_socket, "Loading!!...", 12);

	int fl=0;
	int doupload=0;
	while(fl<20){
		doupload = fl;
		fl++;
	}

	if (read(client_socket, file_details, 524288))
	{
		string fdeets = string(file_details);
		// cout<<"worked1\n";
		if (fdeets != "error")
		{
			string hshval_of_pieces = "";
			vector<string> file_details_vector = mysplit(fdeets, '-');
			// cout<<"worked2\n";
			// file_details_vector = [filepath, peer address, file size, file hash, piecewise hash]

			size_t i = 4;
			while (i < file_details_vector.size())
			{
				hshval_of_pieces = hshval_of_pieces + file_details_vector[i];
				if (i == file_details_vector.size() - 1)
				{
					i += 1;
					// cout<<"workeding\n";
					continue;
				}
				else
				{
					hshval_of_pieces = hshval_of_pieces + "-";
					// cout<<"workeding\n";
				}
				i += 1;
			}
			blockhash[filename] = hshval_of_pieces;
			// cout<<"worked2\n";
			file_size[filename] = file_details_vector[2];
			// cout<<"worked good\n";
			write(client_socket, "Uploaded", 8);
			// cout<<"final work\n";
		}
	}
	return 0;
}

int download_file(string groupname, string filename, string des_path, string username, int client_socket)
{
	if (grp_ds.find(groupname) == grp_ds.end())
	{
		// group not found
		return 1;
	}
	if (find(grp_ds[groupname].first.first.begin(), grp_ds[groupname].first.first.end(), username) == grp_ds[groupname].first.first.end())
	{
		// you are not a member of this group
		return 2;
	}
	const string &s = des_path;
	struct stat buffer;
	if (stat(s.c_str(), &buffer) != 0)
	{
		// destination directory not found
		return 3;
	}
	string destinationpathtosource = des_path + '/' + filename;
	string byuser = username + ':' + groupname;

	char fdeetschar[524288] = {0};
	// fdeetschar = [filename, destination, group id]
	write(client_socket, "Downloading...", 13);
	if (read(client_socket, fdeetschar, 524288))
	{
		
		string reply = "";
		string myrep = "";
		int todo = 20;
		vector<string> fdet = mysplit(string(fdeetschar), '-');
		for(int ls=0;ls<todo;ls++){
			myrep = string(fdeetschar);
		}
		if (find(grp_ds[groupname].second.first.begin(), grp_ds[groupname].second.first.end(), fdet[0]) == grp_ds[groupname].second.first.end())
		{
			write(client_socket, "File not found", 14);
			mycount--;
		}
		else
		{
			int temp;
			size_t i;
			for (i = 0; i < grp_ds[groupname].second.first.size(); i++)
			{
				if (fdet[0] == grp_ds[groupname].second.first[i])
				{
					temp = i;
					for (auto j : grp_ds[groupname].second.second[i])
					{
						cout << client_port[j] << endl;
						reply += client_port[j] + '-';
					}
				}
			}

			reply += file_size[fdet[0]];

			write(client_socket, &reply[0], reply.length());
			// synch(client_socket);
			char buff1[5];
			read(client_socket, buff1, 5);

			write(client_socket, &blockhash[fdet[0]][0], blockhash[fdet[0]].length());

			grp_ds[groupname].second.second[temp].insert(username);
		}
	}
	return 0;
}

void compute_client(int client_socket)
{
	
	string username = "";
	while (1)
	{
		char read_buffer[1024] = {0};

		if (read(client_socket, read_buffer, 1024) <= 0)
		{
			session[username] = 0;
			close(client_socket);
			break;
		}

		logger("Client request string : " + string(read_buffer) + "\n");

		vector<string> command = mysplit(string(read_buffer), ' ');

		string cmd = command[0];
		if (cmd == "create_user")
		{
			if(command.size()==3)
			{
				int status = create_user(command[1], command[2]);
				if (status == 2)
				{
					write(client_socket, "Username already exists!", 24);
				}
				else if (status == 1)
				{
					write(client_socket, "File error: users.txt", 20);
				}
				else
				{
					write(client_socket, "Account created", 15);
				}
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "login")
		{
			if(command.size()==3)
			{
				int status = login(command[1], command[2]);
				if (status == 2)
				{
					write(client_socket, "Username/Password incorrect", 27);
				}
				else if (status == 1)
				{
					write(client_socket, "You already have one active session", 35);
				}
				else
				{
					logger("Login Successful: " + to_string(client_socket) + "\n");
					write(client_socket, "Login Successful", 16);

					username = command[1];

					char buf[96];
					read(client_socket, buf, 96);
					string peer_address = string(buf);
					logger("Peer Address: " + peer_address + "\n");
					client_port[username] = peer_address;
				}
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "create_group")
		{
			if(command.size()==2)
			{
				int status = create_group(command[1], username);
				if (status == 1)
				{
					write(client_socket, "Group already exists", 20);
				}
				else
				{
					write(client_socket, "Group created", 13);
				}
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "join_group")
		{
			if (command.size() == 2)
			{
				int status = request_join_group(command[1], username);

				if (status == 1)
				{
					write(client_socket, "Group does not exist", 20);
				}
				else
				{
					write(client_socket, "Request to join group sent", 26);
				}
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "leave_group")
		{
			if (command.size() == 2)
			{
				short status = leave_group(command[1], username);
				if (status == 1)
				{
					write(client_socket, "You are not present in the group", 32);
				}
				else
				{
					write(client_socket, "Group left", 10);
				}
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "list_requests")
		{
			if (command.size() == 2)
			{
				string pending_join_requests = list_pending_join_group_requests(command[1]);
				write(client_socket, pending_join_requests.c_str(), pending_join_requests.size() - 1);
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "accept_request")
		{
			if (command.size() == 3)
			{
				short status = accept_join_group(command[1], username, command[2]);
				if (status == 1)
				{
					write(client_socket, "Group not found", 15);
				}
				else if (status == 2)
				{
					write(client_socket, "Join request not found", 22);
				}
				else if (status == 3)
				{
					write(client_socket, "You are not an admin", 20);
				}
				else
				{
					write(client_socket, "User added in group", 19);
				}
			}
			else{
				write(client_socket, "Invalid argument count", 22);
			}
		}
		else if (cmd == "list_groups")
		{
			if (command.size() != 1)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				string all_groups = list_all_groups();
				write(client_socket, all_groups.c_str(), all_groups.size() - 1);
			}
		}
		else if (cmd == "logout")
		{
			if (command.size() != 1)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				int status = logout(username);
				if (status == 1)
				{
					write(client_socket, "User not logged in", 18);
				}
				else
				{
					write(client_socket, "Logged out", 10);
				}
			}
		}
		else if (cmd == "upload_file")
		{
			if (command.size() != 3)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				int status = upload_file(command[1], command[2], client_socket, username);
				if (status == 1)
				{
					write(client_socket, "Group not found", 15);
				}
				else if (status == 2)
				{
					write(client_socket, "You are not a member of the group", 33);
				}
				else if (status == 3)
				{
					write(client_socket, "File not found", 14);
				}
			}
		}
		else if (cmd == "list_files")
		{
			string res = "";
			for (size_t i = 0; i < grp_ds[command[1]].second.first.size(); i++)
			{
				res += grp_ds[command[1]].second.first[i] + "\n";
			}
			write(client_socket, res.c_str(), res.size() - 1);
		}
		else if (cmd == "download_file")
		{
			if (command.size() != 4)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = download_file(command[1], command[2], command[3], username, client_socket);
				if (status == 1)
				{
					write(client_socket, "Group not found", 15);
				}
				else if (status == 2)
				{
					write(client_socket, "You are not a member of the group", 33);
				}
				else if (status == 3)
				{
					write(client_socket, "Destination path not found", 26);
				}
			}
		}
	}

	logger("----Thread ended for client socket: " + to_string(client_socket));
	close(client_socket);
}

int main(int argc, const char *argv[])
{
	logger("\n----------------------------------------------------------------\n");

	ifstream tracker_info_file("tracker_info.txt");
	if (!tracker_info_file)
	{
		logger("Error opening tracker_info.txt file\n");
		cout << "Tracker 1 IP: ";
		cin >> tracker1_ip;
		cout << "Tracker 1 PORT: ";
		cin >> tracker1_port;
	}
	else
	{
		getline(tracker_info_file, tracker1_ip);

		string port;
		getline(tracker_info_file, port);
		tracker1_port = stoi(port);
	}
	// tracker1_ip = argv[1];
	// tracker1_port = stoi(argv[2]);

	int tracker_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addr_len = sizeof(address);

	if ((tracker_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		logger("Socket creation failed!\n");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(tracker_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		logger("setsockopt error!\n");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_port = htons(tracker1_port);

	if (inet_pton(AF_INET, &tracker1_ip[0], &address.sin_addr) <= 0)
	{
		logger("Address not supported!\n");
		exit(EXIT_FAILURE);
	}

	if (bind(tracker_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		logger("Bind failed!\n");
		exit(EXIT_FAILURE);
	}

	if (listen(tracker_socket, 3) < 0)
	{
		logger("Socket listen failed!\n");
		exit(EXIT_FAILURE);
	}

	vector<thread> thread_vector;

	while (true) // listen for new clients and create new thread for every new client
	{
		cout << "**--Tracker Console--**\n";
		logger("Listening...\n");

		int client_socket;

		if ((client_socket = accept(tracker_socket, (struct sockaddr *)&address, (socklen_t *)&addr_len)) < 0)
		{
			logger("Error in accepting client! Client socket: " + to_string(client_socket) + "\n");
		}
		else
		{
			logger("Connection Accepted for client socket: " + to_string(client_socket) + "\n");
			thread_vector.push_back(thread(compute_client, client_socket));
		}
	}

	// join every thread which has completed its execution
	for (auto i = thread_vector.begin(); i != thread_vector.end(); i++)
	{
		if (i->joinable())
		{
			i->join();
		}
	}

	// close logger file

	return 0;
}