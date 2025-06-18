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
    char OperationNAME[9];
    char FilePATH[256];
    char FileNAME[128];
} Requestinfo;

typedef struct {
    int32_t packet_id;
    int32_t packet_crc; // CRC of this packet
    char data[Packet_Max_SIZE];
    size_t data_size;
    char *packet_buf;
} DataPacket;




int main() {

    int sockfd = {0};
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    Requestinfo Request = {0};
    

    // Ask for user to input Request
    printf("\nPlease type: <OperationNAME> <FilePATH\n");
    printf("Note* FilePATH caculation start from server file location\n");
    scanf("%s %s",Request.OperationNAME,Request.FilePATH);
    printf("\nOperationNAME: %s\nFilePATH: %s\n\n",Request.OperationNAME,Request.FilePATH);

    // Getting File NAME
    char* last_slash = strrchr(Request.FilePATH, '/'); // pointing to the last location of '/' in FilePATH
    if (last_slash != NULL) // Initial input given was a PATH
        {memcpy(Request.FileNAME, last_slash + 1, sizeof(Request.FilePATH - (last_slash + 1)));} 

    else// Initial input given was a File NAME
        {memcpy(Request.FileNAME, Request.FilePATH, sizeof(Request.FileNAME));}


    // Check argument Error - Typo and missing file while uploading
    if (strcasecmp(Request.OperationNAME, "delete") != 0  
        && strcasecmp(Request.OperationNAME, "download") != 0 
        && strcasecmp(Request.OperationNAME, "upload") != 0) {

        perror("Wrong input...\n");
        exit(EXIT_FAILURE);
    }
    if (access(Request.FilePATH, F_OK) != 0 && strcasecmp(Request.OperationNAME, "upload") == 0) {
        fprintf(stderr, "File '%s' not found.\n\n", Request.FilePATH);
        exit(EXIT_FAILURE);
    }
    if (access(Request.FilePATH, R_OK) != 0 && strcasecmp(Request.OperationNAME, "upload") == 0) {
        fprintf(stderr, "File '%s' doesn't have read premission.\n\n", Request.FilePATH);
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

    char *REQ_msg = REQ_msg_build(Request.OperationNAME
        ,(strcasecmp(Request.OperationNAME,"upload") == 0) ? Request.FileNAME : Request.FilePATH 
        ,&Req_message_size); // In the case of UPLOAD send only FileNAME

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
            * UPLOAD - 
            * DOWNLOAD - 

     */ 

        // DELETE Operation Handler section //
    if (strcasecmp(Request.OperationNAME,"delete") == 0) 
    {
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

        }
        else  // Incase for incoming bytes print Response
        {    
            printf("\nServer Response: '%s'\n", DEL_msg_buffer);
            printf("\nClient: Got Feedback, Request finshed and client will close\n\n");
            exit (EXIT_SUCCESS);
        }
    } // END of DELETE Handler


        // UPLOAD Operation Handler section //
    if (strcasecmp(Request.OperationNAME,"upload") == 0)
    {
        // Handeling Server Response for intial UPLOAD Request
        ServerResponseHandleACK(sockfd, server_addr,  server_addr_len, 0);

        
        // Creating a DATA packet from reading file and send it to server
        FILE* Fpoint = NULL; // Itreator traverse file
        int16_t Seq_NUM = 1; // Initial packet number
        size_t last_DATApack_size = 0; // helper var
        size_t packet_buf_size = 0; // full data message size

        Fpoint = fopen(Request.FilePATH,"r"); // creating a poineter file to scan the intented file

        // nain UPLOAD to server Transfer loop
        while (1) 
        {
            DataPacket Dpack = {0}; // Create a struct for packet
            Dpack.packet_id = Seq_NUM;
            
            // Filling up a container to up to a total max of 512(-4) bytes
            Dpack.data_size = fread(Dpack.data, 1, Packet_Max_SIZE-4, Fpoint);


            // Check for EOF - Condition to end loop
            if (Dpack.data_size == 0) {
                 
                if (feof(Fpoint)) { // Due to EOF
                    printf("\nReached END OF FILE.\n");
                    if (last_DATApack_size == 508) // special case
                        {memset(Dpack.data, 0, Packet_Max_SIZE - 4);;} // give server indication for ending transfer
                    else 
                        {free(Dpack.packet_buf); break;} // End transfer loop
                 
                } 
                else if (ferror(Fpoint)) { // Due to ERROR
                    printf("ERROR while reading from file.\n\n");
                    break; //****ERRROr HANDLER */
                }
            }

            // Building DATA packet message
            Dpack.packet_buf = DATApack_Build(Dpack.data_size, Dpack.data, Dpack.packet_id);
            packet_buf_size = Dpack.data_size + 4;

            // Sending packet to server
            sendto(sockfd, Dpack.packet_buf, packet_buf_size, 0,
                (const struct sockaddr *)&server_addr, sizeof(server_addr));

            // Handeling Server Response for packet Transfer 
            printf("\nOPID & SeqNUM in bytes (%ld):  ",packet_buf_size);
                for (size_t i = 0; i < 4; i++)
                    {printf("%02X ", Dpack.packet_buf[i]);}
                printf("\n\n");

            ServerResponseHandleACK(sockfd, server_addr,  server_addr_len, Dpack.packet_id);
            
            // Preparing variables for next itertion - Freeing allocated memory
            free(Dpack.packet_buf);
            Seq_NUM++;
            last_DATApack_size = Dpack.data_size;

        }
        
        fclose(Fpoint);

        // If reach here the UPLOAD was successfull and client will close
        printf ("\n\nUPLOAD Request completed\nclient will now close.\n\n");
        exit (EXIT_SUCCESS);
       
    }// END of UPLOAD Handler

    // DOWNLOAD


    return 0;
}// END of main