
/*
* Copyright 2009 Roast
*/

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_XDATA_FILE_MAX_SIZE  10240
#define NGX_XDATA_WORD_MAX_SIZE	 20

typedef struct {
	ngx_flag_t	enable;
	ngx_str_t	xdata_word_file;
	ngx_array_t	*xdata_word_list;
} ngx_http_xdata_up_conf_t;

static void ngx_http_x_up_body_handler(ngx_http_request_t *r);
static void *ngx_http_xdata_up_create_conf(ngx_conf_t *cf);
static char *ngx_http_xdata_up_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_xdata_up_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_xdata_up_word(ngx_conf_t *cf, ngx_array_t *xdata_word_list, ngx_str_t *word_list_file);

static ngx_command_t ngx_http_xdata_up_commands[] = {
	{ngx_string("xdata_up"),
	NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
	ngx_conf_set_flag_slot,
	NGX_HTTP_LOC_CONF_OFFSET,
	offsetof(ngx_http_xdata_up_conf_t, enable),
	NULL},

	{ngx_string("xdata_up_word_file"),
	NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_str_slot,
	NGX_HTTP_LOC_CONF_OFFSET,
	offsetof(ngx_http_xdata_up_conf_t, xdata_word_file),
	NULL},

	ngx_null_command
};

static ngx_http_module_t ngx_http_xdata_up_module_ctx = {
	NULL,                         /* preconfiguration */
	ngx_http_xdata_up_init,   /* postconfiguration */

	NULL,                         /* create main configuration */
	NULL,                         /* init main configuration */

	NULL,                         /* create server configuration */
	NULL,                         /* merge server configuration */

	ngx_http_xdata_up_create_conf,   /* create location configuration */
	ngx_http_xdata_up_merge_conf     /* merge location configuration */
};

ngx_module_t  ngx_http_xdata_up_module = {
	NGX_MODULE_V1,
	&ngx_http_xdata_up_module_ctx,	   /* module context */
	ngx_http_xdata_up_commands,		   /* module directives */
	NGX_HTTP_MODULE,                       /* module type */
	NULL,                                  /* init master */
	NULL,                                  /* init module */
	NULL,                                  /* init process */
	NULL,                                  /* init thread */
	NULL,                                  /* exit thread */
	NULL,                                  /* exit process */
	NULL,                                  /* exit master */
	NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_xdata_up_handler(ngx_http_request_t *r)
{
	u_char *s;
	ngx_int_t rc;
	ngx_uint_t	i, m;
	ngx_str_t	*word;
	ngx_table_elt_t  *h;
	ngx_http_xdata_up_conf_t	*xucf;

	xucf = ngx_http_get_module_loc_conf(r, ngx_http_xdata_up_module);

	if (!xucf->enable) {
		return NGX_DECLINED;
	}

	ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_xdata_up_handler uri:%V", &(r->request_line));

	h = ngx_list_push(&r->headers_in.headers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	h->hash = 1;
	h->key.len = 3;
	h->key.data = (u_char *)"KEY";
	h->value.len = 5;
	h->value.data = (u_char *)"VALUE";

	/* filter uri */
	word = xucf->xdata_word_list->elts;
	for (i = 0; i < xucf->xdata_word_list->nelts; i++) {
		s = r->request_line.data;
		do {
			s = ngx_strstrn(s, (char *)word[i].data, word[i].len - 1);
			if (s != NULL) {
				for (m = 0; m < word[i].len; m++) {
					s[m] = '*';
				}
				s +=  word[i].len;

				ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "match word:%s", word[i].data);
			}
		} while (s != NULL);
	}

	/* filter post body*/
	if (r->method == NGX_HTTP_POST) {
		rc = ngx_http_read_client_request_body(r, ngx_http_x_up_body_handler);

		if (rc >= NGX_HTTP_SPECIAL_RESPONSE) 
			return rc;
		else
			return NGX_DECLINED;
	}
	
	return NGX_DECLINED;
}


static void
ngx_http_x_up_body_handler(ngx_http_request_t *r)
{
	if (r->request_body) 
	{
		ngx_chain_t	*cl;
		for (cl = r->request_body->bufs; cl; cl = cl->next) 
		{
			cl->buf->pos[0] = '*';
			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "post request body:%s", cl->buf->pos);
		}
	}
}


