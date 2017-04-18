// RDMA server
#include <iostream>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <string>
#include <thread>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
using namespace std;

const int MSGSIZ = 128;

struct context {
	struct rdma_cm_id* id;
	struct ibv_mr* mr;
};

int main(int argc, char* argv[]){
	if (argc != 2){
		cout << "Usage: ./server port" << endl;
		return 0;
	}
	int ret;

	// Obtain and convert addressing information
	// char* IP = NULL;
	// char* port = argv[1];
	// struct rdma_addrinfo hints;
	// struct rdma_addrinfo* res;

	// memset(&hints, 0, sizeof(hints));
	// hints.ai_flags = RAI_PASSIVE;          // Server
	// hints.ai_port_space = RDMA_PS_TCP;
	// if ((ret = rdma_getaddrinfo(IP, port, &hints, &res)) < 0){
	// 	perror("rdma_getaddrinfo");
	// 	exit(-1);
	// }
	struct rdma_event_channel* cm_channel = rdma_create_event_channel();
	if (!cm_channel){
		perror("rdma_create_event_channel");
		exit(-1);
	}

	struct rdma_cm_id* listen_id;
	if ((ret = rdma_create_id(cm_channel, &listen_id, NULL, RDMA_PS_TCP)) < 0){
		perror("rdma_create_id");
		exit(-1);
	}	

	// Create and configure local endpoints for communication
	struct ibv_qp_init_attr qp_init_attr;
	// struct rdma_cm_id* id;
	// struct ibv_pd pd;

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(50051);
	sin.sin_addr.s_addr = INADDR_ANY;

	// Bind to local port and listen for connection request
	if ((ret = rdma_bind_addr(listen_id, (struct sockaddr*)&sin)) < 0){
		perror("rdma_bind_addr");
		exit(-1);
	}

	// Create PD
	struct ibv_pd* pd = ibv_alloc_pd(listen_id->verbs);
	if (!pd){
		perror("ibv_alloc_pd");
		exit(-1);
	}

	// Create completion channel and completion queue
	struct ibv_comp_channel* comp_chan = ibv_create_comp_channel(listen_id->verbs);
	if (!comp_chan) {
		perror("ibv_create_comp_channel");
		exit(-1);
	}
	struct ibv_cq* cq = ibv_create_cq(listen_id->verbs, 2, NULL, comp_chan, 0);
	if (!cq) {
		perror("ibv_create_cq");
		exit(-1);
	}
	if ((ret = ibv_req_notify_cq(cq, 0)) < 0) {
		perror("ibv_req_notify_cq");
		exit(-1);
	}

	// memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	// qp_init_attr.cap.max_send_wr = 1;
	// qp_init_attr.cap.max_recv_wr = 1;
	// qp_init_attr.cap.max_send_sge = 1;
	// qp_init_attr.cap.max_recv_sge = 1;
	// qp_init_attr.cap.max_inline_data = MSGSIZ;
	// qp_init_attr.sq_sig_all = 1;       // Whether send generates CQE
	// if ((ret = rdma_create_ep(&id, res, NULL, &qp_init_attr)) < 0){
	// 	perror("rdma_create_ep");
	// 	exit(-1);
	// }
		

	// Establish socket as listener
	if ((ret = rdma_listen(listen_id, 1)) < 0){
		perror("rdma_listen");
		exit(-1);
	}

	// Start a CQ polling thread
	thread polling_thread([comp_chan, cq](){
		struct ibv_cq* evt_cq;
		void* cq_context;
		struct ibv_wc wc;
		while (true){
			// Get next event
			if ((ibv_get_cq_event(comp_chan, &evt_cq, &cq_context)) < 0){
				perror("ibv_get_cq_event");
				return;
			}
			// Acknowledge the event
			ibv_ack_cq_events(cq, 1);
			// Request notification again
			if ((ibv_req_notify_cq(cq, 0)) < 0){
				perror("ibv_req_notify_cq");
				return;
			}
			if (ibv_poll_cq(cq, 1, &wc) < 1){
				perror("ibv_poll_cq");
				return;
			}
			if (wc.status != IBV_WC_SUCCESS){
				perror("wc.status");
				return;
			}
			if (wc.opcode == IBV_WC_RECV){
				// Recv completed
				struct context* ctx = (struct context*)(wc.wr_id);
				struct rdma_cm_id* id = ctx->id;
				struct ibv_mr* mr = ctx->mr;
				// Send back
				if ((rdma_post_send(id, ctx, mr->addr, mr->length, mr, 0)) < 0){
					perror("rdma_post_send");
					exit(-1);
				}
			}
			else {
				// Send completed
				// Do nothing
			}
		}
	});

	// Loop to accept connections
	while (true){
		struct rdma_cm_id* conn_id;
		// Get connection request from client
		if ((ret = rdma_get_request(listen_id, &conn_id)) < 0){
			perror("rdma_get_request");
			exit(-1);
		}

		// Create QP
		memset(&qp_init_attr, 0, sizeof(qp_init_attr));
		qp_init_attr.cap.max_send_wr = 1;
		qp_init_attr.cap.max_recv_wr = 1;
		qp_init_attr.cap.max_send_sge = 1;
		qp_init_attr.cap.max_recv_sge = 1;
		qp_init_attr.cap.max_inline_data = MSGSIZ;
		qp_init_attr.sq_sig_all = 1;       // Whether send generates CQE
		// Bind to the only CQ
		qp_init_attr.send_cq = cq;
		qp_init_attr.recv_cq = cq;
		qp_init_attr.qp_type = IBV_QPT_RC;
		if ((ret = rdma_create_qp(conn_id, pd, &qp_init_attr)) < 0){
			perror("rdma_create_qp");
			exit(-1);
		}

		// Allocate and register memory
		void* msg = malloc(MSGSIZ);
		struct ibv_mr* mr = rdma_reg_msgs(conn_id, msg, MSGSIZ);
		if (!mr){
			perror("rdma_reg_msgs");
			exit(-1);
		}

		struct context* ctx = new context;
		ctx->id = conn_id;
		ctx->mr = mr;
		// Post a recv
		// Set the wr_id as the second parameter
		if ((rdma_post_recv(conn_id, ctx, msg, MSGSIZ, mr)) < 0){
			perror("rdma_post_recv");
			exit(-1);
		}

		// Accept the connection
		if ((rdma_accept(conn_id, NULL)) < 0){
			perror("rdma_accept");
			exit(-1);
		}
	}

	// Seems not possible to break connection and reclaim resources

	return 0;
}
