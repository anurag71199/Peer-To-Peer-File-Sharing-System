#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <bits/stdc++.h>

using namespace std;

struct cfiles
{
	long long int flen;
	string cfclient_ip;
	string cfclient_fname;
};

struct chunkstruct
{
	string chunkip;
	string chunk_filename;
	long long int chunkno;
	string dest;
};

/*** DATA STRUCTURES ***/
unordered_map<string, string> filepath_storage;
unordered_map<string, vector<int>> fchunkinfomation;
unordered_map<string, unordered_map<string, int>> upload_check;
vector<vector<string>> cdownloaded_file_chunk;
unordered_map<string, string> download_file_lis;
vector<string> cfile_piece_wise_hash;
/*** --------------- ***/

/*** GLOBAL VARIABLES ***/
int session = 0;
string client_ip;
unsigned short client_port;
int corrupt_status;
int cc;
/*** ---------------- ***/

/*** LOGGER ***/
void log(string text)
{
	ofstream log_file("client_log.txt", ios_base::out | ios_base::app);
	log_file << text;
	log_file.close();
}
/*** ------- ***/

vector<string> extract_trackerfile(string s)
{
    vector<string> retstring;
    fstream f;
    f.open(s);
    if (!f.is_open())
    {
        // cout<<"ret from here \n";
        return retstring; // return empty vector, handled in main
    }
    string rd;
    while (getline(f, rd))
    {
        retstring.push_back(rd);
    }
    f.close();
    return retstring;
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

long long int size_calc(char *path)
{
	long long int file_size = -1;
	FILE *f = fopen(path, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		file_size = ftell(f) + 1;
		fclose(f);	
	}
	else
	{
		cout << "File not found" << endl;
	}
	cc++;
	return file_size;
}

int connect_to_tracker(struct sockaddr_in &serv_addr, int sock, string tracker_ip, unsigned short tracker_port)
{
	// connect client to tracker
	char *connected_tracker_ip = &tracker_ip[0];
	;
	unsigned short connected_tracker_port = tracker_port;

	// connected_tracker_ip = &tracker_ip[0];
	// connected_tracker_port = tracker_port;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(connected_tracker_port);
	int flag = 0;
	if (inet_pton(AF_INET, connected_tracker_ip, &serv_addr.sin_addr) <= 0)
	{
		flag == 1;
	}

	int status = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (status < 0)
	{
		flag == 1;
	}
	if (flag == 1)
	{
		return -1;
	}
	return 0;
}

void hash_str_conv(string &hashval, string blockval)
{
	unsigned char md[20];
	if (SHA1(reinterpret_cast<const unsigned char *>(&blockval[0]), blockval.length(), md))
	{

		for (int i = 0; i < 20; i++)
		{
			char buffer[3];
			string temp;
			sprintf(buffer, "%02x", md[i] & 0xff);
			temp = string(buffer);
			hashval += temp;
		}
		hashval += "-";
	}
	else
	{

		printf("Error in hashing\n");
		hashval += "-";
	}
}

string make_hash(char *fpath)
{
	FILE *f;
	// int i;
	long long int f_size = size_calc(fpath);
	if (f_size == -1)
		return "-";
	string hasher = "";
	char line[32769];

	int blocks = f_size / 524288 + 1; // 512 KB into BYTES
	f = fopen(fpath, "r");

	// check function ************************************
	if (!f)
	{
		hasher = hasher + "-";
		printf("Enter valid file name File not found.\n");
		hasher.pop_back();
		hasher.pop_back();
		return hasher;
	}
	for (int i = 0; i < blocks; i++)
	{
		string blockval;
		int accum = 0;
		int rc;
		while (accum < 524288 && (rc = fread(line, 1, min(32767, 524288 - accum), f)))
		{
			line[rc] = '\0';
			blockval = blockval + line;
			accum = accum + strlen(line);
			memset(line, 0, sizeof(line)); // clear buffer
		}
		hash_str_conv(hasher, blockval);
	}
	fclose(f);
	hasher.pop_back();
	hasher.pop_back();
	return hasher;
}

string filehashhelper(char *path)
{
	string hvalue;
	unsigned char md[32];
	ifstream inp(path);
	ostringstream buffer;

	buffer << inp.rdbuf();
	string data = buffer.str();

	if (SHA256(reinterpret_cast<const unsigned char *>(&data[0]), data.length(), md))
	{
		for (int i = 0; i < 32; i++)
		{
			char buffer[3];
			string buffstr;
			sprintf(buffer, "%02x", md[i] & 0xff);
			buffstr = string(buffer);
			hvalue += buffstr;
		}
		return hvalue;
	}
	cout << "Error in Hashing" << endl;
	return hvalue;
}

int check_uploadstat(int sock, string fname, vector<string> input)
{
	if (upload_check[input[2]].find(fname) != upload_check[input[2]].end())
	{
		cout << "File uploaded previously" << endl;
		int sstatus = send(sock, "error", 5, MSG_NOSIGNAL);
		if (sstatus != -1)
		{
			return 0;
		}
		else
		{
			printf("Error: %s\n", strerror(errno));
			return -1;
		}
	}
	return 1;
}

int upload_filefunc(vector<string> cmd, int sock)
{
	if (cmd.size() != 3)
	{
		return 0;
	}

	char *fpath = &cmd[1][0];
	string fpathstr = string(fpath);
	vector<string> fpathvect = mysplit(fpathstr, '/');
	string fname = fpathvect.back();

	int status_of_upload = check_uploadstat(sock, fname, cmd);
	if (status_of_upload != 1)
	{
		return status_of_upload;
	}
	upload_check[cmd[2]][fname] = 1;
	filepath_storage[fname] = fpathstr;
	string block_hash = make_hash(fpath);

	string file_deets = "";
	if (block_hash != "-")
	{
		string file_hash = filehashhelper(fpath);
		string fsize = to_string(size_calc(fpath));

		// appendip_port(fileDetails, fpathstr, filehash, piece_wise_hash, fsize);
		file_deets += fpathstr + "-" + string(client_ip) + ":" + to_string(client_port) + "-" + fsize + "-" + file_hash + "-" + block_hash;
		// fileDetails += fsize + "-" + filehash + "-" + piece_wise_hash;

		int sockcheck = send(sock, &file_deets[0], strlen(&file_deets[0]), MSG_NOSIGNAL);
		if (sockcheck != -1)
		{
			char sreply[10240] = {0};
			read(sock, sreply, 10240);
			cout << sreply << "\n";

			long long int x = stoll(fsize) / 524288 + 1;
			vector<int> tmp(x + 1, 1);
			fchunkinfomation[fname] = tmp;
			return 0;
		}
		else
		{
			cout << "Error " << strerror(errno) << endl;
			return -1;
		}
	}
	else
	{
		return 0;
	}
}

int write_in_file(int csock, long long int chunkno, char *fpath)
{
	char buffer[524288];
	string cont = "";
	int n;
	string hashval = "";
	for (int j = 0; j < 524288; j = j + n)
	{
		n = read(csock, buffer, 524287);
		if (n <= 0)
		{
			break;
		}
		else
		{
			buffer[n] = 0;
			fstream outfile(fpath, std::fstream::in | std::fstream::out | std::fstream::binary);
			outfile.seekp(chunkno * 524288 + j, ios::beg);
			outfile.write(buffer, n);
			outfile.close();
			// string writtenlocst = to_string(total + chunkno * 524288);
			// string writtenloced = to_string(total + chunkno * 524288 + n - 1);

			cont = cont + buffer;

			// bzero(buffer, 524288);
			memset(buffer, 0, 524288);
		}
	}

	hash_str_conv(cont, hashval);

	string fpathstr = string(fpath);
	vector<string> fpathvect = mysplit(fpathstr, '/');
	string filename = fpathvect.back();
	// string chnknumstr = to_string(chunkno);

	fchunkinfomation[filename][chunkno] = 1;

	return 0;
}

string client_as_server_connect(char *cserverip, char *cserverport_ip, string cmd)
{
	struct sockaddr_in caddress;
	int csock = socket(AF_INET, SOCK_STREAM, 0);
	if (csock < 0)
	{
		cout << "Error in creating Socket" << endl;
		return "error";
	}

	caddress.sin_family = AF_INET;
	uint16_t cport = stoi(string(cserverport_ip));
	caddress.sin_port = htons(cport);
	if (inet_pton(AF_INET, cserverip, &caddress.sin_addr) < 0)
	{
		perror("Client Connection Error(INET)");
	}

	vector<string> cmdvect = mysplit(cmd, '-');
	string cmdprefix = cmdvect.front();
	if (connect(csock, (struct sockaddr *)&caddress, sizeof(caddress)) < 0)
	{
		perror("Client Connection Error");
	}

	if (cmdprefix == "get_chunk_vector")
	{
		char sreply[10240] = {0};
		int sstatus = send(csock, &cmd[0], strlen(&cmd[0]), MSG_NOSIGNAL);
		if (sstatus == -1)
		{
			cout << "Error has occured while sending command" << strerror(errno) << endl;

			return "error";
		}

		int repstatus = read(csock, sreply, 10240);
		if (repstatus < 0)
		{
			perror("err: ");
			return "error";
		}
		else
		{
			close(csock);
			return string(sreply);
		}
	}
	if (cmdprefix == "get_chunk")
	{
		int sstatus = send(csock, &cmd[0], strlen(&cmd[0]), MSG_NOSIGNAL);
		if (sstatus == -1)
		{
			printf("Error: %s\n", strerror(errno));
			return "error";
		}
		else
		{
			vector<string> cmdsplitvect = mysplit(cmd, '-');

			string despath = cmdsplitvect[3];
			long long int chunkno = stoll(cmdsplitvect[2]);
			// string cserverport_ipstr = string(cserverport_ip);
			// string chunkNumstr = to_string(chunkno);

			write_in_file(csock, chunkno, &despath[0]);
			return "ss"; // check ************
		}
	}
	if (cmdprefix == "get_file_path")
	{
		char sreply[10240] = {0};
		int sstatus = send(csock, &cmd[0], strlen(&cmd[0]), MSG_NOSIGNAL);
		if (sstatus == -1)
		{
			printf("Error: %s\n", strerror(errno));
			return "error";
		}

		if (read(csock, sreply, 10240) < 0)
		{
			perror("err: ");
			return "error";
		}
		else
		{
			vector<string> fn = mysplit(cmd, '-');
			filepath_storage[fn.back()] = string(sreply);
		}
	}

	close(csock);
	string cportstr = to_string(cport);
	string cserveripstr = string(cserverip);
	return "aa"; // check *******
}

void getChunkInfo(cfiles *pf)
{

	string cip = string(pf->cfclient_ip);
	vector<string> senderadd = mysplit(cip, ':');
	string pname = string(pf->cfclient_fname);
	string command = "get_chunk_vector-" + pname;
	string response = client_as_server_connect(&senderadd[0][0], &senderadd[1][0], command);
	int csize = cdownloaded_file_chunk.size();
	for (size_t i = 0; i < csize; i++)
	{
		if (response[i] == '1')
		{
			cdownloaded_file_chunk[i].push_back(string(pf->cfclient_ip));
		}
	}
	for (auto j : cdownloaded_file_chunk)
	{
		for (auto b : j)
		{
			cout << b << "\t";
		}
		cout << "\n";
	}
	delete pf;
}

void getChunk(chunkstruct *obj)
{

	string filename = obj->chunk_filename;
	vector<string> sender_ip = mysplit(obj->chunkip, ':');
	long long int chunkno = obj->chunkno;
	string dest = obj->dest;

	string command1 = "get_chunk-" + filename;
	string cnumstr = to_string(chunkno);
	command1 = command1 + "-" + cnumstr;
	command1 = command1 + "-" + dest;
	client_as_server_connect(&sender_ip[0][0], &sender_ip[1][0], command1);

	delete obj;
	return;
}

void piece_selection_algorithm(vector<string> clients, vector<string> inpt,long long filesize)
{

	long long blocks=filesize/524288+1;

	cdownloaded_file_chunk.clear();
	cdownloaded_file_chunk.resize(blocks);

	vector<thread> thread1;
	vector<thread> thread2;

	int n = clients.size();
	size_t i = 0;

	for(i=0;i<n;i++)
	{
		cfiles *cp = new cfiles();
		cp->flen = blocks;
		cp->cfclient_fname = inpt[2];
		cp->cfclient_ip = clients[i];
		thread1.push_back(thread(getChunkInfo, cp));
	}

	for (auto i = thread1.begin(); i != thread1.end(); i++)
	{
		if (i->joinable())
		{
			i->join();
		}
	}

	// int n1 = cdownloaded_file_chunk.size();
	// for (size_t i = 0; i < n1; i++)
	// {
	// 	int sz = cdownloaded_file_chunk[i].size();
	// 	if (sz == 0)
	// 	{
	// 		cout << "Error: Parts not available" << endl;
	// 		return;
	// 	}
	// }
	
	thread1.clear();
	srand((unsigned)time(0));
	long long int blocksReceived = 0;

	string destpath = inpt[3] + "/" + inpt[2];
	// string destpath = destpath + inpt[2];
	FILE *fp = fopen(&destpath[0], "r+");
	if (fp == 0)
	{
		string f(filesize, '\0');
		fstream in(&destpath[0], ios::out | ios::binary);
		in.write(f.c_str(), strlen(f.c_str()));
		in.close();
		fchunkinfomation[inpt[2]].resize(blocks, 0);

		vector<int> temp(blocks, 0);
		corrupt_status = false;
		fchunkinfomation[inpt[2]] = temp;

		string cl_path;

		while (blocksReceived < blocks)
		{

			long long int select_piece;
			while (true)
			{
				select_piece = rand() % blocks;
				if (fchunkinfomation[inpt[2]][select_piece] != 0)
				{
					continue;
				}
				else
				{
					break;
				}
			}
			long long int select_list = cdownloaded_file_chunk[select_piece].size();
			string selected_cl = cdownloaded_file_chunk[select_piece][rand() % select_list];

			chunkstruct *newch = new chunkstruct();

			newch->chunkip = selected_cl;
			// string dest1 = inpt[3] + "/";
			newch->dest = inpt[3] + "/" + inpt[2];
			newch->chunkno = select_piece;
			// string reqchunknumstr = to_string(newch->chunkno);
			newch->chunk_filename = inpt[2];

			fchunkinfomation[inpt[2]][select_piece] = 1;

			thread2.push_back(thread(getChunk, newch));
			blocksReceived++;
			cl_path = selected_cl;
		}

		for (auto i = thread2.begin(); i != thread2.end(); i++)
		{
			if (i->joinable())
			{
				i->join();
			}
		}

		download_file_lis.insert({inpt[2], inpt[1]});

		if (corrupt_status == 0)
		{

			cout << "Download successful. Corruption Status : Not corrupted\n";
		}
		if (corrupt_status == 1)
		{
			cout << "Downloaded Successful. Corruption Status : File Corrupted\n";
		}

		string cmdsend;
		vector<string> serverAddress = mysplit(cl_path, ':');
		cmdsend = "get_file_path-";
		cmdsend = cmdsend + inpt[2];
		client_as_server_connect(&serverAddress[0][0], &serverAddress[1][0], cmdsend);
		return;
	}
	else
	{
		printf("The file already exists.\n");
		fclose(fp);
		return;
	}
}

void chunk_send_fun(char *fpath, int chunkno, int csock)
{
	// string sent = "";
	std::ifstream op(fpath, std::ios::in | std::ios::binary);
	op.seekg(chunkno * 524288, op.beg);

	char buffer[524288] = {0};
	op.read(buffer, sizeof(buffer));
	int count = op.gcount();
	int rc = send(csock, buffer, count, 0);

	if (rc == -1)
	{
		perror("Error in sending file.");
		exit(1);
	}

	op.close();
}

void process_request(int csock)
{
	// string clientid = "";
	char ipline[1024] = {0};
	if (read(csock, ipline, 1024) <= 0)
	{
		close(csock);
		return;
	}

	vector<string> inp = mysplit(string(ipline), '-');

	if (inp[0] == "get_chunk_vector")
	{
		string s = "";
		vector<int> chunkvec = fchunkinfomation[inp[1]];
		for (int i = 0; i < chunkvec.size(); i++)
		{
			s += to_string(chunkvec[i]);
		}
		char *reply = &s[0];
		write(csock, reply, strlen(reply));
		close(csock);
		return;
	}
	else if (inp[0] == "get_file_path")
	{
		string fpath = filepath_storage[inp[1]];
		write(csock, &fpath[0], strlen(fpath.c_str()));
		close(csock);
		return;
	}
	else if (inp[0] == "get_chunk")
	{
		long long int chunknom = stoll(inp[2]);
		string fpath = filepath_storage[inp[1]];

		chunk_send_fun(&fpath[0], chunknom, csock);
		close(csock);
		return;
	}

	close(csock);
	return;
}

void *client_as_server(void *arg)
{
	int sock;
	struct sockaddr_in address;
	int opt = 1;
	int addr_len = sizeof(address);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		log("Socket creation failed!\n");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		log("setsockopt error!\n");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_port = htons(client_port);

	if (inet_pton(AF_INET, &client_ip[0], &address.sin_addr) <= 0)
	{
		log("Address not supported!\n");
		exit(EXIT_FAILURE);
	}

	if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		log("Bind failed!\n");
		exit(EXIT_FAILURE);
	}

	if (listen(sock, 3) < 0)
	{
		log("Socket listen failed!\n");
		exit(EXIT_FAILURE);
	}

	vector<thread> th_vector;

	while (1) 
	{
		log("Client is listening\n");

		int client_socket;

		if ((client_socket = accept(sock, (struct sockaddr *)&address, (socklen_t *)&addr_len)) < 0)
		{
			log("Error-> Client socket: " + to_string(client_socket) + "\n");
		}
		else
		{
			log("Accepted for : " + to_string(client_socket) + "\n");
			th_vector.push_back(thread(process_request, client_socket));
		}
	}

	// join every thread which has completed its execution
	for (auto i = th_vector.begin(); i != th_vector.end(); i++)
	{
		if (i->joinable())
		{
			i->join();
		}
	}

	close(sock);
}

