#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H


/* Maximium header size allowed */
#define HTTP_MAX_HEADER_SIZE (8*1024)


typedef struct http_parser http_parser;
typedef struct http_parser_settings http_parser_settings;


typedef int (*http_data_cb) (http_parser*, const char *at, int length);
typedef int (*http_cb) (http_parser*);


enum
{
    HTTP_DELETE =0,
	HTTP_GET	=1,	
	HTTP_HEAD	=2,
	HTTP_POST	=3,
	HTTP_PUT	=4,
	HTTP_CONNECT=5,
	HTTP_OPTIONS=6,
	HTTP_TRACE	=7,
	HTTP_COPY	=8,
	HTTP_LOCK	=9,
	HTTP_MKCOL	=10,
	HTTP_MOVE	=11,
	HTTP_PROPFIND=12,
	HTTP_PROPPATCH=13,
	HTTP_SEARCH	=14,
	HTTP_UNLOCK	=15,
	HTTP_REPORT	=16,
	HTTP_MKACTIVITY=17,
	HTTP_CHECKOUT=18,
	HTTP_MERGE	=19,
	HTTP_MSEARCH=20,
	HTTP_NOTIFY	=21,
	HTTP_SUBSCRIBE=22,
	HTTP_UNSUBSCRIBE=23,
	HTTP_PATCH	=24,
	HTTP_PURGE	=25,
};


enum http_parser_type { HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH };


/* Flag values for http_parser.flags field */
enum flags
{ 
    F_CHUNKED               = 1 << 0
    , F_CONNECTION_KEEP_ALIVE = 1 << 1
    , F_CONNECTION_CLOSE      = 1 << 2
    , F_TRAILING              = 1 << 3
    , F_UPGRADE               = 1 << 4
    , F_SKIPBODY              = 1 << 5
};

enum http_errno 
{
    HPE_OK,
    HPE_CB_message_begin,
    HPE_CB_status_complete,
    HPE_CB_url,
    HPE_CB_header_field,
    HPE_CB_header_value,
    HPE_CB_headers_complete,
    HPE_CB_body,
    HPE_CB_message_complete,
    HPE_INVALID_EOF_STATE,
    HPE_HEADER_OVERFLOW,
    HPE_CLOSED_CONNECTION,
    HPE_INVALID_VERSION,
    HPE_INVALID_STATUS,
    HPE_INVALID_METHOD,
    HPE_INVALID_URL,
    HPE_INVALID_HOST,
    HPE_INVALID_PORT,
    HPE_INVALID_PATH,
    HPE_INVALID_QUERY_STRING,
    HPE_INVALID_FRAGMENT,
    HPE_LF_EXPECTED,
    HPE_INVALID_HEADER_TOKEN,
    HPE_INVALID_CONTENT_LENGTH,
    HPE_INVALID_CHUNK_SIZE,
    HPE_INVALID_CONSTANT,
    HPE_INVALID_INTERNAL_STATE,
    HPE_STRICT,
    HPE_PAUSED,
    HPE_UNKNOWN,
};

/* Get an http_errno value from an http_parser */
#define HTTP_PARSER_ERRNO(p)            ((enum http_errno) (p)->http_errno)


struct http_parser
{
  unsigned char type;     /* enum http_parser_type */
  unsigned char flags;    /* F_* values from 'flags' enum; semi-public */
  unsigned char state;        /* enum state from http_parser.c */
  unsigned char header_state; /* enum header_state from http_parser.c */
  unsigned char index;        /* index into current matcher */

  unsigned int nread;          /* # bytes read in various scenarios */
  unsigned int content_length; /* # bytes in body (0 if no Content-Length header) */

  /** READ-ONLY **/
  unsigned short http_major;
  unsigned short http_minor;
  unsigned short status_code; /* responses only */
  unsigned char method;       /* requests only */
  unsigned char http_errno;

  /* 1 = Upgrade header was present and the parser has exited because of that.
   * 0 = No upgrade header present.
   * Should be checked when http_parser_execute() returns in addition to
   * error checking.
   */
  unsigned char upgrade;

  /** PUBLIC **/
  void *data; /* A pointer to get hook to the "connection" or "socket" object */
};


struct http_parser_settings {
  http_cb      on_message_begin;
  http_data_cb on_url;
  http_cb      on_status_complete;
  http_data_cb on_header_field;
  http_data_cb on_header_value;
  http_cb      on_headers_complete;
  http_data_cb on_body;
  http_cb      on_message_complete;
};


enum http_parser_url_fields
{ UF_SCHEMA           = 0
  , UF_HOST             = 1
  , UF_PORT             = 2
  , UF_PATH             = 3
  , UF_QUERY            = 4
  , UF_FRAGMENT         = 5
  , UF_USERINFO         = 6
  , UF_MAX              = 7
};


void http_parser_init(http_parser *parser, enum http_parser_type type);
int  http_parser_execute(http_parser *parser, const http_parser_settings *settings, const char *data, int len);
int  http_should_keep_alive(const http_parser *parser);


/*
 *	smart funcs for http module
 */
char* we_url_make_header(int iMethod, char *pszURL, int postLen/*valid when POST*/);
char* we_url_path(char* url);
unsigned short we_url_port(char* url, int port);
char* we_url_host(char* url);
char* we_url_hostport(char* url);


#endif
