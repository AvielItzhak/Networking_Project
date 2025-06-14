#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "udp_file_transfer.h"

#define PORT 55555
#define MAX_BUFFER_SIZE 1024
#define Packet_Max_SIZE 512
#define UPLOAD_Folder_NAME "./UploadedFiles"

/* OP_CODE */
#define UPLOAD      1 
#define DOWNLAOD    2
#define DATA        3
#define ACK         4 
#define ERROR       5
#define DELETE      6

typedef struct {// Request INFO
    int16_t CurOP_ID;
    char CurOP_FilePATH[256];
    size_t FilePATH_len;
    char CurOP_Detail[256];
} CurrRequestINFO;



int main() {
    
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    printf("TFTP Server listening on port %d...\n", PORT);


    // Main client Request and Data transfer Handler loop //
    while (1) {
         char ErrorHandler[256] = {0}; // Error string initialition
         char ACK_Response[32] = {0}; 

        // Recive Request from client
        int Req_msg_rec = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,
                                      (struct sockaddr *)&client_addr, &addr_len);
        if (Req_msg_rec <= 0) {
            perror("Error receiving message\n"); // Print Error detail in server terminal

            // Save Error detail in ErrorHandler and send it to Client
            sprintf(ErrorHandler,"Error receiving message: %s", strerror(errno));
            sendto(sockfd, ErrorHandler, 256, 0,(const struct sockaddr *)&client_addr, addr_len);

            printf("\nResponse sent to client\nListinig to further Request.....\n\n\n");
            continue; // Continue listening
        }
         if (Req_msg_rec > 0) { // Print message recieved in bytes
            printf("Request Message (%d bytes):\n", Req_msg_rec);
            for (int i = 0; i < Req_msg_rec; i++) {
                printf("%02X ", buffer[i]);
            }
            printf("\n\n");
        }


        // Translating the information from the message
        CurrRequestINFO ClientRequest = {0}; // Creating a Request struct to handle message info 

        if (0 != Convert_REQBytes_String(buffer, &ClientRequest.FilePATH_len, &ClientRequest.CurOP_ID, ClientRequest.CurOP_FilePATH)){
            printf("Request INFO:\nOperationID - %d : FilePATH/NAME - %s\n\n",ClientRequest.CurOP_ID,ClientRequest.CurOP_FilePATH);}
        else{
            printf("Error while trying translating");
        }
        

        /* Request Handler - DELETE || UPLOAD || DOWNLOAD:
            * DELETE - Deleting the file that client Requested and print and send the result detail.
            * DOWNLOAD - 
            * UPLOAD - 
         */ 

        if (ClientRequest.CurOP_ID == DELETE){
            
            if (RequestHandler_Delete(ClientRequest.CurOP_FilePATH,ClientRequest.CurOP_Detail) != 0){
                printf("somthing went wrong...");
            }
            printf("%s\n\n",ClientRequest.CurOP_Detail);
            
            sendto(sockfd, ClientRequest.CurOP_Detail, strlen(ClientRequest.CurOP_Detail), 0,
                                 (const struct sockaddr *)&client_addr, addr_len);
            printf("\nResponse sent to client\nListinig to further Request.....\n\n\n");
            
        }

        if (ClientRequest.CurOP_ID == UPLOAD){
            // In case Upload folder isn't exist create it
            if (access(UPLOAD_Folder_NAME, F_OK) != 0){
                if (mkdir(UPLOAD_Folder_NAME,777) == 0) {
                    printf("Directory '%s' created successfully.\n", UPLOAD_Folder_NAME);
                }else {
                    perror("Error creating directory\n");
                    exit(EXIT_FAILURE);
                }
            }

            // Building ACK Response and sending to client
            RequestACK_Upload(ACK_Response);
            
            sendto(sockfd, ACK_Response, (size_t)32, 0,
                                 (const struct sockaddr *)&client_addr, addr_len);
            printf("\nResponse sent to client\nAwaiting Data packets.....\n\n");

        }

       // if (ClientRequest.CurOP_ID == DOWNLAOD){
       //     
       //     
       //     sendto(sockfd, ClientRequest.CurOP_Detail, strlen(ClientRequest.CurOP_Detail), 0,
       //                          (const struct sockaddr *)&client_addr, addr_len);
       //     printf("\nResponse sent to client\nListinig to further Request.....\n\n\n");
       // }

        
        /*

        // Decrypting and checking integrity
        int bytes_rec_decrypt = Decryption(Req_msg_rec);
        if (bytes_rec_decrypt = 0) {
            perror("Error currapted packet");
            continue; // Continue listening, send package again 
        }

        // Check if packet according to the format and following request
        char **Req_info = Request_format_check(bytes_rec_decrypt);
        if (Req_info == 0){ 
            perror("Error packet not according to format");
            continue; // Continue listening, send package again  
        }

        // Action according to the request
        switch (*Req_info[0]) {
          case 1: // RRQ
            // find file and sent it to client without ACK first

            break;
          case 2: // WRQ
            // send ACK and waitng for packet

            break;
          case 7: // delete
            // find the file and delete it after that send a indication to client
            
            break;
        }

        
        // buffer[Req_msg_rec] = '\0'; // Null-terminate the received data
        // printf("Received from client %s:%d: %s\n",
        //        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);
        // // You can process the received data here and send a response back
        // const char *response = "Message received!";
        // sendto(sockfd, response, strlen(response), 0,
        //        (const struct sockaddr *)&client_addr, addr_len);

        printf("\n\n\n\nTFTP Server listening on port %d...\n", PORT);

        */
    }


    close(sockfd);
    return 0;
}