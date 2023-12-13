## Peer-to-Peer Group Based File Sharing System Assignment - AOS

### Tracker :
Compile Tracker : 
`g++ tracker.cpp -o tracker` 

Run Tracker : 
`./tracker`


### Client :
Compile Client : 
`g++ client.cpp -o client -lssl -lcrypto` 

Run Client : 
`./client 127.0.0.1 8080 tracker_info.txt`


### List of Commands :

- Create user account :
```
   create_user​ <user_id> <password> 
```
- Login :
```
    login​ <user_id> <password>
```
- Create Group :
```
    create_group <group_id>
```
- Join Group :
```
    join_group​ <group_id>
```
- Leave Group :
```
    leave_group​ <group_id>
```
- List pending requests :
```
    list_requests ​<group_id>
```
- Accept Group Joining Request :
```
    accept_request​ <group_id> <user_id>
```
- List All Group In Network :
```
    list_groups
```
- List All sharable Files In Group :
```
    list_files​ <group_id>
```
- Upload File :
```
    upload_file​ <file_path> <group_id​>
```
- Download File :
```
    download_file​ <group_id> <file_name> <destination_path>
```
- Logout :
```
    logout
```
