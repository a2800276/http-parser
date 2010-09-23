/* Copyright 2009,2010 Ryan Dahl <ry@tinyclouds.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <http_parser.h>
#ifdef _WIN32
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int16 int32_t;
typedef unsigned __int32 uint32_t;
#else
#include <stdint.h>
#endif
#include <assert.h>
#include <stddef.h>


#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif


#define MARK(FOR)  (FOR##_mark = p)

#define RECORD_MARK_NOCLEAR(FOR)                  \
do {                                              \
  if (FOR##_mark) {                               \
    assert(data_index + 1 < data_len);            \
    data[data_index].payload.string.p = FOR##_mark; \
    data[data_index].payload.string.len = p - FOR##_mark; \
    data[data_index].type = HTTP_##FOR;           \
    data_index++;                                 \
  }                                               \
} while (0)

#define RECORD_BODY(LEN)                          \
do {                                              \
  assert(data_index + 1 < data_len);              \
  data[data_index].payload.string.p = p;          \
  data[data_index].payload.string.len = (LEN);    \
  data[data_index].type = HTTP_BODY;              \
  data_index++;                                   \
} while (0)

#define RECORD_MARK(FOR)                          \
do {                                              \
  RECORD_MARK_NOCLEAR(FOR);                       \
  FOR##_mark = NULL;                              \
} while (0)

#define RECORD(FOR)                               \
do {                                              \
  assert(data_index + 1 < data_len);              \
  data[data_index].payload.string.p = p+1;        \
  data[data_index].payload.string.len = 0;        \
  data[data_index].type = HTTP_##FOR;             \
  data_index++;                                   \
} while (0)



#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"


static const char *method_strings[] =
  { "DELETE"
  , "GET"
  , "HEAD"
  , "POST"
  , "PUT"
  , "CONNECT"
  , "OPTIONS"
  , "TRACE"
  , "COPY"
  , "LOCK"
  , "MKCOL"
  , "MOVE"
  , "PROPFIND"
  , "PROPPATCH"
  , "UNLOCK"
  , "REPORT"
  , "MKACTIVITY"
  , "CHECKOUT"
  , "MERGE"
  };


/* ' ', '_', '-' and all alpha-numeric ascii characters are accepted by acceptable_header.
   The 'A'-'Z' are lower-cased.  */
static const char acceptable_header[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
       ' ',      0,       0,       0,       0,       0,       0,       0,
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        0,       0,       0,       0,       0,      '-',      0,       0,
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
       'x',     'y',     'z',      0,       0,       0,       0,      '_',
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
        0,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
       'x',     'y',     'z',      0,       0,       0,       0,       0 };


static const int8_t unhex[256] =
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };


static const uint8_t normal_url_char[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
        0,       1,       1,       0,       1,       1,       1,       1,
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        1,       1,       1,       1,       1,       1,       1,       1,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
        1,       1,       1,       1,       1,       1,       1,       1,
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
        1,       1,       1,       1,       1,       1,       1,       0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
        1,       1,       1,       1,       1,       1,       1,       1,
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
        1,       1,       1,       1,       1,       1,       1,       1,
/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
        1,       1,       1,       1,       1,       1,       1,       1,
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
        1,       1,       1,       1,       1,       1,       1,       1,
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
        1,       1,       1,       1,       1,       1,       1,       1,
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
        1,       1,       1,       1,       1,       1,       1,       1,
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
        1,       1,       1,       1,       1,       1,       1,       1,
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
        1,       1,       1,       1,       1,       1,       1,       0 };


enum state
  { s_dead = 1 /* important that this is > 0 */

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
  , s_req_host
  , s_req_port
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

  , s_header_almost_done

  , s_headers_almost_done
  /* Important: 's_headers_almost_done' must be the last 'header' state. All
   * states beyond this must be 'body' states. It is used for overflow
   * checking. See the PARSING_HEADER() macro.
   */

  /* Fake state for responses which get punted out of http_parser_execute2 */
  , s_decide_body

  , s_chunk_size_start
  , s_chunk_size
  , s_chunk_size_almost_done
  , s_chunk_parameters
  , s_chunk_data
  , s_chunk_data_almost_done
  , s_chunk_data_done

  , s_body_identity
  , s_body_identity_eof
  };


#define PARSING_HEADER(state) (state <= s_headers_almost_done && 0 == (parser->flags & F_TRAILING))


