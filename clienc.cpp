// RDMA client
#include <iostream>
#include <cstdio>

int main(int argc, char* argv[]){
	int ret;

	// --------------- Setup Phase ---------------
	// Obtain and convert addressing information
	char* node = argv[1];        // IP
	char* service = argv[2];     // Port
	struct rdma_addrinfo hints = {};
	struct rdma_addrinfo* res;
	ret = rdma_getaddrinfo(node, service, &hints, &res);

	// Create and configure local endpoints for communication
	// struct ibv_qp_init_attr qp_init_attr;
	struct rdma_cm_id* id;
	// struct ibv_pd pd;
	ret = rdma_create_ep(&id, res, NULL, NULL);

	// Setup local memory to be used in transfer
	int MSGSIZ = 1024;
	void* msg = malloc(MSGSIZ);
	struct ibv_mr* mr = rdma_reg_msgs(id, mem, MSGSIZ);

	// Establish the connection with the remote side
	struct rdma_conn_param conn_param;
	ret = rdma_connect(id, &conn_param);

	// --------------- Use Phase ---------------
	// Actually transfer data to/from the remote side
	ret = rdma_post_send(id, NULL, msg, MSGSIZ, mr, 0);
	ibv_poll_cq();
	ret = rdma_post_recv(id, NULL, msg, MSGSIZ, mr);
	ibv_poll_cq();

	// ------------ Break-down Phase ------------
	// Close connection, free memory and communication resources
	ret = rdma_disconnect(id);
	ret = rdma_dereg_mr(mr);
	rdma_freeaddrinfo(res);

	return 0;
}