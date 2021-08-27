/*

Client

 Description: This program tests 5 transmissions from client to server based on UDP protocol.
              The transmission are designed in the followed format:
                 Case 1: Out of sequence packet was sent -Order goes 1, 2, 5, 4, 3
                 Case 2: Length mismatch error-The 3rd packet's length field is set to 0x04; required length is 0xFF
                 Case 3: End of packet ID is wrong- End of packet id for the 3rd packet is 0xFF11, but 0xFFFF required
                 Case 4: Duplicated packet-Order goes 1, 2, 3, 4, 3
                 Case 0 or any other number: Normal Transmission-Order goes 1, 2, 3, 4,5
             If nothing or not digital is entered, case 4 is the default case.
             If server does not respond, client will retransmit the message for 2 more times.
             If server port has been used, you can change it to an available port number on your local.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <zconf.h>
#include <arpa/inet.h>
#include <poll.h>
#include "data1.h"
#include "client1.h"


int main(void)
{
    int demos = 4; // total transmissions
    printf("Please enter the CASE NUMBER to simulate (Please enter 0, 1, 2, 3, 4):    ");
    scanf("%d",&demos);

    struct sockaddr_in local_addr, remote_addr; // we need to send messages to remote_addr
    int socket_desk, i = 0, send_try = 1, poll_result;
    // socket, index for create packet,  times of send if no response from server, result from poll,
    int remote_addr_len=sizeof(remote_addr), recv_len;
    // remote address length, ACK message bytes
    char *server = "127.0.0.1";    // try other server that work on your local if this does not work.
    ret_packet response;
    data_packet data;

    //Create socket
    socket_desk=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_desk < 0){
        printf("Socket create Error\n");
        return -1;
    }
    printf("Socket created successfully!\n");

    //Set port and IP
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(0);

    //if bind failed, print error
    if (bind(socket_desk, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Client: bind failed");
        return -1;
    }
    printf("Socket bind successfully!\n");

    // define remote_addr
    memset((char *) &remote_addr, 0, sizeof(remote_addr)); //initial to 0
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(SERVICE_PORT);
    if (inet_aton(server, &remote_addr.sin_addr)==0) {
        fprintf(stderr, "Client: inet_aton() failed\n");
        exit(1);
    } //the host address is expressed as a numeric IP address, will convert that to a binary format

    struct pollfd pfd;  //for ack_timer
    pfd.fd = socket_desk;
    pfd.events = POLLIN;

    data_packet datas[PACKETS];
    produce_packets(datas);//produce packets

    data_packet temp;
    switch(demos){  // 4 cases of errors for demo
        case 1: //Out of sequence error
            temp = datas[2];
            datas[2] = datas[4];
            datas[4] = temp;
            printf(">>>>>>>>>>In this case, datas are transited with packets out of order.\n"
                           ">>>>>>>>>>Effects are as follow: \n\n");
            break;
        case 2: //Length mismatch error
            datas[2].length = 0x04; // length should be the length of payload
            printf(">>>>>>>>>>In this case,Packet's length mismatch \n"
                           ">>>>>>>>>>Effects are as follow: \n\n");
            break;
        case 3: //End of packet id missing error
            datas[2].end_id = 0xFF11;
            printf(">>>>>>>>>>In this case, packet with wrong End of packet id was sent \n"
                           ">>>>>>>>>>Effects are as follow: \n\n");
            break;
        case 4: //Duplicate Packet error
            datas[4] = datas[2];
            printf(">>>>>>>>>>In this case, duplicate packet is transmitted\n"
                           ">>>>>>>>>>Effects are as follow: \n\n");
            break;
        default:
            printf(">>>>>>>>>>In this case, normal transmission is sent. \n"
                           ">>>>>>>>>>Effects are as follow: \n\n");
    }


    while (i < PACKETS) { //sends messages and waits for feedback from server
        data = datas[i];
        printf("Client: Sending packet %d to Server %s port %d. Try 1 out of 3 times\n", i+1, server, SERVICE_PORT);

        if (sendto(socket_desk, &data, sizeof(data_packet), 0, (struct sockaddr *)&remote_addr, remote_addr_len)==-1) {
            perror("Client: Sendto error:\n");
            exit(1);
        }

        send_try = 1;
        //ack_timer
        while(send_try < 3){//poll for 3 seconds each time, and resend up to 2 times
            poll_result = poll(&pfd,1,3000);

            if(poll_result == 0) {//timeout, resend
                send_try++;
                printf("Client: No response. Retransmitting. Try %d out of 3\n", send_try);
                if (sendto(socket_desk, &data, sizeof(data_packet), 0, (struct sockaddr *)&remote_addr, remote_addr_len) < 0) {
                    perror("Client: Sendto error:\n");
                    exit(1);
                }
            }
            else if(poll_result == -1) {
                //error
                perror("Client: Poll error:\n");
                return poll_result;

            }
            else {
                //receive response from the server
                recv_len = recvfrom(socket_desk, &response, sizeof(ret_packet), 0, (struct sockaddr *)&remote_addr, &remote_addr_len);
                if(recv_len < 0){
                    perror("Client: Receive error:\n");
                    exit(1);
                }

                if(response.type == (short)ACK){ //ACK received!
                    printf("Server: ACK. Packet %d acknowledged.\n\n", i+1);
                    break;
                }
                else if(response.type == (short)REJ){ //Error received! determine what error occured
                    if(response.rej_sub == (short)REJ_SUB1){
                        printf("Server: ERROR. Out of Sequence. Was expecting seg_num %d but received seg_num %d.\n\n", i+1, response.seg_num_rec);
                        return -1;
                    }
                    else if(response.rej_sub == (short)REJ_SUB2){
                        printf("Server: ERROR. Packet %d has a length mismatch.\n\n", i+1);
                        return -1;
                    }
                    else if(response.rej_sub == (short)REJ_SUB3){
                        printf("Server: ERROR. Packet %d, missing end of packet id.\n\n", i+1);
                        return -1;
                    }
                    else if(response.rej_sub == (short)REJ_SUB4){
                        printf("Server: ERROR. Duplicate packets were sent. Was expecting seg_num %d but received seg_num %d again.\n\n", i+1, response.seg_num_rec);
                        return -1;
                    }

                }
                else{ //Neither ACK nor REJ was received.
                    printf("Client: Unknown response packet received.\n");
                    return -1;
                }
            }
        }

        if(send_try >= 3){ // time out error, break out of transmission loop
            break;
        }
        else  //packet was sent and acknowledged, go to the next packet.
            i++;
    }
    close(socket_desk); // packets was sent

    if(send_try < 3){
        printf("Client: Finished all with no error\n\n");
        return 0;
    }
    else{ // while loop broken due to time-out error
        printf("Client: No response. Time-out Error.\nClient: After 3 transmission sent, packet %d was not acknowledged .\nServer does not respond.\nExiting. . . \n\n",i);
        return -1;
    }
}

// initial the data packet.
void produce_packets(data_packet datas[]) {
    char client_id = get_client_id();
    printf("Client ID: %d\n", client_id & 0xff);
    char buffer[BUF_SIZE];   //buffer for transmission
    for (int i=0; i<PACKETS; i++){
        sprintf(buffer, "Client: packet #%d", i+1);
        datas[i].client_id= client_id;
        datas[i].start_id = START_ID;
        datas[i].end_id = END_ID;
        datas[i].data = DATA;
        datas[i].seg_num = i+1;
        strncpy(datas[i].payload, buffer, 255);
        datas[i].length = sizeof(datas[i].payload);
    }
}