enum header_states
  { h_general = 0
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


enum flags
  { F_CHUNKED               = 1 << 0
  , F_CONNECTION_KEEP_ALIVE = 1 << 1
  , F_CONNECTION_CLOSE      = 1 << 2
  , F_TRAILING              = 1 << 3
  , F_UPGRADE               = 1 << 4
  , F_SKIPBODY              = 1 << 5
  };


#define CR '\r'
#define LF '\n'
#define LOWER(c) (unsigned char)(c | 0x20)


#define start_state (parser->type == HTTP_REQUEST ? s_start_req : s_start_res)


#if HTTP_PARSER_STRICT
# define STRICT_CHECK(cond) if (cond) goto error
# define NEW_MESSAGE() (http_should_keep_alive(parser) ? start_state : s_dead)
#else
# define STRICT_CHECK(cond)
# define NEW_MESSAGE() start_state
#endif


static inline
int body_logic (http_parser *parser,
                const char *p,
                http_parser_data data[],
                int data_len,
                int *data_index_)
{
  int state; 
  int data_index = *data_index_;

  if (parser->flags & F_SKIPBODY) {
    RECORD(MESSAGE_END);
    state = NEW_MESSAGE();
  } else if (parser->flags & F_CHUNKED) {
    /* chunked encoding - ignore Content-Length header */
    state = s_chunk_size_start;
  } else {
    if (parser->content_length == 0) {
      /* Content-Length header given but zero: Content-Length: 0\r\n */
      RECORD(MESSAGE_END);
      state = NEW_MESSAGE();
    } else if (parser->content_length > 0) {
      /* Content-Length header given and non-zero */
      state = s_body_identity;
    } else {
      if (parser->type == HTTP_REQUEST || http_should_keep_alive(parser)) {
        /* Assume content-length 0 - read the next */
        RECORD(MESSAGE_END);
        state = NEW_MESSAGE();
      } else {
        /* Read body until EOF */
        state = s_body_identity_eof;
      }
    }
  }

  *data_index_ = data_index;
  return state;
}


int http_parser_execute2(http_parser *parser,
                         const char *buf,
                         size_t buf_len,
                         http_parser_data data[],
                         int data_len)
{
  char c, ch;
  const char *p = buf, *pe;
  int64_t to_read;
  int data_index = 0;

  enum state state = (enum state) parser->state;
  enum header_states header_state = (enum header_states) parser->header_state;
  uint64_t index = parser->index;
  uint64_t nread = parser->nread;

  if (data_len < 1) {
    assert(0 && "Must supply at least a few http_parser_data objects");
    return 0;
  }

  if (state == s_decide_body) {
    assert(parser->type == HTTP_RESPONSE);
    state = body_logic(parser, p, data, data_len, &data_index);
  } else if (buf_len == 0) {
    if (state == s_body_identity_eof) {
      RECORD(MESSAGE_END);
    }
    goto exit;
  }


  /* technically we could combine all of these (except for URL_mark) into one
     variable, saving stack space, but it seems more clear to have them
     separated. */
  const char *HEADER_FIELD_mark = 0;
  const char *HEADER_VALUE_mark = 0;
  const char *FRAGMENT_mark = 0;
  const char *QUERY_STRING_mark = 0;
  const char *PATH_mark = 0;
  const char *URL_mark = 0;

  if (state == s_header_field)
    HEADER_FIELD_mark = buf;
  if (state == s_header_value)
    HEADER_VALUE_mark = buf;
  if (state == s_req_fragment)
    FRAGMENT_mark = buf;
  if (state == s_req_query_string)
    QUERY_STRING_mark = buf;
  if (state == s_req_path)
    PATH_mark = buf;
  if (state == s_req_path || state == s_req_schema || state == s_req_schema_slash
      || state == s_req_schema_slash_slash || state == s_req_port
      || state == s_req_query_string_start || state == s_req_query_string
      || state == s_req_host
      || state == s_req_fragment_start || state == s_req_fragment)
    URL_mark = buf;

  for (p = buf, pe = buf + buf_len; p != pe; p++) {
    ch = *p;

    // We always need at least enough space to put two elements on the data. 
    if (data_index + 2 >= data_len) {
      data[data_index].payload.string.p = p;
      data[data_index].payload.string.len = 0;
      data[data_index].type = HTTP_NEEDS_DATA_ELEMENTS;
      data_index++;
      goto exit; 
    }

    if (PARSING_HEADER(state)) {
      ++nread;
      /* Buffer overflow attack */
      if (nread > HTTP_MAX_HEADER_SIZE) goto error;
    }

    switch (state) {

      case s_dead:
        /* this state is used after a 'Connection: close' message
         * the parser will error out if it reads another message
         */
        goto error;

      case s_start_req_or_res:
      {
        if (ch == CR || ch == LF)
          break;
        parser->flags = 0;
        parser->content_length = -1;

        if (ch == 'H')
          state = s_res_or_resp_H;
        else {
          parser->type = HTTP_REQUEST;
          goto start_req_method_assign;
        }
        break;
      }

      case s_res_or_resp_H:
        if (ch == 'T') {
          parser->type = HTTP_RESPONSE;
          state = s_res_HT;
        } else {
          if (ch != 'E') goto error;
          parser->type = HTTP_REQUEST;
          parser->method = HTTP_HEAD;
          index = 2;
          state = s_req_method;
        }
        break;

      case s_start_res:
      {
        parser->flags = 0;
        parser->content_length = -1;

        switch (ch) {
          case 'H':
            state = s_res_H;
            break;

          case CR:
          case LF:
            break;

          default:
            goto error;
        }
        break;
      }

      case s_res_H:
        STRICT_CHECK(ch != 'T');
        state = s_res_HT;
        break;

      case s_res_HT:
        STRICT_CHECK(ch != 'T');
        state = s_res_HTT;
        break;

      case s_res_HTT:
        STRICT_CHECK(ch != 'P');
        state = s_res_HTTP;
        break;

      case s_res_HTTP:
        STRICT_CHECK(ch != '/');
        state = s_res_first_http_major;
        break;

      case s_res_first_http_major:
        if (ch < '1' || ch > '9') goto error;
        parser->http_major = ch - '0';
        state = s_res_http_major;
        break;

      /* major HTTP version or dot */
      case s_res_http_major:
      {
        if (ch == '.') {
          state = s_res_first_http_minor;
          break;
        }

        if (ch < '0' || ch > '9') goto error;

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) goto error;
        break;
      }

      /* first digit of minor HTTP version */
      case s_res_first_http_minor:
        if (ch < '0' || ch > '9') goto error;
        parser->http_minor = ch - '0';
        state = s_res_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_res_http_minor:
      {
        if (ch == ' ') {
          state = s_res_first_status_code;
          break;
        }

        if (ch < '0' || ch > '9') goto error;

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) goto error;
        break;
      }

      case s_res_first_status_code:
      {
        if (ch < '0' || ch > '9') {
          if (ch == ' ') {
            break;
          }
          goto error;
        }
        parser->status_code = ch - '0';
        state = s_res_status_code;
        break;
      }

      case s_res_status_code:
      {
        if (ch < '0' || ch > '9') {

          /* assert the last data element, if any, was not an HTTP_VERSION. */
          assert(data_index == 0 || data[data_index-1].type != HTTP_VERSION);

          /* START OF RESPONSE */
          assert(data_index + 2 < data_len);
          data[data_index].type = HTTP_RES_MESSAGE_START;
          data[data_index].payload.status_code = parser->status_code;
          data_index++;

          data[data_index].type = HTTP_VERSION;
          data[data_index].payload.version.major = parser->http_major;
          data[data_index].payload.version.minor = parser->http_minor;
          data_index++;
          

          switch (ch) {
            case ' ':
              state = s_res_status;
              break;
            case CR:
              state = s_res_line_almost_done;
              break;
            case LF:
              state = s_header_field_start;
              break;
            default:
              goto error;
          }
          break;
        }

        parser->status_code *= 10;
        parser->status_code += ch - '0';

        if (parser->status_code > 999) goto error;
        break;
      }

      case s_res_status:
        /* the human readable status. e.g. "NOT FOUND"
         * we are not humans so just ignore this */
        if (ch == CR) {
          state = s_res_line_almost_done;
          break;
        }

        if (ch == LF) {
          state = s_header_field_start;
          break;
        }
        break;

      case s_res_line_almost_done:
        STRICT_CHECK(ch != LF);
        state = s_header_field_start;
        break;

      case s_start_req:
      {
        if (ch == CR || ch == LF)
          break;
        parser->flags = 0;
        parser->content_length = -1;

        if (ch < 'A' || 'Z' < ch) goto error;

      start_req_method_assign:
        parser->method = (enum http_method) 0;
        index = 1;
        switch (ch) {
          case 'C': parser->method = HTTP_CONNECT; /* or COPY, CHECKOUT */ break;
          case 'D': parser->method = HTTP_DELETE; break;
          case 'G': parser->method = HTTP_GET; break;
          case 'H': parser->method = HTTP_HEAD; break;
          case 'L': parser->method = HTTP_LOCK; break;
          case 'M': parser->method = HTTP_MKCOL; /* or MOVE, MKACTIVITY, MERGE */ break;
          case 'O': parser->method = HTTP_OPTIONS; break;
          case 'P': parser->method = HTTP_POST; /* or PROPFIND or PROPPATCH or PUT */ break;
          case 'R': parser->method = HTTP_REPORT; break;
          case 'T': parser->method = HTTP_TRACE; break;
          case 'U': parser->method = HTTP_UNLOCK; break;
          default: goto error;
        }
        state = s_req_method;
        break;
      }

      case s_req_method:
      {
        if (ch == '\0')
          goto error;

        const char *matcher = method_strings[parser->method];
        if (ch == ' ' && matcher[index] == '\0') {
          state = s_req_spaces_before_url;

          /* START OF REQUEST */
          assert(data_index + 1 < data_len);
          data[data_index].type = HTTP_REQ_MESSAGE_START;
          data[data_index].payload.method = parser->method;
          data_index++;

        } else if (ch == matcher[index]) {
          ; /* nada */
        } else if (parser->method == HTTP_CONNECT) {
          if (index == 1 && ch == 'H') {
            parser->method = HTTP_CHECKOUT;
          } else if (index == 2  && ch == 'P') {
            parser->method = HTTP_COPY;
          }
        } else if (parser->method == HTTP_MKCOL) {
          if (index == 1 && ch == 'O') {
            parser->method = HTTP_MOVE;
          } else if (index == 1 && ch == 'E') {
            parser->method = HTTP_MERGE;
          } else if (index == 2 && ch == 'A') {
            parser->method = HTTP_MKACTIVITY;
          }
        } else if (index == 1 && parser->method == HTTP_POST && ch == 'R') {
          parser->method = HTTP_PROPFIND; /* or HTTP_PROPPATCH */
        } else if (index == 1 && parser->method == HTTP_POST && ch == 'U') {
          parser->method = HTTP_PUT;
        } else if (index == 4 && parser->method == HTTP_PROPFIND && ch == 'P') {
          parser->method = HTTP_PROPPATCH;
        } else {
          goto error;
        }

        ++index;
        break;
      }
      case s_req_spaces_before_url:
      {
        if (ch == ' ') break;

        if (ch == '/') {
          MARK(URL);
          MARK(PATH);
          state = s_req_path;
          break;
        }

        c = LOWER(ch);

        if (c >= 'a' && c <= 'z') {
          MARK(URL);
          state = s_req_schema;
          break;
        }

        goto error;
      }

      case s_req_schema:
      {
        c = LOWER(ch);

        if (c >= 'a' && c <= 'z') break;

        if (ch == ':') {
          state = s_req_schema_slash;
          break;
        } else if (ch == '.') {
          state = s_req_host;
          break;
        }

        goto error;
      }

      case s_req_schema_slash:
        STRICT_CHECK(ch != '/');
        state = s_req_schema_slash_slash;
        break;

      case s_req_schema_slash_slash:
        STRICT_CHECK(ch != '/');
        state = s_req_host;
        break;

      case s_req_host:
      {
        c = LOWER(ch);
        if (c >= 'a' && c <= 'z') break;
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') break;
        switch (ch) {
          case ':':
            state = s_req_port;
            break;
          case '/':
            MARK(PATH);
            state = s_req_path;
            break;
          case ' ':
            /* The request line looks like:
             *   "GET http://foo.bar.com HTTP/1.1"
             * That is, there is no path.
             */
            RECORD_MARK(URL);
            state = s_req_http_start;
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_port:
      {
        if (ch >= '0' && ch <= '9') break;
        switch (ch) {
          case '/':
            MARK(PATH);
            state = s_req_path;
            break;
          case ' ':
            /* The request line looks like:
             *   "GET http://foo.bar.com:1234 HTTP/1.1"
             * That is, there is no path.
             */
            RECORD_MARK(URL);
            state = s_req_http_start;
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_path:
      {
        if (normal_url_char[(unsigned char)ch]) break;

        switch (ch) {
          case ' ':
            RECORD_MARK(URL);
            RECORD_MARK(PATH);
            state = s_req_http_start;
            break;
          case CR:
            RECORD_MARK(URL);
            RECORD_MARK(PATH);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            RECORD_MARK(URL);
            RECORD_MARK(PATH);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '?':
            RECORD_MARK(PATH);
            state = s_req_query_string_start;
            break;
          case '#':
            RECORD_MARK(PATH);
            state = s_req_fragment_start;
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_query_string_start:
      {
        if (normal_url_char[(unsigned char)ch]) {
          MARK(QUERY_STRING);
          state = s_req_query_string;
          break;
        }

        switch (ch) {
          case '?':
            break; /* XXX ignore extra '?' ... is this right? */
          case ' ':
            RECORD_MARK(URL);
            state = s_req_http_start;
            break;
          case CR:
            RECORD_MARK(URL);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            RECORD_MARK(URL);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '#':
            state = s_req_fragment_start;
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_query_string:
      {
        if (normal_url_char[(unsigned char)ch]) break;

        switch (ch) {
          case '?':
            /* allow extra '?' in query string */
            break;
          case ' ':
            RECORD_MARK(URL);
            RECORD_MARK(QUERY_STRING);
            state = s_req_http_start;
            break;
          case CR:
            RECORD_MARK(URL);
            RECORD_MARK(QUERY_STRING);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            RECORD_MARK(URL);
            RECORD_MARK(QUERY_STRING);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '#':
            RECORD_MARK(QUERY_STRING);
            state = s_req_fragment_start;
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_fragment_start:
      {
        if (normal_url_char[(unsigned char)ch]) {
          MARK(FRAGMENT);
          state = s_req_fragment;
          break;
        }

        switch (ch) {
          case ' ':
            RECORD_MARK(URL);
            state = s_req_http_start;
            break;
          case CR:
            RECORD_MARK(URL);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            RECORD_MARK(URL);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '?':
            RECORD_MARK(FRAGMENT);
            state = s_req_fragment;
            break;
          case '#':
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_fragment:
      {
        if (normal_url_char[(unsigned char)ch]) break;

        switch (ch) {
          case ' ':
            RECORD_MARK(URL);
            RECORD_MARK(FRAGMENT);
            state = s_req_http_start;
            break;
          case CR:
            RECORD_MARK(URL);
            RECORD_MARK(FRAGMENT);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            RECORD_MARK(URL);
            RECORD_MARK(FRAGMENT);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '?':
          case '#':
            break;
          default:
            goto error;
        }
        break;
      }

      case s_req_http_start:
        switch (ch) {
          case 'H':
            state = s_req_http_H;
            break;
          case ' ':
            break;
          default:
            goto error;
        }
        break;

      case s_req_http_H:
        STRICT_CHECK(ch != 'T');
        state = s_req_http_HT;
        break;

      case s_req_http_HT:
        STRICT_CHECK(ch != 'T');
        state = s_req_http_HTT;
        break;

      case s_req_http_HTT:
        STRICT_CHECK(ch != 'P');
        state = s_req_http_HTTP;
        break;

      case s_req_http_HTTP:
        STRICT_CHECK(ch != '/');
        state = s_req_first_http_major;
        break;

      /* first digit of major HTTP version */
      case s_req_first_http_major:
        if (ch < '1' || ch > '9') goto error;
        parser->http_major = ch - '0';
        state = s_req_http_major;
        break;

      /* major HTTP version or dot */
      case s_req_http_major:
      {
        if (ch == '.') {
          state = s_req_first_http_minor;
          break;
        }

        if (ch < '0' || ch > '9') goto error;

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) goto error;
        break;
      }

      /* first digit of minor HTTP version */
      case s_req_first_http_minor:
        if (ch < '0' || ch > '9') goto error;
        parser->http_minor = ch - '0';
        state = s_req_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_req_http_minor:
      {
        if (data_index && data[data_index-1].type == HTTP_VERSION) {
          /* only in the case of a second digit to http_minor */
          assert(0); // mostly should happen. REMOVEME
          assert(parser->http_minor > 10);
          data[data_index-1].payload.version.minor = parser->http_minor;
        } else {
          assert(data_index + 1 < data_len);
          data[data_index].type = HTTP_VERSION;
          data[data_index].payload.version.major = parser->http_major;
          data[data_index].payload.version.minor = parser->http_minor;
          data_index++;
        }

        if (ch == CR) {
          state = s_req_line_almost_done;
          break;
        }

        if (ch == LF) {
          state = s_header_field_start;
          break;
        }

        /* XXX allow spaces after digit? */

        if (ch < '0' || ch > '9') goto error;

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) goto error;
        break;
      }

      /* end of request line */
      case s_req_line_almost_done:
      {
        if (ch != LF) goto error;
        state = s_header_field_start;
        break;
      }

      case s_header_field_start:
      {
        if (ch == CR) {
          state = s_headers_almost_done;
          break;
        }

        if (ch == LF) {
          /* they might be just sending \n instead of \r\n so this would be
           * the second \n to denote the end of headers*/
          state = s_headers_almost_done;
          goto headers_almost_done;
        }

        c = LOWER(ch);

        if (c < 'a' || 'z' < c) goto error;

        MARK(HEADER_FIELD);

        index = 0;
        state = s_header_field;

        switch (c) {
          case 'c':
            header_state = h_C;
            break;

          case 'p':
            header_state = h_matching_proxy_connection;
            break;

          case 't':
            header_state = h_matching_transfer_encoding;
            break;

          case 'u':
            header_state = h_matching_upgrade;
            break;

          default:
            header_state = h_general;
            break;
        }
        break;
      }

      case s_header_field:
      {
        c = acceptable_header[(unsigned char)ch];

        if (c) {
          switch (header_state) {
            case h_general:
              break;

            case h_C:
              index++;
              header_state = (c == 'o' ? h_CO : h_general);
              break;

            case h_CO:
              index++;
              header_state = (c == 'n' ? h_CON : h_general);
              break;

            case h_CON:
              index++;
              switch (c) {
                case 'n':
                  header_state = h_matching_connection;
                  break;
                case 't':
                  header_state = h_matching_content_length;
                  break;
                default:
                  header_state = h_general;
                  break;
              }
              break;

            /* connection */

            case h_matching_connection:
              index++;
              if (index > sizeof(CONNECTION)-1
                  || c != CONNECTION[index]) {
                header_state = h_general;
              } else if (index == sizeof(CONNECTION)-2) {
                header_state = h_connection;
              }
              break;

            /* proxy-connection */

            case h_matching_proxy_connection:
              index++;
              if (index > sizeof(PROXY_CONNECTION)-1
                  || c != PROXY_CONNECTION[index]) {
                header_state = h_general;
              } else if (index == sizeof(PROXY_CONNECTION)-2) {
                header_state = h_connection;
              }
              break;

            /* content-length */

            case h_matching_content_length:
              index++;
              if (index > sizeof(CONTENT_LENGTH)-1
                  || c != CONTENT_LENGTH[index]) {
                header_state = h_general;
              } else if (index == sizeof(CONTENT_LENGTH)-2) {
                header_state = h_content_length;
              }
              break;

            /* transfer-encoding */

            case h_matching_transfer_encoding:
              index++;
              if (index > sizeof(TRANSFER_ENCODING)-1
                  || c != TRANSFER_ENCODING[index]) {
                header_state = h_general;
              } else if (index == sizeof(TRANSFER_ENCODING)-2) {
                header_state = h_transfer_encoding;
              }
              break;

            /* upgrade */

            case h_matching_upgrade:
              index++;
              if (index > sizeof(UPGRADE)-1
                  || c != UPGRADE[index]) {
                header_state = h_general;
              } else if (index == sizeof(UPGRADE)-2) {
                header_state = h_upgrade;
              }
              break;

            case h_connection:
            case h_content_length:
            case h_transfer_encoding:
            case h_upgrade:
              if (ch != ' ') header_state = h_general;
              break;

            default:
              assert(0 && "Unknown header_state");
              break;
          }
          break;
        }

        if (ch == ':') {
          RECORD_MARK(HEADER_FIELD);
          state = s_header_value_start;
          break;
        }

        if (ch == CR) {
          state = s_header_almost_done;
          RECORD_MARK(HEADER_FIELD);
          break;
        }

        if (ch == LF) {
          RECORD_MARK(HEADER_FIELD);
          state = s_header_field_start;
          break;
        }

        goto error;
      }

      case s_header_value_start:
      {
        if (ch == ' ') break;

        MARK(HEADER_VALUE);

        state = s_header_value;
        index = 0;

        c = acceptable_header[(unsigned char)ch];

        if (!c) {
          if (ch == CR) {
            RECORD_MARK(HEADER_VALUE);
            header_state = h_general;
            state = s_header_almost_done;
            break;
          }

          if (ch == LF) {
            RECORD_MARK(HEADER_VALUE);
            state = s_header_field_start;
            break;
          }

          header_state = h_general;
          break;
        }

        switch (header_state) {
          case h_upgrade:
            parser->flags |= F_UPGRADE;
            header_state = h_general;
            break;

          case h_transfer_encoding:
            /* looking for 'Transfer-Encoding: chunked' */
            if ('c' == c) {
              header_state = h_matching_transfer_encoding_chunked;
            } else {
              header_state = h_general;
            }
            break;

          case h_content_length:
            if (ch < '0' || ch > '9') goto error;
            parser->content_length = ch - '0';
            break;

          case h_connection:
            /* looking for 'Connection: keep-alive' */
            if (c == 'k') {
              header_state = h_matching_connection_keep_alive;
            /* looking for 'Connection: close' */
            } else if (c == 'c') {
              header_state = h_matching_connection_close;
            } else {
              header_state = h_general;
            }
            break;

          default:
            header_state = h_general;
            break;
        }
        break;
      }

      case s_header_value:
      {
        c = acceptable_header[(unsigned char)ch];

        if (!c) {
          if (ch == CR) {
            RECORD_MARK(HEADER_VALUE);
            state = s_header_almost_done;
            break;
          }

          if (ch == LF) {
            RECORD_MARK(HEADER_VALUE);
            goto header_almost_done;
          }
          break;
        }

        switch (header_state) {
          case h_general:
            break;

          case h_connection:
          case h_transfer_encoding:
            assert(0 && "Shouldn't get here.");
            break;

          case h_content_length:
            if (ch < '0' || ch > '9') goto error;
            parser->content_length *= 10;
            parser->content_length += ch - '0';
            break;

          /* Transfer-Encoding: chunked */
          case h_matching_transfer_encoding_chunked:
            index++;
            if (index > sizeof(CHUNKED)-1
                || c != CHUNKED[index]) {
              header_state = h_general;
            } else if (index == sizeof(CHUNKED)-2) {
              header_state = h_transfer_encoding_chunked;
            }
            break;

          /* looking for 'Connection: keep-alive' */
          case h_matching_connection_keep_alive:
            index++;
            if (index > sizeof(KEEP_ALIVE)-1
                || c != KEEP_ALIVE[index]) {
              header_state = h_general;
            } else if (index == sizeof(KEEP_ALIVE)-2) {
              header_state = h_connection_keep_alive;
            }
            break;

          /* looking for 'Connection: close' */
          case h_matching_connection_close:
            index++;
            if (index > sizeof(CLOSE)-1 || c != CLOSE[index]) {
              header_state = h_general;
            } else if (index == sizeof(CLOSE)-2) {
              header_state = h_connection_close;
            }
            break;

          case h_transfer_encoding_chunked:
          case h_connection_keep_alive:
          case h_connection_close:
            if (ch != ' ') header_state = h_general;
            break;

          default:
            state = s_header_value;
            header_state = h_general;
            break;
        }
        break;
      }

      case s_header_almost_done:
      header_almost_done:
      {
        STRICT_CHECK(ch != LF);

        state = s_header_field_start;

        switch (header_state) {
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
        break;
      }

      case s_headers_almost_done:
      headers_almost_done:
      {
        STRICT_CHECK(ch != LF);

        if (parser->flags & F_TRAILING) {
          /* End of a chunked request */
          RECORD(MESSAGE_END);
          state = NEW_MESSAGE();
          break;
        }

        nread = 0;

        if (parser->flags & F_UPGRADE || parser->method == HTTP_CONNECT) {
          parser->upgrade = 1;
        }


        // RECORD HEADERS_END
        assert(data_index + 1 < data_len);
        data[data_index].type = HTTP_HEADERS_END;
        data[data_index].payload.flags = parser->flags;
        data_index++;


        /* Exit, the rest of the connect is in a different protocol. */
        if (parser->upgrade) {
          RECORD(MESSAGE_END);
          goto exit;
        }

        state = s_decide_body;

        if (parser->type == HTTP_RESPONSE) {
          /* RESPONSE PARSING: We need to exit the function and get
           * information before knowing how to proceed. This could be a
           * response to a HEAD request. We'll do body_logic() next time we
           * enter http_parser_execute2().
           */
          RECORD(NEEDS_INPUT);
          goto exit;

        } else {
          /* REQUEST PARSING: Just make a decision about how to proceed on
           * the body...
           */
          state = body_logic(parser, p, data, data_len, &data_index);
        }

        break;
      }

      case s_decide_body:
        assert(0 && "Should not reach this state");
        break;

      case s_body_identity:
        to_read = MIN(pe - p, (int64_t)parser->content_length);
        if (to_read > 0) {
          RECORD_BODY(to_read);
          p += to_read - 1;

          parser->content_length -= to_read;

          if (parser->content_length == 0) {
            RECORD(MESSAGE_END);
            state = NEW_MESSAGE();
          }
        }
        break;

      /* read until EOF */
      case s_body_identity_eof:
        to_read = pe - p;
        if (to_read > 0) {
          RECORD_BODY(to_read);
          p += to_read - 1;
        }
        break;

      case s_chunk_size_start:
      {
        assert(parser->flags & F_CHUNKED);

        c = unhex[(unsigned char)ch];
        if (c == -1) goto error;
        parser->content_length = c;
        state = s_chunk_size;
        break;
      }

      case s_chunk_size:
      {
        assert(parser->flags & F_CHUNKED);

        if (ch == CR) {
          state = s_chunk_size_almost_done;
          break;
        }

        c = unhex[(unsigned char)ch];

        if (c == -1) {
          if (ch == ';' || ch == ' ') {
            state = s_chunk_parameters;
            break;
          }
          goto error;
        }

        parser->content_length *= 16;
        parser->content_length += c;
        break;
      }

      case s_chunk_parameters:
      {
        assert(parser->flags & F_CHUNKED);
        /* just ignore this shit. TODO check for overflow */
        if (ch == CR) {
          state = s_chunk_size_almost_done;
          break;
        }
        break;
      }

      case s_chunk_size_almost_done:
      {
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != LF);

        if (parser->content_length == 0) {
          parser->flags |= F_TRAILING;
          state = s_header_field_start;
        } else {
          state = s_chunk_data;
        }
        break;
      }

      case s_chunk_data:
      {
        assert(parser->flags & F_CHUNKED);

        to_read = MIN(pe - p, (int64_t)(parser->content_length));

        if (to_read > 0) {
          RECORD_BODY(to_read);
          p += to_read - 1;
        }

        if (to_read == parser->content_length) {
          state = s_chunk_data_almost_done;
        }

        parser->content_length -= to_read;
        break;
      }

      case s_chunk_data_almost_done:
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != CR);
        state = s_chunk_data_done;
        break;

      case s_chunk_data_done:
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != LF);
        state = s_chunk_size_start;
        break;

      default:
        assert(0 && "unhandled state");
        goto error;
    }
  }

  RECORD_MARK_NOCLEAR(HEADER_FIELD);
  RECORD_MARK_NOCLEAR(HEADER_VALUE);
  RECORD_MARK_NOCLEAR(FRAGMENT);
  RECORD_MARK_NOCLEAR(QUERY_STRING);
  RECORD_MARK_NOCLEAR(PATH);
  RECORD_MARK_NOCLEAR(URL);

exit:
  parser->state = state;
  parser->header_state = header_state;
  parser->index = index;
  parser->nread = nread;
  return data_index;

error:
  RECORD(PARSER_ERROR);
  parser->state = s_dead;
  return data_index;
}



#define CALLBACK(FOR)                                                \
do {                                                                 \
  if (settings->on_##FOR) {                                          \
    if (0 != settings->on_##FOR(&fake_parser,                        \
                                data[i].payload.string.p,            \
                                data[i].payload.string.len)) {       \
      return (data[i].payload.string.p - buf);                       \
    }                                                                \
  }                                                                  \
} while (0)


#define CALLBACK2(FOR)                                               \
do {                                                                 \
  if (settings->on_##FOR) {                                          \
    if (0 != settings->on_##FOR(&fake_parser))                       \
      return (data[i].payload.string.p - buf);                       \
  }                                                                  \
} while (0)


size_t http_parser_execute (http_parser *parser,
                            const http_parser_settings *settings,
                            const char *buf,
                            size_t buf_len)
{
# define DATA_SIZE 50
  http_parser_data data[DATA_SIZE];
  int i, ndata;
  size_t read = 0;

  /* Fake the parser object being passed to the callbacks */
  http_parser fake_parser;
  fake_parser.data = parser->data;
  fake_parser.type = parser->type;

  while (1) {
    ndata = http_parser_execute2(parser,
                                 buf + read,
                                 buf_len - read,
                                 data,
                                 DATA_SIZE);

    for (i = 0; i < ndata; i++) {
      switch (data[i].type) {
        case HTTP_PARSER_ERROR:
          // HTTP_PARSER_ERROR should always be the last element of 'data'
          assert(ndata - 1 == i);
          return data[i].payload.string.p - buf;

        case HTTP_NEEDS_INPUT:
          // HTTP_NEEDS_INPUT should always be the last element of 'data'
          assert(ndata - 1 == i);
          // Ignore.. we handled this in HTTP_HEADERS_END
          break;

        case HTTP_NEEDS_DATA_ELEMENTS:
          // HTTP_NEEDS_DATA_ELEMENTS is the last element of 'data'
          assert(ndata - 1 == i);
          // Go around the while loop again.
          break;

        case HTTP_REQ_MESSAGE_START:
          fake_parser.method = data[i].payload.method;
          CALLBACK2(message_begin);
          break;

        case HTTP_RES_MESSAGE_START:
          fake_parser.status_code = data[i].payload.status_code;
          CALLBACK2(message_begin);
          break;

        case HTTP_VERSION:
          fake_parser.http_major = data[i].payload.version.major;
          fake_parser.http_minor = data[i].payload.version.minor;
          break;

        case HTTP_PATH:
          CALLBACK(path);
          break;

        case HTTP_QUERY_STRING:
          CALLBACK(query_string);
          break;

        case HTTP_URL:
          CALLBACK(url);
          break;

        case HTTP_FRAGMENT:
          CALLBACK(fragment);
          break;

        case HTTP_HEADER_FIELD:
          CALLBACK(header_field);
          break;

        case HTTP_HEADER_VALUE:
          CALLBACK(header_value);
          break;

        case HTTP_HEADERS_END:
          fake_parser.flags = data[i].payload.flags;
          if (settings->on_headers_complete) {
            switch (settings->on_headers_complete(&fake_parser)) {
              case 0:
                http_parser_has_body(&fake_parser, 1);
                break;

              case 1:
                http_parser_has_body(&fake_parser, 0);
                break;

              default:
                return data[i].payload.string.p - buf; /* Error */
            }
          }
          break;

        case HTTP_BODY:
          CALLBACK(body);
          break;

        case HTTP_MESSAGE_END:
          CALLBACK2(message_complete);
          break;
       }
    }

    /* If the last data element is NEEDS_INPUT or NEEDS_DATA_ELEMENTS
     * Go round the loop again. (Note to self: This API isn't very nice...)
     */
    if (ndata > 0 && (data[ndata - 1].type == HTTP_NEEDS_INPUT ||
        data[ndata - 1].type == HTTP_NEEDS_DATA_ELEMENTS)) {
      /* We've parsed only as far as the data point */
      read += data[ndata - 1].payload.string.p - (buf+read);

    } else {
      /* We've parsed the whole thing that was passed in. */
      read += buf_len - read;
      if (read >= buf_len) break;
    }
  }
  return read;
}


void http_parser_has_body (http_parser *parser, int has)
{
  if (!has)  {
    parser->flags |= F_SKIPBODY;
  }
}


int
http_should_keep_alive (http_parser *parser)
{
  if (parser->http_major > 0 && parser->http_minor > 0) {
    /* HTTP/1.1 */
    if (parser->flags & F_CONNECTION_CLOSE) {
      return 0;
    } else {
      return 1;
    }
  } else {
    /* HTTP/1.0 or earlier */
    if (parser->flags & F_CONNECTION_KEEP_ALIVE) {
      return 1;
    } else {
      return 0;
    }
  }
}


const char * http_method_str (enum http_method m)
{
  return method_strings[m];
}


void
http_parser_init (http_parser *parser, enum http_parser_type t)
{
  parser->type = t;
  parser->state = (t == HTTP_REQUEST ? s_start_req : (t == HTTP_RESPONSE ? s_start_res : s_start_req_or_res));
  parser->nread = 0;
  parser->upgrade = 0;
  parser->flags = 0;
  parser->method = 0;
}
