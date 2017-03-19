// RDMA server
#include <iostream>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

const int MSGSIZ = 128;

int main(int argc, char* argv[]){
	int ret;

	// --------------- Setup Phase ---------------
	// Obtain and convert addressing information
	char* IP = NULL;
	char* port = argv[1];
	struct rdma_addrinfo hints;
	struct rdma_addrinfo* res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = RAI_PASSIVE;          // Server
	hints.ai_port_space = RDMA_PS_TCP;
	if ((ret = rdma_getaddrinfo(IP, port, &hints, &res)))
		perror("rdma_getaddrinfo");

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
	qp_init_attr.sq_sig_all = 1;       // Whether send generates CQE
	if ((ret = rdma_create_ep(&id, res, NULL, &qp_init_attr)))
		perror("rdma_create_ep");

	// Establish socket as listener
	if ((ret = rdma_listen(id, 0)))
		perror("rdma_listen");

	// Loop
	while (true){
		struct rdma_cm_id* conn_id;
		// Get connection request from client
		if ((ret = rdma_get_request(id, &conn_id)))
			perror("rdma_get_request");

		// ------------- Agent Setup -------------
		// Allocate memory and register
		void* msg = malloc(MSGSIZ);
		struct ibv_mr* mr = rdma_reg_msgs(conn_id, msg, MSGSIZ);
		assert(mr);
		// Define properties of new connection
		// struct rdma_conn_param conn_param;
		// Finalize connection with client
		if ((ret = rdma_accept(conn_id, NULL)))
			perror("rdma_accept");

		// ------------- Agent Use ---------------
		struct ibv_wc wc;      // CQE array with length 1
		// Post receive
		if ((ret = rdma_post_recv(conn_id, NULL, msg, MSGSIZ, mr)))
			perror("rdma_post_recv");
		// Wait to receive ping data
		while ((ret = ibv_poll_cq(conn_id->recv_cq, 1, &wc)) == 0)
			/* Waiting */ ;
		if (ret < 0) perror("ibv_poll_cq");
		// Post send
		if ((ret = rdma_post_send(conn_id, NULL, msg, MSGSIZ, mr, 0)))
			perror("rdma_post_send");
		// Wait for send to complete
		while ((ret = ibv_poll_cq(conn_id->send_cq, 1, &wc)) == 0)
			/* Waiting */ ;
		if (ret < 0) perror("ibv_poll_cq");

		// -------- Agent Break-down -------------
		// Break connection
		if ((ret = rdma_disconnect(conn_id)))
			perror("rdma_disconnect");
		// Deregister user virtual memory
		if ((ret = rdma_dereg_mr(mr)))
			perror("rdma_dereg_mr");
		// Free memory
		free(msg);
		// Destroy local end point
		if ((ret = rdma_destroy_ep(conn_id)))
			perror("rdma_destroy_ep");
	}

	// ------------ Break-down Phase ------------
	// Close connection, free memory and communication resources
	if ((ret = rdma_disconnect(id)))
		perror("rdma_disconnect");
	rdma_freeaddrinfo(res);

	return 0;
}
