#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <eosio/kafka_plugin/kafka_producer.hpp>

 
/*
    每条消息调用一次该回调函数，说明消息是传递成功(rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR)
    还是传递失败(rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR)
    该回调函数由rd_kafka_poll()触发，在应用程序的线程上执行
 */
namespace eosio {

    int kafka_producer::kafka_create_topic(char *brokers, char *topic,rd_kafka_t** rk,rd_kafka_topic_t** rkt,rd_kafka_conf_t** conf){
        char errstr[512];
        if (brokers == NULL || topic == NULL) {
            return KAFKA_STATUS_INIT_FAIL;
        }

        *conf = rd_kafka_conf_new();

        if (rd_kafka_conf_set(*conf, "bootstrap.servers", brokers, errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            fprintf(stderr, "%s\n", errstr);
            return KAFKA_STATUS_INIT_FAIL;
        }

        rd_kafka_conf_set_dr_msg_cb(*conf, dr_msg_cb);

        *rk = rd_kafka_new(RD_KAFKA_PRODUCER, *conf, errstr, sizeof(errstr));
        if (!(*rk)) {
            fprintf(stderr, "%% Failed to create new producer:%s\n", errstr);
            return KAFKA_STATUS_INIT_FAIL;
        }

        *rkt = rd_kafka_topic_new(*rk, topic, NULL);
        if (!(*rkt)) {
            fprintf(stderr, "%% Failed to create topic object: %s\n",
                    rd_kafka_err2str(rd_kafka_last_error()));
            rd_kafka_destroy(*rk);
            *rk = NULL;
            return KAFKA_STATUS_INIT_FAIL;
        }

        return KAFKA_STATUS_OK;

    }

    int kafka_producer::kafka_init(char *brokers, char *trx_acceptopic, char *trx_appliedtopic,char *trx_transfertopic, char *block_accepttopic) {

        if (brokers == NULL) {
            return KAFKA_STATUS_INIT_FAIL;
        }

        if (trx_acceptopic != NULL) {
            if(KAFKA_STATUS_OK!=kafka_create_topic(brokers,trx_acceptopic,&trx_accept_rk,&trx_accept_rkt,&trx_accept_conf)){
                return KAFKA_STATUS_INIT_FAIL;
            }
        }

        if (trx_appliedtopic != NULL) {
            if(KAFKA_STATUS_OK!=kafka_create_topic(brokers,trx_appliedtopic,&trx_applied_rk,&trx_applied_rkt,&trx_applied_conf)){
                return KAFKA_STATUS_INIT_FAIL;
            }
        }

        if (trx_transfertopic != NULL) {
            if(KAFKA_STATUS_OK!=kafka_create_topic(brokers,trx_transfertopic,&trx_transfer_rk,&trx_transfer_rkt,&trx_transfer_conf)){
                return KAFKA_STATUS_INIT_FAIL;
            }
        }
        if (block_accepttopic != NULL) {
            if(KAFKA_STATUS_OK!=kafka_create_topic(brokers,block_accepttopic,&block_accept_rk,&block_accept_rkt,&block_accept_conf)){
                return KAFKA_STATUS_INIT_FAIL;
            }
        }
        return KAFKA_STATUS_OK;
    }

    int kafka_producer::kafka_sendmsg(int trxtype, char *msgstr) {
        rd_kafka_t *rk;
        rd_kafka_topic_t *rkt;
        if (trxtype == KAFKA_TRX_ACCEPT && trx_accept_rk!=NULL && trx_accept_rkt!=NULL) {
            rk = trx_accept_rk;
            rkt = trx_accept_rkt;
        } else if (trxtype == KAFKA_TRX_APPLIED && trx_applied_rk!=NULL && trx_applied_rkt!=NULL) {
            rk = trx_applied_rk;
            rkt = trx_applied_rkt;
        } else if(trxtype == KAFKA_TRX_TRANSFER && trx_transfer_rk!=NULL && trx_transfer_rkt!=NULL){
            rk = trx_transfer_rk;
            rkt = trx_transfer_rkt;
        } else if(trxtype == KAFKA_BLOCK_ACCEPT && block_accept_rk!=NULL && block_accept_rkt!=NULL){
            rk = block_accept_rk;
            rkt = block_accept_rkt;
        }  else {
            return KAFKA_STATUS_MSG_INVALID;
        }

        size_t len = strlen(msgstr);
        if (len == 0) {
            rd_kafka_poll(rk, 0);
            return KAFKA_STATUS_MSG_INVALID;
        }
        retry:
        if (rd_kafka_produce(
                rkt,
                RD_KAFKA_PARTITION_UA,
                RD_KAFKA_MSG_F_COPY,
                msgstr, len,
                NULL, 0,
                NULL) == -1) {
            fprintf(stderr,
                    "%% Failed to produce to topic %s: %s\n",
                    rd_kafka_topic_name(rkt),
                    rd_kafka_err2str(rd_kafka_last_error()));

            if (rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                rd_kafka_poll(rk, 1000);
                goto retry;
            }
        } else {
//        fprintf(stderr, "%% Enqueued message (%zd bytes) for topic %s\n", len, rd_kafka_topic_name(rkt));
        }

        rd_kafka_poll(rk, 0);
        return KAFKA_STATUS_OK;

    }

    rd_kafka_topic_t* kafka_producer::kafka_get_topic(int trxtype){

        if(trxtype == KAFKA_TRX_ACCEPT){
            return trx_accept_rkt;
        }else if(trxtype == KAFKA_TRX_APPLIED){
            return trx_applied_rkt;
        }else if(trxtype == KAFKA_TRX_TRANSFER){
            return trx_transfer_rkt;
        }else if(trxtype == KAFKA_BLOCK_ACCEPT){
            return block_accept_rkt;
        }else{
            return NULL;
        }

    }

    int kafka_producer::kafka_destroy(void) {
        fprintf(stderr, "=== kafka_destroyFlushing final message.. \n");
        if (trx_accept_rk != NULL) {
            rd_kafka_flush(trx_accept_rk, 10 * 1000);
            /* Destroy topic object */
            rd_kafka_topic_destroy(trx_accept_rkt);
            /* Destroy the producer instance */
            rd_kafka_destroy(trx_accept_rk);
            trx_accept_rk = NULL;
            trx_accept_rkt = NULL;
        }
        if (trx_applied_rk != NULL) {
            rd_kafka_flush(trx_applied_rk, 10 * 1000);
            /* Destroy topic object */
            rd_kafka_topic_destroy(trx_applied_rkt);
            /* Destroy the producer instance */
            rd_kafka_destroy(trx_applied_rk);
            trx_applied_rk = NULL;
            trx_applied_rkt = NULL;
        }
        if (trx_transfer_rk != NULL) {
            rd_kafka_flush(trx_transfer_rk, 10 * 1000);
            /* Destroy topic object */
            rd_kafka_topic_destroy(trx_transfer_rkt);
            /* Destroy the producer instance */
            rd_kafka_destroy(trx_transfer_rk);
            trx_transfer_rk = NULL;
            trx_transfer_rkt = NULL;
        }
        if (block_accept_rk != NULL) {
            rd_kafka_flush(block_accept_rk, 10 * 1000);
            /* Destroy topic object */
            rd_kafka_topic_destroy(block_accept_rkt);
            /* Destroy the producer instance */
            rd_kafka_destroy(block_accept_rk);
            block_accept_rk = NULL;
            block_accept_rkt = NULL;
        }
        return KAFKA_STATUS_OK;
    }
}
#if 0
int main(int argc, char **argv)
{
	char buf[512]; 
	int kafkastaus=KAFKA_STATUS_OK;
	trx_kafka_init();
	
	
	fprintf(stderr,
                "%% Type some text and hit enter to produce message\n"
                "%% Or just hit enter to only serve delivery reports\n"
                "%% Press Ctrl-C or Ctrl-D to exit\n");
	while(run && fgets(buf, sizeof(buf), stdin))
	{
		do{
			kafkastaus=trx_kafka_sendmsg(buf);
		}while(kafkastaus==KAFKA_STATUS_QUEUE_FULL);
	}

	trx_kafka_destroy();
}
#endif

#if 0
int main(int argc, char **argv){
	rd_kafka_t *rk;            /*Producer instance handle*/
	rd_kafka_topic_t *rkt;     /*topic对象*/
	rd_kafka_conf_t *conf;     /*临时配置对象*/
	char errstr[512];          
	char buf[512];             
	const char *brokers;       
	const char *topic;         
 
	if(argc != 3){
		fprintf(stderr, "%% Usage: %s <broker> <topic>\n", argv[0]);
        return 1;
	}
 
	brokers = argv[1];
	topic = argv[2];
 
    /* 创建一个kafka配置占位 */
	conf = rd_kafka_conf_new();
 
    /*创建broker集群*/
	if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr,
				sizeof(errstr)) != RD_KAFKA_CONF_OK){
		fprintf(stderr, "%s\n", errstr);
		return 1;
	}
 
    /*设置发送报告回调函数，rd_kafka_produce()接收的每条消息都会调用一次该回调函数
     *应用程序需要定期调用rd_kafka_poll()来服务排队的发送报告回调函数*/
	rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
 
    /*创建producer实例
      rd_kafka_new()获取conf对象的所有权,应用程序在此调用之后不得再次引用它*/
	rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
	if(!rk){
		fprintf(stderr, "%% Failed to create new producer:%s\n", errstr);
		return 1;
	}
 
    /*实例化一个或多个topics(`rd_kafka_topic_t`)来提供生产或消费，topic
    对象保存topic特定的配置，并在内部填充所有可用分区和leader brokers，*/
	rkt = rd_kafka_topic_new(rk, topic, NULL);
	if (!rkt){
		fprintf(stderr, "%% Failed to create topic object: %s\n", 
				rd_kafka_err2str(rd_kafka_last_error()));
		rd_kafka_destroy(rk);
		return 1;
	}
 
    /*用于中断的信号*/
	signal(SIGINT, stop);
 
	fprintf(stderr,
                "%% Type some text and hit enter to produce message\n"
                "%% Or just hit enter to only serve delivery reports\n"
                "%% Press Ctrl-C or Ctrl-D to exit\n");
 
     while(run && fgets(buf, sizeof(buf), stdin)){
     	size_t len = strlen(buf);
 
     	if(buf[len-1] == '\n')
     		buf[--len] = '\0';
 
     	if(len == 0){
            /*轮询用于事件的kafka handle，
            事件将导致应用程序提供的回调函数被调用
            第二个参数是最大阻塞时间，如果设为0，将会是非阻塞的调用*/
     		rd_kafka_poll(rk, 0);
     		continue;
     	}
 
     retry:
         /*Send/Produce message.
           这是一个异步调用，在成功的情况下，只会将消息排入内部producer队列，
           对broker的实际传递尝试由后台线程处理，之前注册的传递回调函数(dr_msg_cb)
           用于在消息传递成功或失败时向应用程序发回信号*/
     	if (rd_kafka_produce(
                    /* Topic object */
     				rkt,
                    /*使用内置的分区来选择分区*/
     				RD_KAFKA_PARTITION_UA,
                    /*生成payload的副本*/
     				RD_KAFKA_MSG_F_COPY,
                    /*消息体和长度*/
     				buf, len,
                    /*可选键及其长度*/
     				NULL, 0,
     				NULL) == -1){
     		fprintf(stderr, 
     			"%% Failed to produce to topic %s: %s\n", 
     			rd_kafka_topic_name(rkt),
     			rd_kafka_err2str(rd_kafka_last_error()));
 
     		if (rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL){
                /*如果内部队列满，等待消息传输完成并retry,
                内部队列表示要发送的消息和已发送或失败的消息，
                内部队列受限于queue.buffering.max.messages配置项*/
     			rd_kafka_poll(rk, 1000);
     			goto retry;
     		}	
     	}else{
     		fprintf(stderr, "%% Enqueued message (%zd bytes) for topic %s\n", 
     			len, rd_kafka_topic_name(rkt));
     	}
 
        /*producer应用程序应不断地通过以频繁的间隔调用rd_kafka_poll()来为
        传送报告队列提供服务。在没有生成消息以确定先前生成的消息已发送了其
        发送报告回调函数(和其他注册过的回调函数)期间，要确保rd_kafka_poll()
        仍然被调用*/
     	rd_kafka_poll(rk, 0);
     }
 
     fprintf(stderr, "%% Flushing final message.. \n");
     /*rd_kafka_flush是rd_kafka_poll()的抽象化，
     等待所有未完成的produce请求完成，通常在销毁producer实例前完成
     以确保所有排列中和正在传输的produce请求在销毁前完成*/
     rd_kafka_flush(rk, 10*1000);
 
     /* Destroy topic object */
     rd_kafka_topic_destroy(rkt);
 
     /* Destroy the producer instance */
     rd_kafka_destroy(rk);
 
     return 0;
}
#endif

