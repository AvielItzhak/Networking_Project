#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include "udp_file_transfer.h"


#define SERVER_IP "127.0.0.1" // Loopback
#define PORT 55555
#define DOWNLOAD_Folder_NAME "./DownloadedFiles"





int main() {

    int sockfd = {0};
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    Requestinfo Request = {0};
    

    // Ask for user to input Request
    printf("\nPlease type: <OperationNAME> <FilePATH\n");
    printf("Note* FilePATH calculation start from server file location\n");
    scanf("%s %s",Request.OperationNAME,Request.FilePATH);
    printf("\nOperationNAME: %s\nFilePATH: %s\n\n",Request.OperationNAME,Request.FilePATH);


    // Getting File NAME
    char* last_slash = strrchr(Request.FilePATH, '/'); // pointing to the last location of '/' in FilePATH
    if (last_slash != NULL) // Initial input given was a PATH
        {memcpy(Request.FileNAME, last_slash + 1, sizeof(Request.FilePATH - (last_slash + 1)));} 

    else// Initial input given was a File NAME
        {memcpy(Request.FileNAME, Request.FilePATH, sizeof(Request.FileNAME));}


    // Check argument Error - Typo and missing file while uploading
    CheckArgError (Request.OperationNAME, Request.FilePATH);


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
        ,&Req_message_size); // In case of UPLOAD Operation send only FileNAME

    if (REQ_msg != NULL) { // Printing Request message in bytes for debug
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
    
    // Defining Timeout for connection and Setin it as OFF
    struct timeval tv;
    tv.tv_sec = 0; 
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));



    /* Response Handler - DELETE || UPLOAD || DOWNLOAD:
            * DELETE - Awaiting and Receiving server detailed Response print the info and end client.
            * UPLOAD - Awaiting ACK and initiating UPLOAD sequence - Send packet, check Response
            * DOWNLOAD - Sending ACK to initiate DOWNLOAD sequence - get packet, send Response
     */ 

        // DELETE Operation Handler section //

    if (strcasecmp(Request.OperationNAME,"delete") == 0) 
    {
        int Server_Response = 0; // initialized Loop condition
        char DEL_msg_buffer[MAX_BUFFER_SIZE] = {0};

        // Setin Timeout ON 
        tv.tv_sec = TimeoutValue; 
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));


        // Receiving bytes from server
        Server_Response = recvfrom(sockfd, DEL_msg_buffer, MAX_BUFFER_SIZE, 0,
                                    (struct sockaddr *)&server_addr, &server_addr_len);

        // Check ERROR and Timeout
        if (Server_Response < 4)
        {
            // Check Timeout ERROR
            if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                perror("\nTimeout occurred while waiting for data");
                printf("\nEnding Transfer session now due to timeout\n\n");
                exit (errno); 
            }
            else // Other ERRORS
            {
                perror("Error Receiving message\n"); // Print Error detail in server terminal
                printf("\nClient: *Didn't* Got Feedback, Request finished and client will close\n\n");
                exit (errno);
            }    
        }    
        else  // Incase for incoming bytes print Response
        {    
            printf("\nServer Response: '%s'\n", DEL_msg_buffer);
            printf("\nClient: Got Feedback, Request finished and client will close\n\n");
            exit (EXIT_SUCCESS);
        }
    } // END of DELETE Handler


    // UPLOAD Operation Handler section //

    if (strcasecmp(Request.OperationNAME,"upload") == 0)
    {
        // Setin ON Timeout
        tv.tv_sec = TimeoutValue; 
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        // Handleing Server Response for initial UPLOAD Request
        ResponseHandleACK(RetryCount ,sockfd, server_addr,  server_addr_len, 0); // RetryCount -> Timeout will exit

        
        // Creating a DATA packet from reading file and sendin it to server
        FILE* Fpoint = NULL; // Iterator traverse file
        u_int16_t Seq_NUM = 1; // Initial packet number
        size_t last_DATApack_size = 0; // helper var
        size_t packet_buf_size = 0; // full data message size

        Fpoint = fopen(Request.FilePATH,"r"); // Creating a pointer file to scan the intended file

        // Main UPLOAD to server Transfer loop
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
                        {memset(Dpack.data, 0, Packet_Max_SIZE - 4);} // Send server indication for ending transfer
                    else 
                        {free(Dpack.packet_buf); break;} // End transfer loop
                 
                } 
                else if (ferror(Fpoint)) { // Due to ERROR
                    printf("ERROR while reading from file.\n\n");
                    printf("\nClient: Couldn't Read FILE, Request finished and client will close\n\n");
                    exit(errno); // End client
                }
            }

            // Building DATA packet message
            Dpack.packet_buf = DATApack_Build(Dpack.data_size, Dpack.data, Dpack.packet_id);
            packet_buf_size = Dpack.data_size + 4;

            int count = 0;
            while (1) // ACK Check LOOP and Retransmission previous packet 
            {
                // Sending packet to server
                sendto(sockfd, Dpack.packet_buf, packet_buf_size, 0,
                    (const struct sockaddr *)&server_addr, sizeof(server_addr));

                // handleing Server Response for packet Transfer 
                printf("\nOP_ID & SeqNUM in bytes (%ld):  ",packet_buf_size);
                    for (size_t i = 0; i < 4; i++)
                        {printf("%02X ", Dpack.packet_buf[i]);}
                    printf("\n\n");

                if (ResponseHandleACK(count ,sockfd, server_addr,  server_addr_len, Dpack.packet_id))
                    {break;} // Only if received the last Seq_NUM NOT Reach here and keep looping
            }

            // Preparing variables for next iteration - Freeing allocated memory
            free(Dpack.packet_buf);
            Seq_NUM++;
            last_DATApack_size = Dpack.data_size;

        }
        
        fclose(Fpoint);

        // If reach here the UPLOAD was successful and client will close
        printf ("\n\nUPLOAD Request completed\nclient will now close.\n\n");
        exit (EXIT_SUCCESS);
       
    }// END of UPLOAD Handler


     // DOWNLOAD Operation Handler section //

    if (strcasecmp(Request.OperationNAME,"download") == 0)
    {
        char ErrorHandler[256] = {0}; // Error string initialization
        unsigned char ACK_Response[4] = {0};
        unsigned char buffer[MAX_BUFFER_SIZE] = {0};

        // In case DOWNLOAD folder doesn't exist create it
            if (access(DOWNLOAD_Folder_NAME, F_OK) != 0){
                if (mkdir(DOWNLOAD_Folder_NAME,0755) == 0) {
                    printf("Directory '%s' created successfully.\n", DOWNLOAD_Folder_NAME);
                }else {
                    perror("Error creating DOWNLOAD directory\n");
                    exit(errno); // Exit client
                }
            }

            // Creating a new FILE with the given name
            size_t FilePATHinClient_len = strlen(DOWNLOAD_Folder_NAME) + 1 + sizeof(Request.FileNAME) + 1;
            char FilePATHinClient[MAX_BUFFER_SIZE] = {0} ;
            FILE* Fpoint; // iterator traverse file

            snprintf(FilePATHinClient, FilePATHinClient_len, "%s/%s", DOWNLOAD_Folder_NAME, Request.FileNAME);
            printf("%s",FilePATHinClient);

            // handles file with same name in client download folder and assign different name
            while (access(FilePATHinClient, F_OK) == 0) 
                  {strcat(FilePATHinClient, "_copy");}

            
            // Creating a pointer file to scan the intended file
            Fpoint = fopen(FilePATHinClient,"ab"); 
            

            // Building ACK Response and sending to server to start transfer DATA
            ACK_Build_send(sockfd, server_addr, server_addr_len,ACK_Response, 0);
            printf("\nResponse sent to server\nAwaiting Data packets.....\n\n");
            u_int16_t Last_SeqNUM = 0, Cur_SeqNUM = 0; // define and last SeqNUM in vars for the transfer loop 



            // Set TIMEOUT for connection
            tv.tv_sec = TimeoutValue; 
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

            int count = 0;
            // receiving packet and sending ACK loop
            while (1) 
            {
                int Req_msg_rec = 0;// initialization

                // receiving bytes from server
                Req_msg_rec = recvfrom(sockfd, buffer, Packet_Max_SIZE, 0,
                                    (struct sockaddr *)&server_addr, &server_addr_len);
                
                if (Req_msg_rec < 4) {
                    // Check Timeout ERROR
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { 
                        perror("\nTimeout occurred while waiting for data\n");

                        if (count < RetryCount) // Didn't Reach Max Retrys -> Send again ACK Response
                        {
                            printf("\nRetransmit Last SeqNUM ACK Response\n\n");
                            count ++;
                        }
                        else // Reach Max Retrys -> Break Transfer loop -> EXIT
                        {
                            // In case of timeout midway --> Deleting partial file left in Upload folder
                            if (access(FilePATHinClient, F_OK) == 0) // Check if file exists
                            {
                                if (RequestHandler_Delete(FilePATHinClient, Request.OP_Detail) != 0) {
                                     printf("\nSomething went wrong trying deleting partial file...");
                                }
                                printf("%s\n\n", Request.OP_Detail);
                            } 
                            else // File not exist
                                {printf("\nPartial file %s not found.\n", FilePATHinClient);}

                            printf("\nEnding Transfer session now due to timeout\n\n");
                            break; // Break Transfer loop
                        }     
                    } 
                    else { // Another type of ERROR
                        perror("Error receiving message\n");
                        // Save Error detail in ErrorHandler and send it to server
                        sprintf(ErrorHandler, "Error receiving message: %s", strerror(errno));
                        sendto(sockfd, ErrorHandler, 256, 0, (const struct sockaddr *)&server_addr, server_addr_len);
                        printf("\nResponse sent to client\nListening to further Request...\n\n\n");
                        break;  // Break Transfer loop
                    }
                }                    

                if ( Req_msg_rec > 4 ) // Print message received in bytes
                {
                   // Printing Message
                   printf("DATA Message (%d bytes):\n", Req_msg_rec);
                   for (int i = 0; i < 4; i++) 
                       {printf("%02X ", buffer[i]);}
                   printf("\n\n");

                   // Checkin to see if given the right order packet
                   memcpy(&Cur_SeqNUM, buffer + 2, 2);
                   Cur_SeqNUM = ntohs(Cur_SeqNUM); 

                   if (Last_SeqNUM == Cur_SeqNUM - 1) // Only if the right order it will Write the DATA
                   {
                        // Copying DATA to File in the client - Removing the OP and SeqNUM bytes
                        fwrite((buffer + 4), 1, (Req_msg_rec - 4), Fpoint);
                        Last_SeqNUM = Cur_SeqNUM;
                   }   
                }
            
                if ( Req_msg_rec == 4 || Req_msg_rec != 512 ) { // Reached END OF FILE

                    // Building ACK Response and sending to server
                    ACK_Build_send(sockfd, server_addr, server_addr_len,ACK_Response, (u_int16_t)((buffer[2] << 8) | buffer[3]));
                    printf("\r\r\nEOF: DOWNLOAD completed successfully\nResponse sent to server\n\n\n");
                    break;
                }

                // If reach here in the loop: (the order of the if condition is relevant)
                // Building ACK Response and sending to server
                ACK_Build_send(sockfd, server_addr, server_addr_len,ACK_Response, (u_int16_t)((buffer[2] << 8) | buffer[3]));
                printf("\nResponse sent to server\nAwaiting further Data packets.....\n\n");

            } // End of Transfer and receiving packet and ACK loop

            fclose(Fpoint);  

        }//END of DOWNLOAD Handler section 

    return 0;
}// END of main