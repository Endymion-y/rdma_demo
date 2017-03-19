// RDMA server
#include <iostream>
#include <memory>
#include <cstdio>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

const int MSGSIZ = 1024;

int main(){
	int ret;

	// --------------- Setup Phase ---------------
	// Obtain and convert addressing information
	char* node = NULL;
	char* service = argv[1];
	struct rdma_addrinfo hints;
	struct rdma_addrinfo* res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = RAI_PASSIVE;          // Server
	hints.ai_port_space = RDMA_PS_TCP;
	ret = rdma_getaddrinfo(node, service, &hints, &res);

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
	ret = rdma_create_ep(&id, res, NULL, &qp_init_attr);

	// Establish socket as listener
	ret = rdma_listen(id, 0);

	// Loop
	while (true){
		struct rdma_cm_id* conn_id;
		// Get connection request from client
		rdma_get_request(id, &conn_id);

		// ------------- Agent Setup -------------
		// Allocate memory and register
		void* msg = malloc(MSGSIZ);
		struct ibv_mr* mr = rdma_reg_msgs(conn_id, mem, MSGSIZ);
		// Define properties of new connection
		struct rdma_conn_param conn_param;
		// Finalize connection with client
		rdma_accept(conn_id, NULL);

		// ------------- Agent Use ---------------
		struct ibv_wc wc;      // CQE array with length 1
		// Post receive
		ret = rdma_post_recv(conn_id, NULL, msg, MSGSIZ, mr);
		// Wait to receive ping data
		while ((ret = ibv_poll_cq(conn_id->recv_cq, 1, &wc)) <= 0)
			/* Waiting */ ;
		// Post send
		ret = rdma_post_send(conn_id, NULL, msg, MSGSIZ, mr, 0);
		// Wait for send to complete
		while ((ret = ibv_poll_cq(conn_id->send_cq, 1, &wc)) <= 0)
			/* Waiting */ ;

		// -------- Agent Break-down -------------
		// Break connection
		rdma_disconnect(conn_id);
		// Deregister user virtual memory
		rdma_dereg_mr(mr);
		// Free memory
		free(msg);
		// Destroy local end point
		rdma_destroy_ep(conn_id);
	}

	// ------------ Break-down Phase ------------
	// Close connection, free memory and communication resources
	ret = rdma_disconnect(id);
	rdma_freeaddrinfo(res);

	return 0;
}