int command_func(int sock, vector<string> inputvect)
{
	char servrep_buff[10240];
	// bzero(servrep_buff, 1024);
	memset(servrep_buff, 0, 10240);
	read(sock, servrep_buff, 10240);
	cout<<servrep_buff<<"\n";

	string server_reply = string(servrep_buff);

	if (server_reply == "Invalid argument count")
	{
		log("Invalid argument count");
		cout<<"Recheck argument count!\n";
		return 0;
	}

	string cmd = inputvect[0];

	if (cmd == "login")
	{
		if (server_reply == "Username/Password incorrect")
		{
			log("Username/Password incorrect\n");
			// cout<<"Invalid Credentials\n";
		}
		else if (server_reply == "You already have one active session")
		{
			log("Active session\n");
			// cout<<"Active session\n";
		}
		else
		{
			string cportval = to_string(client_port);
			string caddress = client_ip + ":" + cportval;
			session = 1;
			int l = caddress.length();
			write(sock, &caddress[0], l);
			log("Login successful\n");
			// cout<<"Login Successful\n";
		}
		return 0;
	}
	else if (cmd == "logout")
	{
		if (session == 0)
		{
			log("You are not logged in\n");
		}
		else
		{
			session = 0;
			log("Logout successful\n");
		}
	}
	else if (cmd == "create_group")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (cmd == "join_group")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (cmd == "leave_group")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (cmd == "list_requests")
	{
		// cout << server_reply << endl;
		log(server_reply + "\n");
		return 0;
	}
	else if (cmd== "accept_request")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (cmd == "list_groups")
	{
		// cout << server_reply << endl;
		log(server_reply + "\n");
		return 0;
	}
	else if (cmd == "upload_file")
	{
		if (server_reply == "You are not a member of the group")
		{
			cout << "You don't have membership in this group\n";
			return 0;
		}
		else if (server_reply == "Group not found")
		{
			cout << "Group does not exist\n";
			return 0;
		}
		else if (server_reply == "File not found")
		{
			cout << "File not found\n";
			return 0;
		}
		else if (server_reply == "Command Invalid!")
		{
			cout << "Command Invalid!\n";
			return 0;
		}
		return upload_filefunc(inputvect, sock);
	}
	else if (cmd == "download_file")
	{
		if (server_reply == "Group not found")
		{
			cout << "Group does not exist\n";
			return 0;
		}
		else if (server_reply == "You are not a member of the group")
		{
			cout << "You don't have membership in this group\n";
			return 0;
		}
		else if (server_reply == "Destination path not found")
		{
			cout << "Destination path invalid\n";
			return 0;
		}
		string file_det = "";
		if (inputvect.size() == 4)
		{

			file_det = file_det + inputvect[2] + "-" + inputvect[3] + "-" + inputvect[1];
			// file_det = file_det + inputvect[3] + "-";
			// file_det = file_det + inputvect[1];
			
			char sreply[524288] = {0};
			// fileDetails = [filename, destination, group id]

			int sockstatval2 = send(sock, &file_det[0], strlen(&file_det[0]), MSG_NOSIGNAL);
			if (sockstatval2 == -1)
			{
				printf("Error: %s\n", strerror(errno));
				return -1;
			}

			read(sock, sreply, 524288);

			if (string(sreply) != "File not found")
			{

				vector<string> file_clients = mysplit(sreply, '-');

				// synch(sock);
				char dumtemp[5];
				strcpy(dumtemp, "ok");
				write(sock, dumtemp, 5);

				// bzero(sreply, 524288);
				memset(sreply, 0, 524288);
				read(sock, sreply, 524288);

				vector<string> tempo = mysplit(string(sreply), '-');

				cfile_piece_wise_hash = tempo;

				long long filesize = stoll(file_clients.back());
				file_clients.pop_back();
				piece_selection_algorithm(file_clients, inputvect,filesize);
				return 0;
			}
			else
			{
				// cout << sreply << endl;
				return 0;
			}
			return 0;
		}
		return 0;
	}
}

