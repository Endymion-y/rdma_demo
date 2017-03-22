// RDMA server
#include <iostream>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <thread>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
using namespace std;

const int MSGSIZ = 128;

void run(struct rdma_cm_id* conn_id){	
	cout << "Thread started" << endl;
	int ret;

	// Allocate memory and register
	void* recv_msg = malloc(MSGSIZ);
	struct ibv_mr* recv_mr = rdma_reg_msgs(conn_id, recv_msg, MSGSIZ);
	assert(recv_mr);
	void* send_msg = malloc(MSGSIZ);
	struct ibv_mr* send_mr = rdma_reg_msgs(conn_id, send_msg, MSGSIZ);
	assert(send_mr);

	// Finalize connection with client
	// if ((ret = rdma_accept(conn_id, NULL)) < 0){
	// 	perror("rdma_accept");
	// 	return;
	// }
			
	struct ibv_wc recv_wc;      // CQE array with length 1
	struct ibv_wc send_wc;

	while (true){
		// Post recv
		if ((ret = rdma_post_recv(conn_id, NULL, recv_msg, MSGSIZ, recv_mr)) < 0){
			perror("rdma_post_recv");
			return;
		}
		if ((ret = rdma_get_recv_comp(conn_id, &recv_wc)) < 0){
			perror("rdma_get_recv_comp");
			return;
		}
#ifdef LOGGING
		cout << "Message received: " << (char*)recv_msg << endl;
#endif
		// Copy message
		memcpy(send_msg, recv_msg, MSGSIZ);

		// Post send
		if ((ret = rdma_post_send(conn_id, NULL, send_msg, MSGSIZ, send_mr, 0)) < 0){
			perror("rdma_post_send");
			return;
		}
		if ((ret = rdma_get_send_comp(conn_id, &send_wc)) < 0){
			perror("rdma_get_send_comp");
			return;
		}
#ifdef LOGGING
		cout << "Message sent: " << (char*)send_msg << endl;
#endif
	}

	// Break connection
	if ((ret = rdma_disconnect(conn_id)) < 0){
		perror("rdma_disconnect");
		return;
	}
	// Deregister user virtual memory
	if ((ret = rdma_dereg_mr(recv_mr)) < 0){
		perror("rdma_dereg_mr");
		return;
	}
	if ((ret = rdma_dereg_mr(send_mr)) < 0){
		perror("rdma_dereg_mr");
		return;
	}
	// Free memory
	free(recv_msg);
	free(send_msg);
	// Destroy local end point
	rdma_destroy_ep(conn_id);
}

int main(int argc, char* argv[]){
	if (argc != 2){
		cout << "Usage: ./server port" << endl;
		return 0;
	}
	int ret;

	// Obtain and convert addressing information
	char* IP = NULL;
	char* port = argv[1];
	struct rdma_addrinfo hints;
	struct rdma_addrinfo* res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = RAI_PASSIVE;          // Server
	hints.ai_port_space = RDMA_PS_TCP;
	if ((ret = rdma_getaddrinfo(IP, port, &hints, &res)) < 0){
		perror("rdma_getaddrinfo");
		exit(-1);
	}
		

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
	if ((ret = rdma_create_ep(&id, res, NULL, &qp_init_attr)) < 0){
		perror("rdma_create_ep");
		exit(-1);
	}
		
	// Establish socket as listener
	if ((ret = rdma_listen(id, 0)) < 0){
		perror("rdma_listen");
		exit(-1);
	}
	cout << "Server listening on " << port << endl;
		
	// Loop
	while (true){
		struct rdma_cm_id* conn_id;
		// Get connection request from client
		if ((ret = rdma_get_request(id, &conn_id)) < 0){
			perror("rdma_get_request");
			exit(-1);
		}
		cout << "Got a request" << endl;
		// Finalize connection with client
		if ((ret = rdma_accept(conn_id, NULL)) < 0){
			perror("rdma_accept");
			exit(-1);
		}

		// thread client(run, conn_id);
		thread* client = new thread(run, conn_id);
	}

	// Close connection, free memory and communication resources
	if ((ret = rdma_disconnect(id)) < 0){
		perror("rdma_disconnect");
		exit(-1);
	}
	rdma_freeaddrinfo(res);

	return 0;
}
