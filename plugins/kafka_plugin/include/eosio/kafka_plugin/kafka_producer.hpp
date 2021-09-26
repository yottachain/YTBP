#pragma once

#include <librdkafka/rdkafka.h>

namespace eosio {
#define KAFKA_STATUS_OK 0
#define KAFKA_STATUS_INIT_FAIL 1
#define KAFKA_STATUS_MSG_INVALID 2
#define KAFKA_STATUS_QUEUE_FULL 3

#define KAFKA_TRX_ACCEPT 0
#define KAFKA_TRX_APPLIED 1
#define KAFKA_TRX_TRANSFER 2
#define KAFKA_BLOCK_ACCEPT 3

class kafka_producer {
    public:
        kafka_producer() {

            trx_accept_rk = NULL;
            trx_applied_rk = NULL;
            trx_transfer_rk = NULL;
            block_accept_rk = NULL;
            trx_accept_rkt = NULL;
            trx_applied_rkt = NULL;
            trx_transfer_rkt = NULL;
            block_accept_rkt = NULL;
            trx_accept_conf = NULL;
            trx_applied_conf = NULL;
            trx_transfer_conf = NULL;
            block_accept_conf = NULL;
        };

        int kafka_init(char *brokers, char *trx_acceptopic, char *trx_appliedtopic,char *trx_transfertopic, char *block_accepttopic);

        int kafka_create_topic(char *brokers, char *topic,rd_kafka_t** rk,rd_kafka_topic_t** rkt,rd_kafka_conf_t** conf);

        int kafka_sendmsg(int trxtype, char *msgstr);

        int kafka_destroy(void);

        rd_kafka_topic_t* kafka_get_topic(int trxtype);



    private:
        rd_kafka_t *trx_accept_rk;            /*Producer instance handle*/
        rd_kafka_t *trx_applied_rk;           /*Producer instance handle*/
        rd_kafka_t *trx_transfer_rk;          /*Producer instance handle*/
        rd_kafka_t *block_accept_rk;          /*Producer instance handle*/
        rd_kafka_topic_t *trx_accept_rkt;     /*topic object*/
        rd_kafka_topic_t *trx_applied_rkt;    /*topic object*/
        rd_kafka_topic_t *trx_transfer_rkt;   /*topic object*/
        rd_kafka_topic_t *block_accept_rkt;   /*topic object*/
        rd_kafka_conf_t *trx_accept_conf;     /*kafka config*/
        rd_kafka_conf_t *trx_applied_conf;    /*kafka config*/
        rd_kafka_conf_t *trx_transfer_conf;   /*kafka config*/
        rd_kafka_conf_t *block_accept_conf;   /*kafka config*/

        static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque){}
    };
}

