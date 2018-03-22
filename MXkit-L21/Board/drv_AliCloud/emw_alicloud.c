#include <stdlib.h>
#include <string.h>
#include "mx_debug.h"
#include "mx_common.h"
#include "emw_api.h"
#include "ATCmdParser.h"
#include "emw_alicloud_db.h"

mx_status emw_ali_config(const emw_ali_config_t* config)
{
	char args[200], *arg_list[5];
	const char* format_arg = emw_arg_for_type(EMW_ARG_ALI_FORMAT, config->product_info.format);
	
	/* Check product info*/
	if (!(ATCmdParser_send("AT+ALINKPRODUCT?")
	   && ATCmdParser_recv("+ALINKPRODUCT:%200[^\r]\r\n",args)
	   && ATCmdParser_recv("OK\r\n"))) {
		return kReadErr;
	}

	if (4 != ATCmdParser_analyse_args(args, arg_list, 4)) {
		return kMalformedErr;
	}

	if (strcmp(arg_list[0], config->product_info.name)
	 || strcmp(arg_list[1], config->product_info.modle)
	 || strcmp(arg_list[2], config->product_info.key)
	 || strcmp(arg_list[3], config->product_info.secret) ){
		 
		if (!(ATCmdParser_send("AT+ALINKPRODUCT=%s,%s,%s,%s,%s", 
							 config->product_info.name, config->product_info.modle, 
	                         config->product_info.key,  config->product_info.secret, format_arg )
		  && ATCmdParser_recv("OK\r\n"))) {
			return kGeneralErr;
		}
	 }
						
	/* Check device info*/
	if (!(ATCmdParser_send("AT+ALINKDEV?")
	   && ATCmdParser_recv("+ALINKDEV:%40[^\r]\r\n",args)
	   && ATCmdParser_recv("OK\r\n"))) {
		return kReadErr;
	}
	
	if (3 != ATCmdParser_analyse_args(args, arg_list, 3)) {
		return kMalformedErr;
	}

	if ((strcmp(arg_list[0], config->dev_info.type)
	  || strcmp(arg_list[1], config->dev_info.category)
	  || strcmp(arg_list[2], config->dev_info.manufacture))){
		 
		if (!(ATCmdParser_send("AT+ALINKDEV=%s,%s,%s", 
							   config->dev_info.type, config->dev_info.category,
	                           config->dev_info.manufacture )
		  && ATCmdParser_recv("OK\r\n"))) {
			return kGeneralErr;
		}
	}
	  
	return kNoErr; 
}

mx_status emw_ali_set_key (const char *dev_key, const char *dev_sec)
{
	if (ATCmdParser_send("AT+ALINKSDS=%s,%s", dev_key, dev_sec)
	 && ATCmdParser_recv("OK\r\n")) {
		return kNoErr;
	}
	return kGeneralErr;
}

mx_status emw_ali_start_service(void)
{
	if (ATCmdParser_send("AT+ALINKSTART")
     && ATCmdParser_recv("OK\r\n")) {
        return kNoErr;
    }
	return kGeneralErr;
}

emw_arg_ali_status_e emw_ali_get_status(void)
{
	char arg[20];

	if (!(ATCmdParser_send("AT+ALINKSTATUS?")
	   && ATCmdParser_recv("+ALINKSTATUS:%20[^\r]\r\n",arg)
	   && ATCmdParser_recv("OK\r\n"))) {
		return EMW_ARG_ERR;
	}
	
	return emw_arg_for_arg( EMW_ARG_ALI_STATUS, arg);
}

mx_status emw_ali_provision(bool on)
{
	if ((on? ATCmdParser_send("AT+ALINKAWSSTART"):
		     ATCmdParser_send("AT+ALINKAWSSTOP"))
		  && ATCmdParser_recv("OK\r\n")) {
		return kNoErr;
	}
	return kGeneralErr;
}

mx_status emw_ali_start_provision(void)
{
	if (ATCmdParser_send("AT+ALINKAWSSTART")
     && ATCmdParser_recv("OK\r\n")) {
        return kNoErr;
    }
	return kGeneralErr;
}

mx_status emw_ali_unbound(void)
{
	if (ATCmdParser_send("AT+ALINKUNBIND")
     && ATCmdParser_recv("OK\r\n")) {
        return kNoErr;
    }
	return kGeneralErr;
}

mx_status emw_ali_stop_provision(void)
{
	if (ATCmdParser_send("AT+ALINKAWSSTOP")
     && ATCmdParser_recv("OK\r\n")) {
        return kNoErr;
    }
	return kGeneralErr;
}

mx_status emw_ali_set_cloud_atts(emw_arg_ali_format_e format, uint8_t *data, int32_t len)
{	
	if (ATCmdParser_send("AT+ALINKSEND=%d", len)
	 && ATCmdParser_recv(">")
	 && ATCmdParser_write(data, len) == len
	 && ATCmdParser_recv("OK\r\n")) {
		return kNoErr;
	}
	return kGeneralErr;
}

void emw_ali_event_handler(void)
{
	mx_status err = kNoErr;
	char arg1[10], arg2[10];
	emw_arg_ali_format_e format;
	emw_arg_ali_conn_e conn;
	emw_ali_local_attrs_t attrs;

	// parse out the packet
	require_action(ATCmdParser_recv("%10[^,],", arg1), exit, err = kMalformedErr);
		
	emw_arg_ali_ev_e event = emw_arg_for_arg(EMW_ARG_ALI_EV, arg1);
	require_action(event != EMW_ARG_ERR, exit,  err = kMalformedErr);

	/* ALINK Server connection event */
	if (event == EMW_ARG_ALI_EV_ALINK) {
		require_action(ATCmdParser_recv("%10[^\r]\r\n", arg2), exit, err = kMalformedErr);
		conn = emw_arg_for_arg(EMW_ARG_ALI_CONN, arg2);
		require_action(conn != EMW_ARG_ERR, exit, err = kMalformedErr);
		emw_ev_ali_connection(conn);
	}
	/* ALINK server <=== attribute value=== device */
	else if (event == EMW_ARG_ALI_EV_GET) {
		require_action(ATCmdParser_recv("%10[^\r]\r\n", arg2), exit, err = kMalformedErr);
		format =  emw_arg_for_arg(EMW_ARG_ALI_FORMAT, arg2);
		require_action(format != EMW_ARG_ERR, exit, err = kMalformedErr);
		
		attrs.format = format;
		attrs.data = NULL;
		attrs.len = 0;
		emw_ev_ali_get_local_atts(&attrs);
	}
	/* ALINK server === attribute value===> device */
	else if (event == EMW_ARG_ALI_EV_SET) {
		require_action(ATCmdParser_recv("%10[^,],", arg2), exit, err = kMalformedErr);
		format = emw_arg_for_arg(EMW_ARG_ALI_FORMAT, arg2);
		require_action(format != EMW_ARG_ERR, exit, err = kMalformedErr);
		
		/* Read package data */
		int32_t count;
		require_action(ATCmdParser_recv("%d,", &count), exit, err = kMalformedErr);

		uint8_t *data = malloc(count);
		require_action(data, exit, err = kNoMemoryErr);
		require_action(ATCmdParser_read(data, count) == count, exit, err = kTimeoutErr);

		attrs.data = data;
		attrs.format = format;
		attrs.len = count;
		emw_ev_ali_set_local_atts(&attrs);
		free(data);
	}
	
exit:
	if (err == kMalformedErr) emw_ev_unknown();
	return;
}

