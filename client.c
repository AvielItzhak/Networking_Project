#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include "udp_file_transfer.h"


#define SERVER_IP "127.0.0.1" // Loopback
#define PORT 55555
#define MAX_BUFFER_SIZE 1024
#define Packet_Max_SIZE 512
#define CounterNUM 10


typedef struct {
    uint32_t file_size;
    uint32_t file_crc; // Overall file CRC
    char filename[MAX_BUFFER_SIZE];
} Fileinfo;

typedef struct {
    uint32_t packet_id;
    uint32_t packet_crc; // CRC of this packet
    unsigned char data[Packet_Max_SIZE];
} DataPacket;




int main() {

    int sockfd = {0};
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    char OperationNAME[9]= {0};
    char FilePATH[256]= {0};
    

    // Ask for user to input Request
    printf("\nPlease type: <OperationNAME> <FilePATH\n");
    printf("Note* FilePATH caculation start from server file location\n");
    scanf("%s %s",OperationNAME,FilePATH);
    printf("\nOperationNAME: %s\nFilePATH: %s\n\n",OperationNAME,FilePATH);


    // Check argument Error - Typo and missing file while uploading
    if (strcasecmp(OperationNAME, "delete") != 0  
        && strcasecmp(OperationNAME, "download") != 0 
        && strcasecmp(OperationNAME, "upload") != 0) {

        perror("Wrong input...\n");
        exit(EXIT_FAILURE);
    }
    if (access(FilePATH, F_OK) != 0 && strcasecmp(OperationNAME, "upload") == 0) {
        fprintf(stderr, "File '%s' not found.\n\n", FilePATH);
        exit(EXIT_FAILURE);
    }
    if (access(FilePATH, R_OK) != 0 && strcasecmp(OperationNAME, "upload") == 0) {
        fprintf(stderr, "File '%s' doesn't have read premission.\n\n", FilePATH);
        exit(EXIT_FAILURE);
    }




    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error while creating socket\n");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server address\n");
        exit(EXIT_FAILURE);
    }
    


    
    // Create a massage to server based on argument
    size_t Req_message_size;
    char *REQ_msg = REQ_msg_build(OperationNAME,FilePATH,&Req_message_size);

    if (REQ_msg != NULL) {
        printf("Request Message (%zu bytes):\n", Req_message_size);
        for (size_t i = 0; i < Req_message_size; i++) {
            printf("%02X ", REQ_msg[i]);
        }
        printf("\n\n");
    }

 
    // Send Request_msg to server
    sendto(sockfd, REQ_msg, Req_message_size, 0,
          (const struct sockaddr *)&server_addr, sizeof(server_addr));

    printf("Request sent to server. Waiting for response....\n\n");

    free(REQ_msg); // Freeing allocated memory used
    




    /* Response Handler - DELETE || UPLOAD || DOWNLOAD:
            * DELETE - Awaiting and Reciving server detailed Response print the info and end client.
            * DOWNLOAD - 
            * UPLOAD - 
     */ 

        // DELETE //
    if (strcmp(OperationNAME,"delete") == 0){ 
        int Server_Response = 0, count = 0; // initilazied Loop condition
        char DEL_msg_buffer[MAX_BUFFER_SIZE] = {0};

        // Reciving bytes from server
        while (Server_Response <=0 && count < CounterNUM){
            
            Server_Response = recvfrom(sockfd, DEL_msg_buffer, MAX_BUFFER_SIZE, 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);
            count++;
        }
        // Check TIMEOUT for Response from server
        if (count == CounterNUM)
        {
            printf("\nSomthing went wrong getting feedback from server");
            exit (EXIT_FAILURE);

        }else { // Incase for incoming bytes print Response
            printf("\nServer Response: '%s'\n", DEL_msg_buffer);
            printf("\nClient: Got Feedback, Request finshed and client will close\n\n");
            exit (EXIT_SUCCESS);
        }
    }


        // UPLOAD //
    if (strcmp(OperationNAME,"upload") == 0){
        int Server_Response = 0, count = 0; // initilazied Loop condition
        char ACK_Resp_buf[4] = {0};
        
        // Reciving bytes from server
        while (Server_Response <=0 && count < CounterNUM){

            Server_Response = recvfrom(sockfd, ACK_Resp_buf, sizeof(int32_t), 0,
                                      (struct sockaddr *)&server_addr, &server_addr_len);
            count++;
        }
        // Check TIMEOUT for Response from server
        if (count == CounterNUM){ 
            printf("\nSomthing went wrong getting feedback from server");
            exit (EXIT_FAILURE);

        }else { // Incase for incoming bytes print Response
            printf("\nServer Response:\n");
            for (int i = 0; i < Server_Response; i++) {
                printf("%02X ", ACK_Resp_buf[i]);
            }
            printf("\n\n");

            // Checking for correct ACK Response bytes
            if (CompareResponseTOExpectedACK(Server_Response, ACK_Resp_buf) == 1){
                printf("\nClient: Correct ACK, Initiating UPLOAD\n\n");
                
            }else{
                printf("\nClient: Unknown Response, Request finsihed and client will close\n\n");
                exit (EXIT_FAILURE);
            }
        }
        
    }

    
}