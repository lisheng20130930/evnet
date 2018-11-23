#include "libos.h"
#include "httparser.h"

#define HTTP_UINT32_MAX ((unsigned int) -1) /* 2^64-1 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BIT_AT(a,i) (!!((unsigned int)(a)[(unsigned int)(i)>>3]&(1<<((unsigned int)(i)&7))))
#define ELEM_AT(a,i,v) ((unsigned int)(i)<ARRAY_SIZE(a)?(a)[(i)] : (v))
#define SET_ERRNO(e)  do{parser->http_errno=(e);}while(0)

/* Run the notify callback FOR, returning ER if it fails */
#define CALLBACK_NOTIFY_(FOR, ER)                                    \
    do {                                                                 \
    if (settings->on_##FOR) {                                          \
    if (0 != settings->on_##FOR(parser)) {                           \
    SET_ERRNO(HPE_CB_##FOR);                                       \
    }                                                                \
    \
    /* We either errored above or got paused; get out */             \
    if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {                       \
    return (ER);                                                   \
    }                                                                \
    }                                                                  \
    } while (0)

/* Run the notify callback FOR and consume the current byte */
#define CALLBACK_NOTIFY(FOR)            CALLBACK_NOTIFY_(FOR, p - data + 1)

/* Run the notify callback FOR and don't consume the current byte */
#define CALLBACK_NOTIFY_NOADVANCE(FOR)  CALLBACK_NOTIFY_(FOR, p - data)

/* Run data callback FOR with LEN bytes, returning ER if it fails */
#define CALLBACK_DATA_(FOR, LEN, ER)                                 \
    do {                                                                 \
    if (FOR##_mark) {                                                  \
    if (settings->on_##FOR) {                                        \
    if (0 != settings->on_##FOR(parser, FOR##_mark, (LEN))) {      \
    SET_ERRNO(HPE_CB_##FOR);                                     \
    }                                                              \
    \
    /* We either errored above or got paused; get out */           \
    if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {                     \
    return (ER);                                                 \
    }                                                              \
    }                                                                \
    FOR##_mark = NULL;                                               \
    }                                                                  \
    } while (0)

/* Run the data callback FOR and consume the current byte */
#define CALLBACK_DATA(FOR) CALLBACK_DATA_(FOR, p - FOR##_mark, p - data + 1)
/* Run the data callback FOR and don't consume the current byte */
#define CALLBACK_DATA_NOADVANCE(FOR)   CALLBACK_DATA_(FOR, p - FOR##_mark, p - data)
/* Set the mark FOR; non-destructive if mark is already set */
#define MARK(FOR) do {if (!FOR##_mark) {FOR##_mark = p; }}while(0)

#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"


static const char *method_strings[] =
{
	"DELETE",
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"CONNECT",
	"OPTIONS",
	"TRACE",
	"COPY",
	"LOCK", 
	"MKCOL",
	"MOVE", 
	"PROPFIND",
	"PROPPATCH",
	"SEARCH",
	"UNLOCK",
	"REPORT",
	"MKACTIVITY",
	"CHECKOUT",
	"MERGE",
	"M-SEARCH",
	"NOTIFY",
	"SUBSCRIBE",
	"UNSUBSCRIBE",
	"PATCH",
	"PURGE",
};

/* Tokens as defined by rfc 2616. Also lowercases them.
*        token       = 1*<any CHAR except CTLs or separators>
*     separators     = "(" | ")" | "<" | ">" | "@"
*                    | "," | ";" | ":" | "\" | <">
*                    | "/" | "[" | "]" | "?" | "="
*                    | "{" | "}" | SP | HT
*/
static const char tokens[256] = 
{
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0,      '!',      0,      '#',     '$',     '%',     '&',    '\'',
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    0,       0,      '*',     '+',      0,      '-',     '.',      0,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    '0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    '8',     '9',      0,       0,       0,       0,       0,       0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    0,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    'x',     'y',     'z',      0,       0,       0,      '^',     '_',
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    '`',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    'x',     'y',     'z',      0,      '|',      0,      '~',       0 
};
    
    
static const char unhex[256] =
{
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
    ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
    ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
    ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};


static const unsigned char normal_url_char[32] = 
{
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0    | 2   |   0    |   0    | 16  |   0    |   0    |   0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0    |   2    |   4    |   0    |   16   |   32   |   64   |  128,
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |   0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |   0, 
};
        
enum state
{ 
    s_dead = 1 /* important that this is > 0 */
    , s_start_req_or_res
    , s_res_or_resp_H
    , s_start_res
    , s_res_H
    , s_res_HT
    , s_res_HTT
    , s_res_HTTP
    , s_res_first_http_major
    , s_res_http_major
    , s_res_first_http_minor
    , s_res_http_minor
    , s_res_first_status_code
    , s_res_status_code
    , s_res_status
    , s_res_line_almost_done
    , s_start_req
    , s_req_method
    , s_req_spaces_before_url
    , s_req_schema
    , s_req_schema_slash
    , s_req_schema_slash_slash
    , s_req_server_start
    , s_req_server
    , s_req_server_with_at
    , s_req_path
    , s_req_query_string_start
    , s_req_query_string
    , s_req_fragment_start
    , s_req_fragment
    , s_req_http_start
    , s_req_http_H
    , s_req_http_HT
    , s_req_http_HTT
    , s_req_http_HTTP
    , s_req_first_http_major
    , s_req_http_major
    , s_req_first_http_minor
    , s_req_http_minor
    , s_req_line_almost_done
    , s_header_field_start
    , s_header_field
    , s_header_value_start
    , s_header_value
    , s_header_value_lws
    , s_header_almost_done
    , s_chunk_size_start
    , s_chunk_size
    , s_chunk_parameters
    , s_chunk_size_almost_done
    , s_headers_almost_done
    , s_headers_done
    , s_chunk_data
    , s_chunk_data_almost_done
    , s_chunk_data_done
    , s_body_identity
    , s_body_identity_eof
    , s_message_done
};    
        
enum header_states
{ 
    h_general = 0
    , h_C
    , h_CO
    , h_CON    
    , h_matching_connection
    , h_matching_proxy_connection
    , h_matching_content_length
    , h_matching_transfer_encoding
    , h_matching_upgrade    
    , h_connection
    , h_content_length
    , h_transfer_encoding
    , h_upgrade    
    , h_matching_transfer_encoding_chunked
    , h_matching_connection_keep_alive
    , h_matching_connection_close    
    , h_transfer_encoding_chunked
    , h_connection_keep_alive
    , h_connection_close
};
        
enum http_host_state
{
    s_http_host_dead = 1
    , s_http_userinfo_start
    , s_http_userinfo
    , s_http_host_start
    , s_http_host_v6_start
    , s_http_host
    , s_http_host_v6
    , s_http_host_v6_end
    , s_http_host_port_start
    , s_http_host_port
};
        
        /* Macros for character classes; depends on strict-mode  */
#define CR                  '\r'
#define LF                  '\n'
#define LOWER(c)            (unsigned char)(c | 0x20)
#define IS_ALPHA(c)         (LOWER(c) >= 'a' && LOWER(c) <= 'z')
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)      (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)           (IS_NUM(c) || (LOWER(c) >= 'a' && LOWER(c) <= 'f'))
#define IS_MARK(c)          ((c) == '-' || (c) == '_' || (c) == '.' || (c) == '!' || (c) == '~' || (c) == '*' || (c) == '\'' || (c) == '(' || (c) == ')')
#define IS_USERINFO_CHAR(c) (IS_ALPHANUM(c) || IS_MARK(c) || (c) == '%' || (c) == ';' || (c) == ':' || (c) == '&' || (c) == '=' || (c) == '+' || (c) == '$' || (c) == ',')

#define TOKEN(c)            (tokens[(unsigned char)c])
#define IS_URL_CHAR(c)      (BIT_AT(normal_url_char, (unsigned char)c))
#define IS_HOST_CHAR(c)     (IS_ALPHANUM(c) || (c) == '.' || (c) == '-')
#define start_state (parser->type == HTTP_REQUEST ? s_start_req : s_start_res)
#define STRICT_CHECK(cond)  do{if(cond){SET_ERRNO(HPE_STRICT); goto error; }}while(0)
#define NEW_MESSAGE() (http_should_keep_alive(parser) ? start_state : s_dead)

/* Does the parser need to see an EOF to find the end of the message? */
int http_message_needs_eof (const http_parser *parser)
{
    if (parser->type == HTTP_REQUEST) {
        return 0;
    }
    
    /* See RFC 2616 section 4.4 */
    if (parser->status_code / 100 == 1 || /* 1xx e.g. Continue */
        parser->status_code == 204 ||     /* No Content */
        parser->status_code == 304 ||     /* Not Modified */
        parser->flags & F_SKIPBODY) {     
        /* response to a HEAD request */
        return 0;
    }
    
    if ((parser->flags & F_CHUNKED) || parser->content_length != HTTP_UINT32_MAX) {
        return 0;
    }
    
    return 1;
}
        

static enum state parse_url_char(enum state s, const char ch)
{
    if (ch==' '||ch=='\r'||ch=='\n'||ch=='\t'||ch=='\f') {
        return s_dead;
    }
    
    switch(s) 
    {
    case s_req_spaces_before_url:
        /* Proxied requests are followed by scheme of an absolute URI (alpha).
         * All methods except CONNECT are followed by '/' or '*'.
        */        
        if (ch == '/' || ch == '*')
        {
            return s_req_path;
        }        
        if (IS_ALPHA(ch))
        {
            return s_req_schema;
        }        
        break;        
    case s_req_schema:
        if (IS_ALPHA(ch)) 
        {
            return s;
        }        
        if (ch == ':') 
        {
            return s_req_schema_slash;
        }
        break;        
    case s_req_schema_slash:
        if (ch == '/') 
        {
            return s_req_schema_slash_slash;
        }        
        break;        
    case s_req_schema_slash_slash:
        if (ch == '/') 
        {
            return s_req_server_start;
        }        
        break;        
    case s_req_server_with_at:
        if (ch == '@') 
        {
            return s_dead;
        }        
        /* FALLTHROUGH */
    case s_req_server_start:
    case s_req_server:
        if (ch == '/') 
        {
            return s_req_path;
        }         
        if (ch == '?') 
        {
            return s_req_query_string_start;
        }           
        if (ch == '@') 
        {
            return s_req_server_with_at;
        }        
        if (IS_USERINFO_CHAR(ch) || ch == '[' || ch == ']')
        {
            return s_req_server;
        }
        break;        
    case s_req_path:
        if (IS_URL_CHAR(ch)) 
        {
            return s;
        }        
        switch (ch) 
        {
        case '?':
            return s_req_query_string_start;            
        case '#':
            return s_req_fragment_start;
        }        
        break;        
    case s_req_query_string_start:
    case s_req_query_string:
        if (IS_URL_CHAR(ch)) 
        {
            return s_req_query_string;
        }            
        switch (ch) 
        {
        case '?':
            /* allow extra '?' in query string */
            return s_req_query_string;                
        case '#':
            return s_req_fragment_start;
        }            
        break;            
    case s_req_fragment_start:
        if (IS_URL_CHAR(ch)) 
        {
            return s_req_fragment;
        }                
        switch (ch) 
        {
        case '?':
            return s_req_fragment;                    
        case '#':
            return s;
        }                
        break;                
    case s_req_fragment:
        if (IS_URL_CHAR(ch)) 
        {
            return s;
        }                    
        switch (ch) 
        {
        case '?':
        case '#':
            return s;
        }                    
        break;                    
    default:
        break;
    }

    return s_dead;
}

int http_parser_execute (http_parser *parser, const http_parser_settings *settings, const char *data, int len)
{
    char c, ch;
    char unhex_val;
    const char *p = data;
    const char *header_field_mark = 0;
    const char *header_value_mark = 0;
    const char *url_mark = 0;
    const char *body_mark = 0;
    
    /* We're in an error state. Don't bother doing anything. */
    if (HTTP_PARSER_ERRNO(parser) != HPE_OK) 
    {
        return 0;
    }
    
    if (len == 0) 
    {
        switch (parser->state)
        {
        case s_body_identity_eof:
            /* Use of CALLBACK_NOTIFY() here would erroneously return 1 byte read if
            * we got paused.
            */
            CALLBACK_NOTIFY_NOADVANCE(message_complete);
            return 0;            
        case s_dead:
        case s_start_req_or_res:
        case s_start_res:
        case s_start_req:
            return 0;            
        default:
            SET_ERRNO(HPE_INVALID_EOF_STATE);
            return 1;
        }
    }
        
    if (parser->state == s_header_field)
        header_field_mark = data;
    if (parser->state == s_header_value)
        header_value_mark = data;
    switch (parser->state) 
    {
    case s_req_path:
    case s_req_schema:
    case s_req_schema_slash:
    case s_req_schema_slash_slash:
    case s_req_server_start:
    case s_req_server:
    case s_req_server_with_at:
    case s_req_query_string_start:
    case s_req_query_string:
    case s_req_fragment_start:
    case s_req_fragment:
        url_mark = data;
        break;
    }
    
    for (p=data; p != data + len; p++) 
    {
        ch = *p;        
        if (parser->state <= s_headers_done) 
        {
            ++parser->nread;
            /* Buffer overflow attack */
            if (parser->nread > HTTP_MAX_HEADER_SIZE) 
            {
                SET_ERRNO(HPE_HEADER_OVERFLOW);
                goto error;
            }
        }
        
reexecute_byte:
        switch (parser->state) 
        {            
        case s_dead:
            /* this state is used after a 'Connection: close' message
            * the parser will error out if it reads another message
            */
            if (ch == CR || ch == LF)
                break;            
            SET_ERRNO(HPE_CLOSED_CONNECTION);
            goto error;            
        case s_start_req_or_res:
            {
                if (ch == CR || ch == LF)
                {
                    break;
                }
                parser->flags = 0;
                parser->content_length = HTTP_UINT32_MAX;
                
                if (ch == 'H') 
                {
                    parser->state = s_res_or_resp_H;
                    
                    CALLBACK_NOTIFY(message_begin);
                } 
                else 
                {
                    parser->type = HTTP_REQUEST;
                    parser->state = s_start_req;
                    goto reexecute_byte;
                }                
                break;
            }            
        case s_res_or_resp_H:
            if (ch == 'T') {
                parser->type = HTTP_RESPONSE;
                parser->state = s_res_HT;
            } else {
                if (ch != 'E') {
                    SET_ERRNO(HPE_INVALID_CONSTANT);
                    goto error;
                }
                
                parser->type = HTTP_REQUEST;
                parser->method = HTTP_HEAD;
                parser->index = 2;
                parser->state = s_req_method;
            }
            break;            
        case s_start_res:
            {
                parser->flags = 0;
                parser->content_length = HTTP_UINT32_MAX;
                
                switch (ch) {
                case 'H':
                    parser->state = s_res_H;
                    break;
                    
                case CR:
                case LF:
                    break;
                    
                default:
                    SET_ERRNO(HPE_INVALID_CONSTANT);
                    goto error;
                }
                
                CALLBACK_NOTIFY(message_begin);
                break;
            }            
        case s_res_H:
            STRICT_CHECK(ch != 'T');
            parser->state = s_res_HT;
            break;            
        case s_res_HT:
            STRICT_CHECK(ch != 'T');
            parser->state = s_res_HTT;
            break;            
        case s_res_HTT:
            STRICT_CHECK(ch != 'P');
            parser->state = s_res_HTTP;
            break;            
        case s_res_HTTP:
            STRICT_CHECK(ch != '/');
            parser->state = s_res_first_http_major;
            break;            
        case s_res_first_http_major:
            if (ch < '0' || ch > '9') {
                SET_ERRNO(HPE_INVALID_VERSION);
                goto error;
            }            
            parser->http_major = ch - '0';
            parser->state = s_res_http_major;
            break;            
            /* major HTTP version or dot */
        case s_res_http_major:
            {
                if (ch == '.') {
                    parser->state = s_res_first_http_minor;
                    break;
                }
                
                if (!IS_NUM(ch)) {
                    SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }
                
                parser->http_major *= 10;
                parser->http_major += ch - '0';
                
                if (parser->http_major > 999) {
                    SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }                
                break;
            }            
            /* first digit of minor HTTP version */
        case s_res_first_http_minor:
            if (!IS_NUM(ch)) {
                SET_ERRNO(HPE_INVALID_VERSION);
                goto error;
            }            
            parser->http_minor = ch - '0';
            parser->state = s_res_http_minor;
            break;            
            /* minor HTTP version or end of request line */
        case s_res_http_minor:
            {
                if (ch == ' ') {
                    parser->state = s_res_first_status_code;
                    break;
                }
                
                if (!IS_NUM(ch)) {
                    SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }
                
                parser->http_minor *= 10;
                parser->http_minor += ch - '0';
                
                if (parser->http_minor > 999) {
                    SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }                
                break;
            }            
        case s_res_first_status_code:
            {
                if (!IS_NUM(ch)) {
                    if (ch == ' ') {
                        break;
                    }
                    
                    SET_ERRNO(HPE_INVALID_STATUS);
                    goto error;
                }
                parser->status_code = ch - '0';
                parser->state = s_res_status_code;
                break;
            }            
        case s_res_status_code:
            {
                if (!IS_NUM(ch)) {
                    switch (ch) {
                    case ' ':
                        parser->state = s_res_status;
                        break;
                    case CR:
                        parser->state = s_res_line_almost_done;
                        break;
                    case LF:
                        parser->state = s_header_field_start;
                        break;
                    default:
                        SET_ERRNO(HPE_INVALID_STATUS);
                        goto error;
                    }
                    break;
                }
                
                parser->status_code *= 10;
                parser->status_code += ch - '0';
                
                if (parser->status_code > 999) {
                    SET_ERRNO(HPE_INVALID_STATUS);
                    goto error;
                }                
                break;
            }            
        case s_res_status:
        /* the human readable status. e.g. "NOT FOUND"
            * we are not humans so just ignore this */
            if (ch == CR) {
                parser->state = s_res_line_almost_done;
                break;
            }
            
            if (ch == LF) {
                parser->state = s_header_field_start;
                break;
            }
            break;            
        case s_res_line_almost_done:
            STRICT_CHECK(ch != LF);
            parser->state = s_header_field_start;
            CALLBACK_NOTIFY(status_complete);
            break;            
        case s_start_req:
            {
                if (ch == CR || ch == LF)
                    break;
                parser->flags = 0;
                parser->content_length = HTTP_UINT32_MAX;
                
                if (!IS_ALPHA(ch)) {
                    SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }
                
                parser->method = 0;
                parser->index = 1;
                switch (ch) {
                case 'C': parser->method = HTTP_CONNECT; /* or COPY, CHECKOUT */ break;
                case 'D': parser->method = HTTP_DELETE; break;
                case 'G': parser->method = HTTP_GET; break;
                case 'H': parser->method = HTTP_HEAD; break;
                case 'L': parser->method = HTTP_LOCK; break;
                case 'M': parser->method = HTTP_MKCOL; /* or MOVE, MKACTIVITY, MERGE, M-SEARCH */ break;
                case 'N': parser->method = HTTP_NOTIFY; break;
                case 'O': parser->method = HTTP_OPTIONS; break;
                case 'P': parser->method = HTTP_POST;
                    /* or PROPFIND|PROPPATCH|PUT|PATCH|PURGE */
                    break;
                case 'R': parser->method = HTTP_REPORT; break;
                case 'S': parser->method = HTTP_SUBSCRIBE; /* or SEARCH */ break;
                case 'T': parser->method = HTTP_TRACE; break;
                case 'U': parser->method = HTTP_UNLOCK; /* or UNSUBSCRIBE */ break;
                default:
                    SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }
                parser->state = s_req_method;                
                CALLBACK_NOTIFY(message_begin);                
                break;
            }            
        case s_req_method:
            {
                const char *matcher;
                if (ch == '\0') {
                    SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }
                
                matcher = method_strings[parser->method];
                if (ch == ' ' && matcher[parser->index] == '\0') {
                    parser->state = s_req_spaces_before_url;
                } else if (ch == matcher[parser->index]) {
                    ; /* nada */
                } else if (parser->method == HTTP_CONNECT) {
                    if (parser->index == 1 && ch == 'H') {
                        parser->method = HTTP_CHECKOUT;
                    } else if (parser->index == 2  && ch == 'P') {
                        parser->method = HTTP_COPY;
                    } else {
                        goto error;
                    }
                } else if (parser->method == HTTP_MKCOL) {
                    if (parser->index == 1 && ch == 'O') {
                        parser->method = HTTP_MOVE;
                    } else if (parser->index == 1 && ch == 'E') {
                        parser->method = HTTP_MERGE;
                    } else if (parser->index == 1 && ch == '-') {
                        parser->method = HTTP_MSEARCH;
                    } else if (parser->index == 2 && ch == 'A') {
                        parser->method = HTTP_MKACTIVITY;
                    } else {
                        goto error;
                    }
                } else if (parser->method == HTTP_SUBSCRIBE) {
                    if (parser->index == 1 && ch == 'E') {
                        parser->method = HTTP_SEARCH;
                    } else {
                        goto error;
                    }
                } else if (parser->index == 1 && parser->method == HTTP_POST) {
                    if (ch == 'R') {
                        parser->method = HTTP_PROPFIND; /* or HTTP_PROPPATCH */
                    } else if (ch == 'U') {
                        parser->method = HTTP_PUT; /* or HTTP_PURGE */
                    } else if (ch == 'A') {
                        parser->method = HTTP_PATCH;
                    } else {
                        goto error;
                    }
                } else if (parser->index == 2) {
                    if (parser->method == HTTP_PUT) {
                        if (ch == 'R') parser->method = HTTP_PURGE;
                    } else if (parser->method == HTTP_UNLOCK) {
                        if (ch == 'S') parser->method = HTTP_UNSUBSCRIBE;
                    }
                } else if (parser->index == 4 && parser->method == HTTP_PROPFIND && ch == 'P') {
                    parser->method = HTTP_PROPPATCH;
                } else {
                    SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }
                
                ++parser->index;
                break;
            }            
        case s_req_spaces_before_url:
            {
                if (ch == ' ') break;
                
                MARK(url);
                if (parser->method == HTTP_CONNECT) {
                    parser->state = s_req_server_start;
                }
                
                parser->state = parse_url_char((enum state)parser->state, ch);
                if (parser->state == s_dead) {
                    SET_ERRNO(HPE_INVALID_URL);
                    goto error;
                }                
                break;
            }            
        case s_req_schema:
        case s_req_schema_slash:
        case s_req_schema_slash_slash:
        case s_req_server_start:
            {
                switch (ch) {
                    /* No whitespace allowed here */
                case ' ':
                case CR:
                case LF:
                    SET_ERRNO(HPE_INVALID_URL);
                    goto error;
                default:
                    parser->state = parse_url_char((enum state)parser->state, ch);
                    if (parser->state == s_dead) {
                        SET_ERRNO(HPE_INVALID_URL);
                        goto error;
                    }
                }                
                break;
            }            
        case s_req_server:
        case s_req_server_with_at:
        case s_req_path:
        case s_req_query_string_start:
        case s_req_query_string:
        case s_req_fragment_start:
        case s_req_fragment:
            {
                switch (ch) {
                case ' ':
                    parser->state = s_req_http_start;
                    CALLBACK_DATA(url);
                    break;
                case CR:
                case LF:
                    parser->http_major = 0;
                    parser->http_minor = 9;
                    parser->state = (ch == CR) ?
s_req_line_almost_done :
                    s_header_field_start;
                    CALLBACK_DATA(url);
                    break;
                default:
                    parser->state = parse_url_char((enum state)parser->state, ch);
                    if (parser->state == s_dead) {
                        SET_ERRNO(HPE_INVALID_URL);
                        goto error;
                    }
                }
                break;
            }            
        case s_req_http_start:
            switch (ch) {
            case 'H':
                parser->state = s_req_http_H;
                break;
            case ' ':
                break;
            default:
                SET_ERRNO(HPE_INVALID_CONSTANT);
                goto error;
            }
            break;            
            case s_req_http_H:
                STRICT_CHECK(ch != 'T');
                parser->state = s_req_http_HT;
                break;                
            case s_req_http_HT:
                STRICT_CHECK(ch != 'T');
                parser->state = s_req_http_HTT;
                break;                
            case s_req_http_HTT:
                STRICT_CHECK(ch != 'P');
                parser->state = s_req_http_HTTP;
                break;                
            case s_req_http_HTTP:
                STRICT_CHECK(ch != '/');
                parser->state = s_req_first_http_major;
                break;                
                /* first digit of major HTTP version */
            case s_req_first_http_major:
                if (ch < '1' || ch > '9') {
                    SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }                
                parser->http_major = ch - '0';
                parser->state = s_req_http_major;
                break;                
                /* major HTTP version or dot */
            case s_req_http_major:
                {
                    if (ch == '.') {
                        parser->state = s_req_first_http_minor;
                        break;
                    }
                    
                    if (!IS_NUM(ch)) {
                        SET_ERRNO(HPE_INVALID_VERSION);
                        goto error;
                    }
                    
                    parser->http_major *= 10;
                    parser->http_major += ch - '0';
                    
                    if (parser->http_major > 999) {
                        SET_ERRNO(HPE_INVALID_VERSION);
                        goto error;
                    }                    
                    break;
                }                
                /* first digit of minor HTTP version */
            case s_req_first_http_minor:
                if (!IS_NUM(ch)) {
                    SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }                
                parser->http_minor = ch - '0';
                parser->state = s_req_http_minor;
                break;                
                /* minor HTTP version or end of request line */
            case s_req_http_minor:
                {
                    if (ch == CR) {
                        parser->state = s_req_line_almost_done;
                        break;
                    }
                    
                    if (ch == LF) {
                        parser->state = s_header_field_start;
                        break;
                    }

                    if (!IS_NUM(ch)) {
                        SET_ERRNO(HPE_INVALID_VERSION);
                        goto error;
                    }
                    
                    parser->http_minor *= 10;
                    parser->http_minor += ch - '0';
                    
                    if (parser->http_minor > 999) {
                        SET_ERRNO(HPE_INVALID_VERSION);
                        goto error;
                    }                    
                    break;
                }                
                /* end of request line */
            case s_req_line_almost_done:
                {
                    if (ch != LF) {
                        SET_ERRNO(HPE_LF_EXPECTED);
                        goto error;
                    }                    
                    parser->state = s_header_field_start;
                    break;
                }                
            case s_header_field_start:
                {
                    if (ch == CR) {
                        parser->state = s_headers_almost_done;
                        break;
                    }                    
                    if (ch == LF) {
                    /* they might be just sending \n instead of \r\n so this would be
                        * the second \n to denote the end of headers*/
                        parser->state = s_headers_almost_done;
                        goto reexecute_byte;
                    }                    
                    c = TOKEN(ch);
                    
                    if (!c) {
                        SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
                        goto error;
                    }
                    
                    MARK(header_field);                    
                    parser->index = 0;
                    parser->state = s_header_field;
                    
                    switch (c) {
                    case 'c':
                        parser->header_state = h_C;
                        break;                        
                    case 'p':
                        parser->header_state = h_matching_proxy_connection;
                        break;                        
                    case 't':
                        parser->header_state = h_matching_transfer_encoding;
                        break;                        
                    case 'u':
                        parser->header_state = h_matching_upgrade;
                        break;                        
                    default:
                        parser->header_state = h_general;
                        break;
                    }
                    break;
                }                
            case s_header_field:
                {
                    c = TOKEN(ch);                    
                    if (c) {
                        switch (parser->header_state) {
                        case h_general:
                            break;                            
                        case h_C:
                            parser->index++;
                            parser->header_state = (c == 'o' ? h_CO : h_general);
                            break;                            
                        case h_CO:
                            parser->index++;
                            parser->header_state = (c == 'n' ? h_CON : h_general);
                            break;                            
                        case h_CON:
                            parser->index++;
                            switch (c) {
                            case 'n':
                                parser->header_state = h_matching_connection;
                                break;
                            case 't':
                                parser->header_state = h_matching_content_length;
                                break;
                            default:
                                parser->header_state = h_general;
                                break;
                            }
                            break;                            
                            /* connection */                            
                            case h_matching_connection:
                                parser->index++;
                                if (parser->index > sizeof(CONNECTION)-1
                                    || c != CONNECTION[parser->index]) {
                                    parser->header_state = h_general;
                                } else if (parser->index == sizeof(CONNECTION)-2) {
                                    parser->header_state = h_connection;
                                }
                                break;                                
                                /* proxy-connection */                                
                            case h_matching_proxy_connection:
                                parser->index++;
                                if (parser->index > sizeof(PROXY_CONNECTION)-1
                                    || c != PROXY_CONNECTION[parser->index]) {
                                    parser->header_state = h_general;
                                } else if (parser->index == sizeof(PROXY_CONNECTION)-2) {
                                    parser->header_state = h_connection;
                                }
                                break;                                
                                /* content-length */                                
                            case h_matching_content_length:
                                parser->index++;
                                if (parser->index > sizeof(CONTENT_LENGTH)-1
                                    || c != CONTENT_LENGTH[parser->index]) {
                                    parser->header_state = h_general;
                                } else if (parser->index == sizeof(CONTENT_LENGTH)-2) {
                                    parser->header_state = h_content_length;
                                }
                                break;                                
                                /* transfer-encoding */                                
                            case h_matching_transfer_encoding:
                                parser->index++;
                                if (parser->index > sizeof(TRANSFER_ENCODING)-1
                                    || c != TRANSFER_ENCODING[parser->index]) {
                                    parser->header_state = h_general;
                                } else if (parser->index == sizeof(TRANSFER_ENCODING)-2) {
                                    parser->header_state = h_transfer_encoding;
                                }
                                break;                                
                                /* upgrade */                                
                            case h_matching_upgrade:
                                parser->index++;
                                if (parser->index > sizeof(UPGRADE)-1
                                    || c != UPGRADE[parser->index]) {
                                    parser->header_state = h_general;
                                } else if (parser->index == sizeof(UPGRADE)-2) {
                                    parser->header_state = h_upgrade;
                                }
                                break;                                
                            case h_connection:
                            case h_content_length:
                            case h_transfer_encoding:
                            case h_upgrade:
                                if (ch != ' ') parser->header_state = h_general;
                                break;                                
                            default:
                                break;
                        }
                        break;
        }
        
        if (ch == ':') {
            parser->state = s_header_value_start;
            CALLBACK_DATA(header_field);
            break;
        }
        
        if (ch == CR) {
            parser->state = s_header_almost_done;
            CALLBACK_DATA(header_field);
            break;
        }
        
        if (ch == LF) {
            parser->state = s_header_field_start;
            CALLBACK_DATA(header_field);
            break;
        }
        
        SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
        goto error;
      }
      
      case s_header_value_start:
          {
              if (ch == ' ' || ch == '\t') 
                  break;
              
              MARK(header_value);              
              parser->state = s_header_value;
              parser->index = 0;
              
              if (ch == CR) {
                  parser->header_state = h_general;
                  parser->state = s_header_almost_done;
                  CALLBACK_DATA(header_value);
                  break;
              }
              
              if (ch == LF) {
                  parser->state = s_header_field_start;
                  CALLBACK_DATA(header_value);
                  break;
              }
              
              c = LOWER(ch);              
              switch (parser->header_state) {
              case h_upgrade:
                  parser->flags |= F_UPGRADE;
                  parser->header_state = h_general;
                  break;                  
              case h_transfer_encoding:
                  /* looking for 'Transfer-Encoding: chunked' */
                  if ('c' == c) {
                      parser->header_state = h_matching_transfer_encoding_chunked;
                  } else {
                      parser->header_state = h_general;
                  }
                  break;                  
              case h_content_length:
                  if (!IS_NUM(ch)) {
                      SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                      goto error;
                  }                  
                  parser->content_length = ch - '0';
                  break;                  
              case h_connection:
                  /* looking for 'Connection: keep-alive' */
                  if (c == 'k') {
                      parser->header_state = h_matching_connection_keep_alive;
                      /* looking for 'Connection: close' */
                  } else if (c == 'c') {
                      parser->header_state = h_matching_connection_close;
                  } else {
                      parser->header_state = h_general;
                  }
                  break;                  
              default:
                  parser->header_state = h_general;
                  break;
              }
              break;
          }          
      case s_header_value:
          {              
              if (ch == CR) {
                  parser->state = s_header_almost_done;
                  CALLBACK_DATA(header_value);
                  break;
              }
              
              if (ch == LF) {
                  parser->state = s_header_almost_done;
                  CALLBACK_DATA_NOADVANCE(header_value);
                  goto reexecute_byte;
              }
              
              c = LOWER(ch);              
              switch (parser->header_state) 
              {
              case h_general:
                  break;                  
              case h_connection:
              case h_transfer_encoding:
                  break;                  
              case h_content_length:
                  {
                      unsigned int t = 0;
                      
                      if (ch == ' ') 
                          break;
                      
                      if (!IS_NUM(ch)) {
                          SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                          goto error;
                      }
                      
                      t = parser->content_length;
                      t *= 10;
                      t += ch - '0';
                      
                      /* Overflow? */
                      if (t < parser->content_length || t == HTTP_UINT32_MAX) {
                          SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                          goto error;
                      }                      
                      parser->content_length = t;
                      break;
                  }                  
                  /* Transfer-Encoding: chunked */
              case h_matching_transfer_encoding_chunked:
                  parser->index++;
                  if (parser->index > sizeof(CHUNKED)-1
                      || c != CHUNKED[parser->index]) {
                      parser->header_state = h_general;
                  } else if (parser->index == sizeof(CHUNKED)-2) {
                      parser->header_state = h_transfer_encoding_chunked;
                  }
                  break;                  
                  /* looking for 'Connection: keep-alive' */
              case h_matching_connection_keep_alive:
                  parser->index++;
                  if (parser->index > sizeof(KEEP_ALIVE)-1
                      || c != KEEP_ALIVE[parser->index]) {
                      parser->header_state = h_general;
                  } else if (parser->index == sizeof(KEEP_ALIVE)-2) {
                      parser->header_state = h_connection_keep_alive;
                  }
                  break;                  
                  /* looking for 'Connection: close' */
              case h_matching_connection_close:
                  parser->index++;
                  if (parser->index > sizeof(CLOSE)-1 || c != CLOSE[parser->index]) {
                      parser->header_state = h_general;
                  } else if (parser->index == sizeof(CLOSE)-2) {
                      parser->header_state = h_connection_close;
                  }
                  break;                  
              case h_transfer_encoding_chunked:
              case h_connection_keep_alive:
              case h_connection_close:
                  if (ch != ' ') parser->header_state = h_general;
                  break;                  
              default:
                  parser->state = s_header_value;
                  parser->header_state = h_general;
                  break;
              }
          } 
          break;
      case s_header_almost_done:
          {
              STRICT_CHECK(ch != LF);
              
              parser->state = s_header_value_lws;
              
              switch (parser->header_state) {
              case h_connection_keep_alive:
                  parser->flags |= F_CONNECTION_KEEP_ALIVE;
                  break;
              case h_connection_close:
                  parser->flags |= F_CONNECTION_CLOSE;
                  break;
              case h_transfer_encoding_chunked:
                  parser->flags |= F_CHUNKED;
                  break;
              default:
                  break;
              }
          }    
          break;
      case s_header_value_lws:
          {
              if (ch == ' ' || ch == '\t')
                  parser->state = s_header_value_start;
              else
              {
                  parser->state = s_header_field_start;
                  goto reexecute_byte;
              }              
          } 
          break;
      case s_headers_almost_done:
          {
              STRICT_CHECK(ch != LF);
              if (parser->flags & F_TRAILING) {
                  /* End of a chunked request */
                  parser->state = NEW_MESSAGE();
                  CALLBACK_NOTIFY(message_complete);
                  break;
              }              
              parser->state = s_headers_done;              
              /* Set this here so that on_headers_complete() callbacks can see it */
              parser->upgrade = (parser->flags & F_UPGRADE || parser->method == HTTP_CONNECT);
              
              if (settings->on_headers_complete) {
                  switch (settings->on_headers_complete(parser)) {
                  case 0:
                      break;
                      
                  case 1:
                      parser->flags |= F_SKIPBODY;
                      break;
                      
                  default:
                      SET_ERRNO(HPE_CB_headers_complete);
                      return p - data; /* Error */
                  }
              }              
              if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {
                  return p - data;
              }              
              goto reexecute_byte;
          }
          
      case s_headers_done:
          {
              STRICT_CHECK(ch != LF);              
              parser->nread = 0;              
              /* Exit, the rest of the connect is in a different protocol. */
              if (parser->upgrade) {
                  parser->state = NEW_MESSAGE();
                  CALLBACK_NOTIFY(message_complete);
                  return (p - data) + 1;
              }
              
              if (parser->flags & F_SKIPBODY) {
                  parser->state = NEW_MESSAGE();
                  CALLBACK_NOTIFY(message_complete);
              } else if (parser->flags & F_CHUNKED) {
                  /* chunked encoding - ignore Content-Length header */
                  parser->state = s_chunk_size_start;
              } else {
                  if (parser->content_length == 0) {
                      /* Content-Length header given but zero: Content-Length: 0\r\n */
                      parser->state = NEW_MESSAGE();
                      CALLBACK_NOTIFY(message_complete);
                  } else if (parser->content_length != HTTP_UINT32_MAX) {
                      /* Content-Length header given and non-zero */
                      parser->state = s_body_identity;
                  } else {
                      if (parser->type == HTTP_REQUEST ||
                          !http_message_needs_eof(parser)) {
                          /* Assume content-length 0 - read the next */
                          parser->state = NEW_MESSAGE();
                          CALLBACK_NOTIFY(message_complete);
                      } else {
                          /* Read body until EOF */
                          parser->state = s_body_identity_eof;
                      }
                  }
              }
          }
          break;
      case s_body_identity:
          {
              unsigned int to_read = __min(parser->content_length, (unsigned int)((data + len)-p));
                            
              MARK(body);
              parser->content_length -= to_read;
              p += to_read - 1;
              
              if (parser->content_length == 0) {
                  parser->state = s_message_done;
                  CALLBACK_DATA_(body, p - body_mark + 1, p - data);
                  goto reexecute_byte;
              }                 
          }
          break;
      case s_body_identity_eof:
          MARK(body);
          p = data + len - 1;          
          break;
      case s_message_done:
          parser->state = NEW_MESSAGE();
          CALLBACK_NOTIFY(message_complete);
          break;          
      case s_chunk_size_start:
          {
              unhex_val = unhex[(unsigned char)ch];
              if (unhex_val == -1) {
                  SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
                  goto error;
              }              
              parser->content_length = unhex_val;
              parser->state = s_chunk_size;              
          }      
          break;
      case s_chunk_size:
          {
              unsigned int t = 0;
              
              if (ch == CR) {
                  parser->state = s_chunk_size_almost_done;
                  break;
              }              
              unhex_val = unhex[(unsigned char)ch];              
              if (unhex_val == -1) {
                  if (ch == ';' || ch == ' ') {
                      parser->state = s_chunk_parameters;
                      break;
                  }                  
                  SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
                  goto error;
              }              
              t = parser->content_length;
              t *= 16;
              t += unhex_val;              
              /* Overflow? */
              if (t < parser->content_length || t == HTTP_UINT32_MAX) {
                  SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                  goto error;
              }              
              parser->content_length = t;              
          }       
          break;
      case s_chunk_parameters:
          {
              /* just ignore this shit. TODO check for overflow */
              if (ch == CR) {
                  parser->state = s_chunk_size_almost_done;
                  break;
              }              
          }  
          break;
      case s_chunk_size_almost_done:
          {
              STRICT_CHECK(ch != LF);              
              parser->nread = 0;              
              if (parser->content_length == 0) {
                  parser->flags |= F_TRAILING;
                  parser->state = s_header_field_start;
              } else {
                  parser->state = s_chunk_data;
              }
              break;
          }          
      case s_chunk_data:
          {
              unsigned int to_read = __min(parser->content_length, (unsigned int)((data + len)-p));
              
              /* See the explanation in s_body_identity for why the content
              * length and data pointers are managed this way.
              */
              MARK(body);
              parser->content_length -= to_read;
              p += to_read - 1;
              
              if (parser->content_length == 0) {
                  parser->state = s_chunk_data_almost_done;
              }
          }
          break;
      case s_chunk_data_almost_done:
          STRICT_CHECK(ch != CR);
          parser->state = s_chunk_data_done;
          CALLBACK_DATA(body);
          break;          
      case s_chunk_data_done:
          STRICT_CHECK(ch != LF);
          parser->nread = 0;
          parser->state = s_chunk_size_start;
          break;          
      default:
          SET_ERRNO(HPE_INVALID_INTERNAL_STATE);
          goto error;
    }
  }
    
  CALLBACK_DATA_NOADVANCE(header_field);
  CALLBACK_DATA_NOADVANCE(header_value);
  CALLBACK_DATA_NOADVANCE(url);
  CALLBACK_DATA_NOADVANCE(body);
  
  return len;
  
error:
  if (HTTP_PARSER_ERRNO(parser) == HPE_OK) {
      SET_ERRNO(HPE_UNKNOWN);
  }
  
  return (p - data);
}

int
http_should_keep_alive(const http_parser *parser)
{
    if (parser->http_major > 0 && parser->http_minor > 0) {
        /* HTTP/1.1 */
        if (parser->flags & F_CONNECTION_CLOSE){
            return 0;
        }
    } else {
        /* HTTP/1.0 or earlier */
        if (!(parser->flags & F_CONNECTION_KEEP_ALIVE)) {
            return 0;
        }
    }    
    return !http_message_needs_eof(parser);
}

void
http_parser_init (http_parser *parser, enum http_parser_type t)
{
    void *data = parser->data;

    memset(parser, 0, sizeof(*parser));
    parser->data = data;
    parser->type = t;
    parser->state = (t == HTTP_REQUEST ? s_start_req : (t == HTTP_RESPONSE ? s_start_res : s_start_req_or_res));
    parser->http_errno = HPE_OK;
}



/*
 *	smart funcs for http module
 */
char* we_url_make_header(int iMethod/*0-GET,1-POST*/,char *pszURL, int postLen/*valid when POST*/)
{
    static char *fmtHdrPost = "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded; charset=UTF-8\r\nContent-Length: %d\r\nUser-Agent: MBrowser\r\nAccept: */*\r\n\r\n";
    static char *fmtHdrGet = "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: MBrowser\nAccept: */*\r\n\r\n";
    char *hostport = we_url_hostport(pszURL);
    char *pszPath = we_url_path(pszURL);	
    int iHdrLen = strlen(fmtHdrPost)+strlen(pszPath)+strlen(hostport)+16+1; /* PST-Len is Bigger*/
    char* pszHeader = (char*)malloc(iHdrLen);
    memset(pszHeader, 0x00, iHdrLen);
    if(HTTP_GET==iMethod){
        sprintf(pszHeader, fmtHdrGet,pszPath,hostport);
    }else{
        sprintf(pszHeader, fmtHdrPost,pszPath,hostport,postLen);
    }
    return pszHeader;
}


static char* _url_host_port_(char* url, unsigned short* port, unsigned short def)
{
    static char host[128] = {0};
    int i, j;
    
    memset(host,0x00,128);
    if(port){
        *port = def;
    }
    for(i = j = 0; url[i]; i++){
        if(url[i] == ':' && url[i+1] == '/' && url[i+2] == '/'){
            i+=3;
            while(url[i] && url[i]!=':' && url[i]!='/'){
                host[j++] = url[i++];
            }            
            if(port && url[i] == ':'){
                *port = atoi(url+i+1);
            }
            break;
        }
    }    
    host[j] = 0;
    return host;	
}


char* we_url_host(char* url)
{
    return _url_host_port_(url, NULL, 0);
}

unsigned short we_url_port(char* url, int def)
{
    unsigned short usPort = 0;
    _url_host_port_(url, &usPort, def);
    return usPort;
}

char* we_url_path(char* url)
{
    char *pcPath = NULL;
    pcPath = strstr(url, "//" );
    if(pcPath){
        pcPath += 2;            
        pcPath = strstr(pcPath,"/");
    }else{
        pcPath = strstr(url, "/");
    }
    if(!pcPath){
        pcPath = "/";
    }    
    return pcPath;
}

char* we_url_hostport(char* url)
{
    static char szHostPort[128] = {0};
    
    char *pcTemp = strstr(url, "//");
    char *pcTemp2=NULL;
    
    if(!pcTemp){
        return NULL;
    }
    pcTemp += 2; // skip//
    pcTemp2 = strstr(pcTemp, "/");
    if(!pcTemp2){
        pcTemp2=pcTemp+strlen(pcTemp);
    }
    
    memset(szHostPort,0x00,128);
    memcpy(szHostPort,pcTemp,pcTemp2-pcTemp);
    
    return szHostPort;
}