static ngx_int_t
ngx_http_xdata_up_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt        *h;
	ngx_http_core_main_conf_t  *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_http_xdata_up_handler;

	return NGX_OK;
}

static void *
ngx_http_xdata_up_create_conf(ngx_conf_t *cf) 
{
	ngx_http_xdata_up_conf_t  *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_xdata_up_conf_t));
	if (conf == NULL) {
		return NGX_CONF_ERROR;
	}

	conf->enable = NGX_CONF_UNSET;

	conf->xdata_word_list = ngx_array_create(cf->pool, 10, sizeof(ngx_str_t));
	if (conf->xdata_word_list == NULL) {
		return NGX_CONF_ERROR;
	}

	return conf;
}

static char *
ngx_http_xdata_up_merge_conf(ngx_conf_t *cf, void *parent, void *child) 
{
	ngx_http_xdata_up_conf_t  *prev = parent;
	ngx_http_xdata_up_conf_t  *conf = child;

	ngx_conf_merge_value(conf->enable, prev->enable, 0);
	ngx_conf_merge_str_value(conf->xdata_word_file, prev->xdata_word_file, "");

	if (conf->xdata_word_file.len > 0) {
		ngx_http_xdata_up_word(cf, conf->xdata_word_list, &conf->xdata_word_file);
	}

	return NGX_CONF_OK;
}

static ngx_int_t 
ngx_http_xdata_up_word(ngx_conf_t *cf, ngx_array_t *xdata_word_list, ngx_str_t *word_list_file) 
{
	ssize_t		n;
	ngx_fd_t	fd;
	ngx_file_t	file;
	ngx_uint_t	i, c;
	ngx_str_t	*tmp_str;
	u_char		buf[NGX_XDATA_FILE_MAX_SIZE], word[NGX_XDATA_WORD_MAX_SIZE];

	fd = ngx_open_file(word_list_file->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);

	if (fd == NGX_INVALID_FILE) {
		ngx_log_error(NGX_LOG_CRIT, cf->log, ngx_errno, ngx_open_file_n " %V failed", word_list_file);
		return NGX_ERROR;
	}

	ngx_memzero(&file, sizeof(ngx_file_t));

	file.fd = fd;
	file.name = *word_list_file;
	file.log = cf->log;
	n = ngx_read_file(&file, buf, NGX_XDATA_FILE_MAX_SIZE, 0);

	ngx_close_file(fd);

	if (n == NGX_ERROR) {
		return NGX_ERROR;
	}

	if (n == 0) {
		ngx_log_error(NGX_LOG_EMERG, cf->log, 0, ngx_read_file_n " %V data was zero", word_list_file);
		return NGX_OK;
	}

	for (i = 0, c = 0; i < (ngx_uint_t) n; i++) {
		if (buf[i] == CR || buf[i] == LF) {
			if (c > 0) {
				tmp_str = ngx_array_push(xdata_word_list);
				if (tmp_str == NULL) {
					return NGX_ERROR;
				}

				tmp_str->len = c;

				tmp_str->data = ngx_pcalloc(cf->pool, tmp_str->len);
				if (tmp_str->data == NULL) {
					return NGX_ERROR;
				}

				ngx_memcpy(tmp_str->data, word, tmp_str->len);
				ngx_memzero(word, NGX_XDATA_WORD_MAX_SIZE);
			}

			c = 0;
		} else {
			word[c] = buf[i];
			c++;
		}
	}	

	return NGX_OK;
}
