// RDMA client
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <memory>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

const int MSGSIZ = 128;

int main(int argc, char* argv[]){
	int ret;

	// --------------- Setup Phase ---------------
	// Obtain and convert addressing information
	char* IP = argv[1];        
	char* port = argv[2];     
	struct rdma_addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_port_space = RDMA_PS_TCP;
	struct rdma_addrinfo* res;
	if ((ret = rdma_getaddrinfo(IP, port, &hints, &res)))
		perror("rdma_getaddrinfo");
	assert(res);

	// Create and configure local endpoints for communication
	struct ibv_qp_init_attr qp_init_attr;
	struct rdma_cm_id* id;
	// struct ibv_pd pd;
	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	qp_init_attr.cap.max_send_wr = 1;
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;
	qp_init_attr.cap.max_inline_data = MSGSIZ;
	qp_init_attr.qp_context = id;
	qp_init_attr.sq_sig_all = true;
	if ((ret = rdma_create_ep(&id, res, NULL, &qp_init_attr)))
		perror("rdma_create_ep");
	assert(id);

	// Setup local memory to be used in transfer
	void* msg = malloc(MSGSIZ);
	struct ibv_mr* mr = rdma_reg_msgs(id, msg, MSGSIZ);
	assert(mr);

	// Establish the connection with the remote side
	// struct rdma_conn_param conn_param;
	if ((ret = rdma_connect(id, NULL)))
		perror("rdma_connect");

	// --------------- Use Phase ---------------
	// Actually transfer data to/from the remote side
	struct ibv_wc wc;
	sprintf((char*)msg, "Hello, world");
	printf("Message sent: %s\n", (char*)msg);

	if ((ret = rdma_post_send(id, NULL, msg, MSGSIZ, mr, 0)))
		perror("rdma_post_send");
	while ((ret = ibv_poll_cq(id->send_cq, 1, &wc)) == 0)
		/* Waiting */ ;
	if (ret < 0) perror("ibv_poll_cq");
	if ((ret = rdma_post_recv(id, NULL, msg, MSGSIZ, mr)))
		perror("rdma_post_recv");
	while ((ret = ibv_poll_cq(id->recv_cq, 1, &wc)) == 0)
		/* Waiting */ ;
	if (ret < 0) perror("ibv_poll_cq");
	printf("Message received: %s\n", (char*)msg);

	// ------------ Break-down Phase ------------
	// Close connection, free memory and communication resources
	if ((ret = rdma_disconnect(id)))
		perror("rdma_disconnect");
	if ((ret = rdma_dereg_mr(mr)))
		perror("rdma_dereg_mr");
	free(msg);
	rdma_freeaddrinfo(res);

	return 0;
}