int main(int argc, char *argv[])
{
	log("\nStart Logger\n");

	//  INPUT FORMAT //
    /*
    argument 1 : EXECUTION COMMAND (eg: ./peer)
    argument 2 : IP of CLIENT (eg: 127.0.0.1)
    argument 3 : PORT NO (eg: 8080)
    argument 4 : TRACKER DETAILS TEXT FILE (eg: tracker_details.txt)
     */
    if (argc != 4)
    {
        cout << "Invalid Arguments! Try again\n";
        return 0; // return -1
    }

	client_ip = argv[1];
	client_port = stoi(argv[2]);

	// Text File of Tracker Details
    // Assuming Tracker file is present in the same directory
    string tracker_filename = argv[3]; // tracker_details.txt
    vector<string> tracker_deets;
    tracker_deets = extract_trackerfile(tracker_filename);
    if (tracker_deets.size() == 0)
    {
        cout << "Tracker file not found error.\n";
        return 0;
    }

	string tracker1_ip = tracker_deets[0];
    unsigned short tracker1_port = stoi(tracker_deets[1]);

	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_address;

	if (client_socket < 0)
	{
		log("Error in Creating Socket!\n");
		exit(EXIT_FAILURE);
	}

	// thread(client_as_server);
	pthread_t server_thread; // server thread
	int threadval = pthread_create(&server_thread, NULL, client_as_server, NULL);
	if (threadval == -1) // Creating a new thread and checking if it is created successfully or not
	{
		perror("pthread");
		exit(EXIT_FAILURE);
	}

	if (connect_to_tracker(server_address, client_socket,tracker1_ip,tracker1_port) < 0)
	{
		log("Error connecting to tracker!\n");
		exit(EXIT_FAILURE);
	}
	log("Connected to tracker\n");

	while (true)
	{
		cout << "\n";
        cout << "*****CLIENT CONSOLE*****\n";
        cout << "________________________\n";
        cout << "$ ";

		string input_line;

		getline(cin, input_line);
        cout << "\n________________________\n";
        cout << "\n\n";
		if (input_line.length() < 0)
		{
			continue;
		}

		vector<string> commandvec = mysplit(input_line, ' ');

		string base = commandvec[0];

		if (base == "exit")
		{
			log("Exiting...\n");
			break;
		}

		if (session == true && base == "login")
		{
			cout << "Client is already Logged in!\n";
			continue;
		}

		if (session == false && base != "login" && base != "create_user")
		{
			cout << "Client needs to login first.\n";
			continue;
		}

		if (send(client_socket, &input_line[0], strlen(&input_line[0]), MSG_NOSIGNAL) == -1)
		{
			log("Log connection error client side!\n");
			printf("Error: %s\n", strerror(errno));
			return -1;
		}

		log("Calling command_func\n");
		command_func(client_socket, commandvec);
	}

	close(client_socket);
	return 0;
}